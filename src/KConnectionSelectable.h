#ifndef KCONNECTIONSELECTABLE_H
#define KCONNECTIONSELECTABLE_H
#include "KSelectable.h"
#include "KVirtualHostContainer.h"
class KHttpSpdy;
class KHttpRequest;
class KServer;
class KSubVirtualHost;
#ifdef KSOCKET_SSL
class KSSLSniContext
{
public:
	KSSLSniContext()
	{
		memset(this,0,sizeof(*this));
	}
	~KSSLSniContext();
	query_vh_result result;
	KSubVirtualHost *svh;
};
#endif
class KConnectionSelectable : public KSelectable
{
public:
	KConnectionSelectable()
	{
		socket = NULL;
		/////////[35]
		ls = NULL;
#ifdef KSOCKET_SSL
		sni = NULL;
#endif
	}
	void destroy();
#ifdef KSOCKET_SSL
	void resultSSLShutdown(int got);
	query_vh_result useSniVirtualHost(KHttpRequest *rq);
#endif
	/* 同步读 */
	int read(KHttpRequest *rq,char *buf,int len);
	/* 同步写 */
	int write(KHttpRequest *rq,LPWSABUF buf,int bufCount);
	bool write_all(KHttpRequest *rq, const char *buf, int len);
	/* 延时异步读 */
	void delayRead(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);
	/* 延时异步写 */
	void delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);
	/* 异步读 */
	void read(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int list=KGL_LIST_RW);
	/* 异步写 */
	void write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	void removeSocket();

	void next(KHttpRequest *rq,resultEvent result);	
	void removeRequest(KHttpRequest *rq);
	void startResponse(KHttpRequest *rq);
	void endResponse(KHttpRequest *rq,bool keep_alive);
	

	KSocket *getSocket()
	{
		return socket;
	}
	void release(KHttpRequest *rq);
	/*
	 * server是原始socket,
	 * 一般情况是一样的，如果在ssi里面内部调用时就不一样了。
	 */
	KClientSocket *socket;
	/////////[36]
	//请求所在侦听
	KServer *ls;
#ifdef KSOCKET_SSL
	KSSLSniContext *sni;
#endif

protected:
	virtual ~KConnectionSelectable();
private:
	void ssl_destroy();
};

#endif
