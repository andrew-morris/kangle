#include "KSimulateRequest.h"
#include "KHttpRequest.h"
#include "http.h"
#include "KSelectorManager.h"
#include "KHttpProxyFetchObject.h"
#ifdef ENABLE_SIMULATE_HTTP
int asyncHttpRequest(kgl_async_http *ctx)
{
	if (ctx->postLen>0 && ctx->post==NULL) {
		return 1;
	}
	KConnectionSelectable *c = new KConnectionSelectable;
	KHttpRequest *rq = new KHttpRequest(c);
	//c->rq = rq;
	if (ctx->selector==0) {
		rq->c->selector = selectorManager.getSelector();
	} else {
		rq->c->selector = selectorManager.getSelectorByIndex(ctx->selector - 1);
	}
	if (!parse_url(ctx->url,&rq->raw_url)) {
		rq->raw_url.destroy();
		KStringBuf nu;
		nu << ctx->url << "/";
		if (!parse_url(nu.getString(), &rq->raw_url)) {
			delete rq;
			return 1;
		}
	}
	if (rq->raw_url.host==NULL) {
		delete rq;
		return 1;
	}
	KSimulateSocket *ss = new KSimulateSocket;
	if (ctx->host) {
		ss->host = strdup(ctx->host);
	}
	ss->port = ctx->port;
	ss->life_time = ctx->life_time;
	ss->arg = ctx->arg;
	ss->post = ctx->post;
	ss->header = ctx->header;
	ss->body = ctx->body;
	ss->rh = ctx->rh;
	rq->c->socket = ss;
	rq->workModel = WORK_MODEL_SIMULATE;
	rq->init();
	rq->meth = KHttpKeyValue::getMethod(ctx->meth);
	rq->content_length = ctx->postLen;
	rq->http_major = 1;
	rq->http_minor = 1;
	SET(rq->flags,RQ_CONNECTION_CLOSE|RQ_HAS_NO_CACHE);
	rq->beginRequest();
	rq->active_msec = kgl_current_msec;
	ss->startTime = kgl_current_msec;
	SET(rq->filter_flags,RF_NO_CACHE);
#ifndef _WIN32
#ifndef NDEBUG
	ss->setnoblock();
#endif
#endif
	rq->fetchObj = new KHttpProxyFetchObject();
	async_http_start(rq);
	//printf("call async_http_start\n");
	return 0;
}
int WINAPI test_header_hook(void *arg,int code,KHttpHeader *header)
{
	return 0;
}
int WINAPI test_body_hook(void *arg,const char *data,int len)
{
	printf("len = %d\n",len);
	return 0;
}
int WINAPI test_post_hook(void *arg,char *buf,int len)
{
	memcpy(buf,"test",4);
	return 4;
}
bool test_simulate_request()
{
	kgl_async_http ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.url = "http://www.kanglesoft.com";
	ctx.meth = "post";
	ctx.postLen = 4;
	ctx.header = test_header_hook;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = test_post_hook;
	asyncHttpRequest(&ctx);
	//asyncHttpRequest(METH_GET,"http://www.kanglesoft.com/test.html",NULL,test_header_hook,test_body_hook,NULL);
	return true;
}
int KSimulateSocket::sendError(int code,const char *msg)
{
	if (header) {
		KHttpHeader head;
		head.next = NULL;
		head.attr = (char *)"msg";
		head.val = (char *)msg;
		header(arg,code+10000,&head);
	}
	return 0;
}
KSimulateSocket::KSimulateSocket()
{
	host = NULL;
	rh = NULL;
}
KSimulateSocket::~KSimulateSocket()
{
	if (host) {
		xfree(host);
	}
	if (body) {
		body(arg,NULL,(int)(kgl_current_msec - startTime));
	}
}
int KSimulateSocket::sendHeader(int code,KHttpHeader *header)
{
	if (this->header) {
		return this->header(arg,code,header);
	}
	return 0;
}
int KSimulateSocket::sendBody(const char *buf,int len)
{
	if (this->body) {
		return this->body(arg,buf,len);
	}
	return 0;
}
#endif

