#include "KConnectionSelectable.h"
#include "KSelector.h"
/////////[37]
#include "KServer.h"
#include "KSubVirtualHost.h"
#include "KSelectorManager.h"
struct kgl_delay_io
{
	KConnectionSelectable *c;
	KHttpRequest *rq;
	bufferEvent buffer;
	resultEvent result;
};
void WINAPI delay_read(void *arg)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	io->c->read(io->rq,io->result,io->buffer);
	delete io;
}
void WINAPI delay_write(void *arg)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	io->c->write(io->rq,io->result,io->buffer);
	delete io;
}
#ifdef KSOCKET_SSL
void resultSSLShutdown(void *arg,int got)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	c->resultSSLShutdown(got);
}
#endif
KConnectionSelectable::~KConnectionSelectable()
{
	if (socket) {
		if (selector) {
			/**
			* 这里为什么要调用一次removeSocket呢？
			* 一般来讲，在linux下面,一个socket close了。epoll自动会删除。
			* 但如果存在多进程的时候，此socket在一瞬间被其它进程共享，
			* 则close此socket,epoll还是会返回相关事件。
			*/
			selector->removeSocket(this);
		}
		delRequest(st_flags,socket);
		socket->shutdown(SHUT_RDWR);
		delete socket;
	}
	/////////[38]
	if (ls) {
		ls->release();
	}
#ifdef KSOCKET_SSL
	if (sni) {
		delete sni;
	}
#endif
}
void KConnectionSelectable::release(KHttpRequest *rq)
{
	/////////[39]
	destroy();
}
void KConnectionSelectable::read(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int list)
{
	/////////[40]
	rq->c->selector->addList(rq,list);
	asyncRead(rq,result,buffer);
}
bool KConnectionSelectable::write_all(KHttpRequest *rq, const char *buf, int len)
{
	WSABUF b;
	while (len > 0) {
		b.iov_len = len;
		b.iov_base = (char *)buf;
		int got = write(rq, &b, 1);
		if (got <= 0) {
			return false;
		}
		len -= got;
		buf += got;
	}
	return true;
}
int KConnectionSelectable::write(KHttpRequest *rq,LPWSABUF buf,int bufCount)
{
	/////////[41]
	return socket->writev(buf,bufCount,isSSL());
}
void KConnectionSelectable::write(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	rq->c->selector->addList(rq,KGL_LIST_RW);
	/////////[42]
	asyncWrite(rq,result,buffer);
}
void KConnectionSelectable::next(KHttpRequest *rq,resultEvent result)
{
	if (!selector->next(this,result,rq)) {
		result(rq,-1);
	}
}
void KConnectionSelectable::removeSocket()
{
        /////////[43]
	selector->removeSocket(this);
}
void KConnectionSelectable::removeRequest(KHttpRequest *rq)
{
	selector->removeSocket(this);
	selector->addList(rq,KGL_LIST_SYNC);
}
int KConnectionSelectable::read(KHttpRequest *rq,char *buf,int len)
{
	/////////[44]
	return socket->read(buf,len);
}
void KConnectionSelectable::delayRead(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec)
{
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->removeSocket(this);
	selector->addTimer(rq,delay_read,io,msec);
}
void KConnectionSelectable::delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec)
{
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->removeSocket(this);
	selector->addTimer(rq,delay_write,io,msec);
}
void KConnectionSelectable::startResponse(KHttpRequest *rq)
{
	/////////[45]
	socket->setdelay();
	return;
}
void KConnectionSelectable::endResponse(KHttpRequest *rq,bool keep_alive)
{
	/////////[46]
	if (keep_alive) {
		socket->setnodelay();
		SET(rq->workModel,WORK_MODEL_KA);
	}
}
#ifdef KSOCKET_SSL
query_vh_result KConnectionSelectable::useSniVirtualHost(KHttpRequest *rq)
{
	assert(rq->svh==NULL);
	rq->svh = sni->svh;
	sni->svh = NULL;
	query_vh_result ret = sni->result;
	delete sni;
	sni = NULL;
	return ret;
}
void KConnectionSelectable::resultSSLShutdown(int got)
{
	if (got<0) {
		delete this;
		return;
	}
	assert(socket);
	KSSLSocket *sslSocket = static_cast<KSSLSocket *>(socket);
	SSL *ssl = sslSocket->getSSL();
	assert(ssl);
	int n = SSL_shutdown(ssl);
	if (n==1) {
		delete this;
		return;
	}
	int err = SSL_get_error(ssl,n);
	switch (err) {
	case SSL_ERROR_WANT_READ:
		if (!selector->read(this, ::resultSSLShutdown, NULL, this)) {
			delete this;
		}
		return;
	case SSL_ERROR_WANT_WRITE:
		if (!selector->write(this, ::resultSSLShutdown, NULL, this)) {
			delete this;
		}
		return;
	default:
		delete this;
		return;
	}
}
#endif
void KConnectionSelectable::ssl_destroy()
{
#ifdef KSOCKET_SSL
	if (socket && isSSL() && selector) {
			resultSSLShutdown(0);
			return;
	}
#endif
	delete this;
}
void KConnectionSelectable::destroy()
{
	/////////[47]
	ssl_destroy();
}
#ifdef KSOCKET_SSL
KSSLSniContext::~KSSLSniContext()
{
	if (svh) {
		svh->release();
	}
}
#endif
