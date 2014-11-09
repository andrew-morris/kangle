#include "KSSLSocket.h"
#include "KHttpRequest.h"
#include "KSelector.h"
#include "ssl_utils.h"
#ifdef KSOCKET_SSL
static KMutex *ssl_lock = NULL;
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#include "KVirtualHostManage.h"
//sni lookup server name
int httpSSLServerName(SSL *ssl,int *ad,void *arg)
{
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (servername==NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	KHttpRequest *rq = (KHttpRequest *)SSL_get_ex_data(ssl,kangle_ssl_conntion_index);
	if (rq==NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	//manage request skip SNI
	if (TEST(rq->workModel,WORK_MODEL_MANAGE)) {
		return SSL_TLSEXT_ERR_OK;
	}
	if (rq->svh==NULL && query_vh_success != conf.gvm->queryVirtualHost(rq,servername)) {
		return SSL_TLSEXT_ERR_OK;
	}
	if (rq->svh->vh && rq->svh->vh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl,rq->svh->vh->ssl_ctx);
	}
	return SSL_TLSEXT_ERR_OK;
}
#endif
//ssl关闭要调用SSL_shutdown
void stageSSLShutdown(KHttpRequest *rq)
{
	KSSLSocket *ssl_socket = static_cast<KSSLSocket *>(rq->server);
	SSL *ssl = ssl_socket->getSSL();
	if (ssl) {
		SSL_shutdown(ssl);
		//TODO:这里要处理错误SSL_ERROR_WANT_READ,还是SSL_ERROR_WANT_WRITE
	}
	delete rq;
}
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
void handleSSLAccept(KSelectable *st,int got)
{

	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	assert(TEST(rq->workModel,WORK_MODEL_SSL));
	if (got<0) {
		delete rq;
		return;
	}
	KSSLSocket *socket = static_cast<KSSLSocket *>(rq->server);
	switch(socket->ssl_accept()){
	case ret_ok:
		{
			rq->init();
			SET(rq->raw_url.proto,PROTO_HTTPS);
			rq->handler = handleRequestRead;
			handleRequestRead(rq,0);
			return;
		}
	case ret_want_read:
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_READ);
		return;
	case ret_want_write:
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE);
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
	//SSL_set_session_id_context(ssl, (const unsigned char *) PROGRAM_NAME,strlen(PROGRAM_NAME));
	if (SSL_set_fd(ssl, sockfd)!=1) {
		return false;
	}
	SSL_set_accept_state(ssl);
	return true;
}
ssl_status KSSLSocket::ssl_accept() {
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
	int err;
	assert(ssl==NULL);
	if ((ssl = SSL_new(ctx)) == NULL) {
		err = ERR_get_error();
		fprintf(stderr, "SSL: Error allocating handle: %s\n", ERR_error_string(
				err, NULL));
		return false;
	}
	SSL_set_fd(ssl, sockfd);
	SSL_set_connect_state(ssl);
	if (SSL_connect(ssl) <= 0) {
		err = ERR_get_error();
		fprintf(stderr, "SSL: Error conencting socket: %s\n", ERR_error_string(
				err, NULL));
		return false;
	}
	fprintf(stderr, "SSL: negotiated cipher: %s\n", SSL_get_cipher(ssl));
	return true;
}

#endif
