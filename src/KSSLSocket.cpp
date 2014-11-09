#include "KSSLSocket.h"
#include "KHttpRequest.h"
#include "KSelector.h"
#include "ssl_utils.h"
/////////[373]
#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
#ifdef KSOCKET_SSL
static KMutex *ssl_lock = NULL;
int kangle_ssl_conntion_index;
int kangle_ssl_ctx_index;
typedef struct {
	kgl_str_t                 name;
	int                       mask;
} kgl_string_bitmask_t;
#define KGL_SSL_SSLv2    0x0002
#define KGL_SSL_SSLv3    0x0004
#define KGL_SSL_TLSv1    0x0008
#define KGL_SSL_TLSv1_1  0x0010
#define KGL_SSL_TLSv1_2  0x0020
static kgl_string_bitmask_t  kgl_ssl_protocols[] = {
	{ kgl_string("SSLv2"), KGL_SSL_SSLv2 },
	{ kgl_string("SSLv3"), KGL_SSL_SSLv3 },
	{ kgl_string("TLSv1"), KGL_SSL_TLSv1 },
	{ kgl_string("TLSv1.1"), KGL_SSL_TLSv1_1 },
	{ kgl_string("TLSv1.2"), KGL_SSL_TLSv1_2 },
	{ kgl_null_string, 0 }
};
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#include "KVirtualHostManage.h"
//sni lookup server name
int httpSSLServerName(SSL *ssl,int *ad,void *arg)
{
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (servername==NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	KConnectionSelectable *c = (KConnectionSelectable *)SSL_get_ex_data(ssl,kangle_ssl_conntion_index);
	if (c==NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	if (c->sni) {
		return SSL_TLSEXT_ERR_OK;
	}
	c->sni = new KSSLSniContext;
	if (query_vh_success != conf.gvm->queryVirtualHost(c->ls,&c->sni->svh,servername)) {
		return SSL_TLSEXT_ERR_OK;
	}
	if (c->sni->svh->vh && c->sni->svh->vh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl,c->sni->svh->vh->ssl_ctx);
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif
/////////[374]
static unsigned long __get_thread_id (void)
{
	return (unsigned long) pthread_self();
}
static void __lock_thread (int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		ssl_lock[n].Lock();
	} else {
		ssl_lock[n].Unlock();
	}
}
void init_ssl()
{
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	load_ssl_library();
	if ((CRYPTO_get_id_callback()      == NULL) &&
	    (CRYPTO_get_locking_callback() == NULL))
	{
		//cuint_t n;

		CRYPTO_set_id_callback (__get_thread_id);
		CRYPTO_set_locking_callback (__lock_thread);

		int locks_num = CRYPTO_num_locks();
		ssl_lock = new KMutex[locks_num];	
	}
}
void resultSSLAccept(void *arg,int got)
{

	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(TEST(rq->workModel,WORK_MODEL_SSL));
	if (got<0) {
		delete rq;
		return;
	}
	KSSLSocket *socket = static_cast<KSSLSocket *>(rq->c->socket);
	switch(socket->handshake()){
	case ret_ok:
		{
/////////[375]
			rq->init();
			SET(rq->raw_url.flags,KGL_URL_SSL);
			rq->c->read(rq,resultRequestRead,bufferRequestRead);
			return;
		}
	case ret_want_read:
		rq->c->read(rq,resultSSLAccept,NULL);
		return;
	case ret_want_write:
		rq->c->write(rq,resultSSLAccept,NULL);
		return;
	default:
		delete rq;
		return;
	}
}
KSSLSocket::KSSLSocket(SSL_CTX *ctx) {
	ssl = NULL;
	this->ctx = ctx;
}
KSSLSocket::~KSSLSocket() {
	if (ssl) {
		SSL_free(ssl);
	}
}
void KSSLSocket::get_next_proto_negotiated(const unsigned char **data,unsigned *len)
{
#ifdef TLSEXT_TYPE_next_proto_neg
	SSL_get0_next_proto_negotiated(ssl,data,len);
#endif
}
SSL_CTX * KSSLSocket::init_server(const char *cert_file, const char *key_file,
		const char *verified_file) {
	SSL_CTX * ctx = init_ctx(true);
	if (ctx == NULL) {
		fprintf(stderr, "cann't init_ctx\n");
		return NULL;
	}
	if (cert_file == NULL) {
		cert_file = key_file;
	}
	if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
		fprintf(stderr,
				"SSL use certificate file : Error allocating handle: %s\n",
				ERR_error_string(ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
		fprintf(stderr,
				"SSL use privatekey file: Error allocating handle: %s\n",
				ERR_error_string(ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}

	if (!SSL_CTX_check_private_key(ctx)) {
		fprintf(stderr, "SSL: Error allocating handle: %s\n", ERR_error_string(
				ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}
	if (verified_file) {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER
				| SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
		SSL_CTX_set_verify_depth(ctx, 1);
		if (SSL_CTX_load_verify_locations(ctx, verified_file, NULL) <= 0) {
			fprintf(stderr, "SSL error %s:%d: Error allocating handle: %s\n",
					__FILE__, __LINE__, ERR_error_string(ERR_get_error(), NULL));
			clean_ctx(ctx);
			return NULL;
		}
	}
	int session_context_len = strlen(cert_file);
	const char *session_context = cert_file;
	int pos = session_context_len - SSL_MAX_SSL_SESSION_ID_LENGTH;
	if (pos>0) {
		session_context_len -= pos;
		session_context += pos;
	}
	SSL_CTX_set_session_id_context(ctx,(const unsigned char *)session_context,session_context_len);
	SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_SERVER);
	//SSL_CTX_sess_set_cache_size(ctx,1000);
	return ctx;
}
bool KSSLSocket::verifiedSSL() {
	if (SSL_get_verify_result(ssl) != X509_V_OK) {
		return false;
	}
	return true;
}
SSL_CTX * KSSLSocket::init_ctx(bool server) {
	SSL_CTX *ctx;
	if (server) {
		ctx = SSL_CTX_new(SSLv23_server_method());
	} else {
		ctx = SSL_CTX_new(SSLv23_client_method());
	}
	if (ctx == NULL) {
		fprintf(stderr, "ssl_ctx_new function error\n");
		return NULL;
	}
	  /* client side options */

    SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_SESS_ID_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_NETSCAPE_CHALLENGE_BUG);

    /* server side options */

    SSL_CTX_set_options(ctx, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);

#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    /* this option allow a potential SSL 2.0 rollback (CAN-2005-2969) */
    SSL_CTX_set_options(ctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif

    SSL_CTX_set_options(ctx, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_TLS_D5_BUG);
    SSL_CTX_set_options(ctx, SSL_OP_TLS_BLOCK_PADDING_BUG);

    SSL_CTX_set_options(ctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);

    SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#endif

#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
	SSL_CTX_set_mode(ctx,SSL_MODE_ENABLE_PARTIAL_WRITE);

	SSL_CTX_set_options(ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
	return ctx;
}
SSL_CTX * KSSLSocket::init_client(const char *path, const char *file) {
	SSL_CTX *ctx = init_ctx(false);
	if (ctx) {
		if (file != NULL) {
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
			SSL_CTX_set_verify_depth(ctx, 1);

			if (SSL_CTX_load_verify_locations(ctx, file, path) <= 0) {
				fprintf(stderr, "SSL error %s:%d: Error allocating handle: %s\n",
						__FILE__, __LINE__, ERR_error_string(ERR_get_error(), NULL));
				clean_ctx(ctx);
				return NULL;
			}
		}
		SSL_CTX_set_session_id_context(ctx,(const unsigned char *)PROGRAM_NAME,sizeof(PROGRAM_NAME)-1);
		SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_BOTH);
	}
	return ctx;
}
void KSSLSocket::clean_ctx(SSL_CTX *ctx) {
	SSL_CTX_free(ctx);
}
void KSSLSocket::close() {
	if (ssl) {
		SSL_free(ssl);
	}
	ssl = NULL;
	KSocket::close();
}
int KSSLSocket::get_ssl_error(int re)
{
	return SSL_get_error(ssl,re);
}
int KSSLSocket::write(const char *str, int len) {
	return SSL_write(ssl, str, len);
}
int KSSLSocket::read(char *str, int len) {
	return SSL_read(ssl, str, len);
}
bool KSSLSocket::bind_fd()
{
	assert(ssl==NULL);
	ssl = SSL_new(ctx);
	if (ssl==NULL) {
		return false;
	}
	if (SSL_set_fd(ssl, sockfd)!=1) {
		return false;
	}
	SSL_set_accept_state(ssl);
	return true;
}
void KSSLSocket::set_ssl_protocols(SSL_CTX *ctx, const char *protocols)
{	
	char *buf = strdup(protocols);
	char *hot = buf;
	int mask = 0;
	for (;;) {
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		char *p = hot;
		while (*p && !isspace((unsigned char)*p)) {
			p++;
		}		
		if (p == hot) {
			break;
		}	
		if (*p) {		
			*p = '\0';
			p++;
		}
		kgl_string_bitmask_t *h = kgl_ssl_protocols;
		while (h->name.data) {
			if (strcasecmp(h->name.data, hot) == 0) {
				SET(mask, h->mask);
			}
			h++;
		}
		if (*p == '\0') {
			break;
		}
		hot = p;
	}
	xfree(buf);
	if (mask > 0) {
		if (!(mask & KGL_SSL_SSLv2)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
		}
		if (!(mask & KGL_SSL_SSLv3)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
		}
		if (!(mask & KGL_SSL_TLSv1)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
		}
#ifdef SSL_OP_NO_TLSv1_1
		if (!(mask & KGL_SSL_TLSv1_1)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
		}
#endif
#ifdef SSL_OP_NO_TLSv1_2
		if (!(mask & KGL_SSL_TLSv1_2)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2);
		}
#endif
	}	
}
ssl_status KSSLSocket::handshake() {
	int err;
	assert(ssl);
	int re = SSL_do_handshake(ssl);
	if (re<=0) {
		err = SSL_get_error(ssl,re);
		switch (err) {
		case SSL_ERROR_WANT_READ:
			return ret_want_read;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			return ret_want_write;
#ifndef _WIN32
		case SSL_ERROR_SYSCALL:
			if (errno == EAGAIN) {
				//return ret_error;
			}
			return ret_error;
#endif
		case SSL_ERROR_SSL:
		case SSL_ERROR_ZERO_RETURN:
			//printf("error = %d\n",err);
			return ret_error;
		default:
			//printf("error = %d\n",err);
			return ret_error;
		}
	}
	if (ssl->s3) {
            ssl->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
	}
	return ret_ok;
}
bool KSSLSocket::ssl_connect() {
	assert(ssl==NULL);
	assert(ctx);
	if ((ssl = SSL_new(ctx)) == NULL) {
		int err = ERR_get_error();
		fprintf(stderr, "SSL: Error allocating handle: %s\n", ERR_error_string(
				err, NULL));
		return false;
	}
	if (SSL_set_fd(ssl, sockfd)!=1) {
		return false;
	}
	SSL_set_connect_state(ssl);
	return true;
}
bool KSSLSocket::setHostName(const char *hostname)
{
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	SSL_set_tlsext_host_name(ssl,hostname);
	return true;
#else
	return false;
#endif
}
#endif
