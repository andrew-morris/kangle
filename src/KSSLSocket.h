#ifndef KSSLSOCKET_H
#define KSSLSOCKET_H
#include "KSocket.h"
#include "KSelectable.h"
#ifdef KSOCKET_SSL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/ocsp.h>
enum ssl_status
{
	ret_error,
	ret_ok,
	ret_want_read,
	ret_want_write
};
class KSSLSocket: public KClientSocket {
public:
	KSSLSocket(SSL_CTX *ctx);
	~KSSLSocket();
	void close();
	bool bind_fd();
	//异步accept,返回-1，错误，0=成功，1=want read,2=want write
	ssl_status handshake();
	int read(char *buf,int len);
	int write(const char *buf,int len);
	int get_ssl_error(int re);
	bool ssl_connect();
	SSL *getSSL() {
		return ssl;
	}
	void get_next_proto_negotiated(const unsigned char **data,unsigned *len);
	bool verifiedSSL();
	static SSL_CTX * init_server(const char *cert_file, const char *key_file,
			const char *verified_file);
	static void set_ssl_protocols(SSL_CTX *ctx, const char *protocols);
	static SSL_CTX * init_client(const char *path, const char *file);
	static void clean_ctx(SSL_CTX *ctx);
	bool setHostName(const char *hostname);

private:
	static SSL_CTX * init_ctx(bool server);
	SSL *ssl;
	SSL_CTX *ctx;
};
void resultSSLAccept(void *arg,int got);
class KHttpRequest;
int httpSSLServerName(SSL *ssl,int *ad,void *arg);
int httpSSLNpnAdvertised(SSL *ssl_conn,const unsigned char **out, unsigned int *outlen, void *arg);
int httpSSLNpnSelected(SSL *ssl,unsigned char **out,unsigned char *outlen,const unsigned char *in,unsigned int inlen,void *arg);
void init_ssl();
extern int kangle_ssl_conntion_index;
extern int kangle_ssl_ctx_index;
#endif
#endif
