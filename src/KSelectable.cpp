#include "KSelectable.h"
#include "KSelector.h"
#include "KSSLSocket.h"
KSelectable::~KSelectable()
{
#ifndef NDEBUG
#ifdef _WIN32
	assert(TEST(st_flags,STF_READ|STF_WRITE)==0);
#endif
	klog(KLOG_DEBUG,"delete st=%p,at %p\n",this,pthread_self());
	selector = NULL;
#endif
}
void KSelectable::eventRead(void *arg,resultEvent result,bufferEvent buffer)
{
	if (buffer==NULL) {
		result(arg,0);
		return;
	}
	WSABUF recvBuf[1];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = 1;
	KClientSocket *server = static_cast<KClientSocket *>(getSocket());
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->read((char *)recvBuf[0].iov_base,recvBuf[0].iov_len);
	if (got>=0) {
		result(arg,got);
		return;
	}
	if (errno==EAGAIN) {
		if (!selector->read(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		int err = sslsocket->get_ssl_error(got);
		if (err==SSL_ERROR_WANT_READ) {
			if (!selector->read(this,result,buffer,arg)) {
				result(arg,-1);
			}
			return;
		}
	}
#endif
	result(arg,-1);
}
void KSelectable::eventWrite(void *arg,resultEvent result,bufferEvent buffer)
{
#define MAXSENDBUF 16
	if (unlikely(buffer==NULL)) {
		result(arg,0);
		return;
	}
	WSABUF recvBuf[MAXSENDBUF];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = MAXSENDBUF;
	KClientSocket *server = static_cast<KClientSocket *>(getSocket());
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->writev(recvBuf,bufferCount,isSSL());
	if (got>=0) {
		result(arg,got);
		return;
	}
	/*
	if (errno==EINTR) {
		continue;
	}
	*/
	if (errno==EAGAIN) {
		if (!selector->write(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_WRITE) {
			if (!selector->write(this,result,buffer,arg)) {
				result(arg,-1);
			}
			return;
		}
	}
#endif
	result(arg,-1);
}
