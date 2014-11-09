#ifndef KSSLSOCKET_H
#define KSSLSOCKET_H
#include "KSocket.h"
#include "KSelectable.h"
#ifdef KSOCKET_SSL
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
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
	ssl_status ssl_accept();
	int read(char *buf,int len);
	int write(const char *buf,int len);
	int get_ssl_error(int re);
	bool ssl_connect();
	SSL *getSSL() {
		return ssl;
	}
	bool verifiedSSL();
	static SSL_CTX * init_server(const char *cert_file, const char *key_file,
			const char *verified_file);
	static SSL_CTX * init_client(const char *path, const char *file);
	static void clean_ctx(SSL_CTX *ctx);
	//handleEvent uphandler;

private:
	static SSL_CTX * init_ctx(bool server);
	SSL *ssl;
	SSL_CTX *ctx;
};
void handleSSLAccept(KSelectable *st,int got);
class KHttpRequest;
void stageSSLShutdown(KHttpRequest *rq);
int httpSSLServerName(SSL *ssl,int *ad,void *arg);
void init_ssl();
extern int kangle_ssl_conntion_index;
#endif
#endif
