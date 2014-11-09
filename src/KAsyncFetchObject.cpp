#include "KAsyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
#include "KAsyncWorker.h"
#ifdef _WIN32
#include "KIOCPSelector.h"
#endif
#include "KSingleAcserver.h"
#include "KCdnContainer.h"
/////////[323]

static KTHREAD_FUNCTION asyncDnsCallBack(void *data)
{
	KHttpRequest *rq = (KHttpRequest *)data;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (kgl_current_msec - rq->active_msec > conf.time_out * 1000) {
		fo->connectCallBack(rq,NULL);
		KTHREAD_RETURN;
	}
	const char *ip = rq->bind_ip;
	KUrl *url = (TEST(rq->filter_flags,RF_PROXY_RAW_URL)?&rq->raw_url:rq->url);
	const char *host = url->host;
	u_short port = url->port;
	bool isIp = false;
	const char *ssl = NULL;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (TEST(rq->workModel,WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
		if (TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)) {
			if (ip==NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->server->get_socket(), (struct sockaddr *) &s_sockaddr, &addr_len);
		KSocket::make_ip(&s_sockaddr, mip, MAXIPLEN);
		host = mip;
#ifdef KSOCKET_IPV6
		if (s_sockaddr.v4.sin_family == PF_INET6) {
			port = ntohs(s_sockaddr.v6.sin6_port);
		} else
#endif
		port = ntohs(s_sockaddr.v4.sin_port);
		isIp = true;
	}
#endif
#endif
	if (url->proto == PROTO_HTTPS) {
		ssl = "s";
	}
	KSingleAcserver *sa = cdnContainer.refsRedirect(ip,host,port,ssl,2,Proto_http,isIp);
	if (sa==NULL) {
		fo->connectCallBack(rq,NULL);
	} else {
		sa->connect(rq);
		sa->release();
	}
	KTHREAD_RETURN;
}
void handleUpStreamReadBodyResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadBody(rq,got);
}
void handleUpstreamReadHeadResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadHead(rq,got);
}
void handleUpstreamReadPostResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadPost(rq,got);
}
void handleUpstreamSendPostResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSendPost(rq,got);
}
void handleUpstreamSendHeadResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (got==-1) {
		fo->handleConnectError(rq,STATUS_GATEWAY_TIMEOUT,"Cann't send head to remote host");
		return;
	}
	fo->handleSendHead(rq,got);
}
void handleUpstreamConnectResult(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleConnectResult(rq,got);
}
void KAsyncFetchObject::handleConnectResult(KHttpRequest *rq,int got)
{
	if (got==-1) {
		handleConnectError(rq,STATUS_GATEWAY_TIMEOUT,"connect to remote host time out");
		return;
	}
/////////[324]
	sendHead(rq);
}
void KAsyncFetchObject::retryOpen(KHttpRequest *rq)
{
	if (client) {
		rq->selector->removeSocket(rq);
/////////[325]
		client->destroy(-1);
		client = NULL;
/////////[326]
	}
	hot = header;
	//重置接收的buffer
	buffer.destroy();
	//重置post body
	rq->resetPostBody();
	//重置协议解析
	reset();
	open(rq);
}
void KAsyncFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	tryCount++;
	if (brd==NULL) {
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
		if (TEST(rq->workModel,WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
			asyncDnsCallBack(rq);
			return;
		}
#endif
#endif
		//异步解析
		rq->selector->removeRequest(rq);
		conf.dnsWorker->start(rq,asyncDnsCallBack);
		return;
	}
	if (!brd->rd->enable) {
		handleError(rq,STATUS_SERVICE_UNAVAILABLE,"extend is disable");
		return ;
	}
	badStage = BadStage_Connect;
	brd->rd->connect(rq);
}
void KAsyncFetchObject::connectCallBack(KHttpRequest *rq,KPoolableSocket *client,bool half_connection)
{
	this->client = client;
	if(this->client==NULL || this->client->get_socket()==INVALID_SOCKET){
		if (client) {
			client->isBad(BadStage_Connect);
		}
		if (tryCount<=conf.errorTryCount) {
			//connect try again
			retryOpen(rq);
			return;
		}
		handleError(rq,STATUS_GATEWAY_TIMEOUT,"Cann't connect to remote host");
		return;
	}
/////////[327]
	if (half_connection) {
		rq->handler = handleUpstreamConnectResult;
		rq->selector->addRequest(rq,KGL_LIST_CONNECT,STAGE_OP_UPSTREAM_CONNECT);
	} else {
		sendHead(rq);
	}
}
void KAsyncFetchObject::sendHead(KHttpRequest *rq)
{
	badStage = BadStage_Send;
	buildHead(rq);
	unsigned len = buffer.startRead();
	if (len==0) {
		handleError(rq,STATUS_SERVER_ERROR,"cann't build head");
		return;
	}
	client->setdelay();
	rq->handler = handleUpstreamSendHeadResult;
#ifdef _WIN32
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_WHEAD);
#else
	this->handleSendHead(rq,0);
#endif
}
void KAsyncFetchObject::continueReadBody(KHttpRequest *rq)
{
	//检查是否还要继续读body
	if (!checkContinueReadBody(rq)) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	rq->handler = handleUpStreamReadBodyResult;
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_READ);
}
//解析完body数据之后，从缓冲区读body数据并处理
void KAsyncFetchObject::readBody(KHttpRequest *rq)
{
	int bodyLen;
	for (;;) {
		//对缓冲区的body解析
		char *body = nextBody(rq,bodyLen);
		if (body==NULL) {
			//缓冲区没有了
			break;
		}
		//处理收到了的body数据
		if (!pushHttpBody(rq,body,bodyLen)) {
			return;
		}
	}
	//继续读body
	continueReadBody(rq);
}
//处理读到body数据
void KAsyncFetchObject::handleReadBody(KHttpRequest *rq,int got)
{
	assert(header);
	if (got==0) {
		got = client->read(header,current_size);
		/////////[328]
	}
	if (got<=0) {
		assert(rq->send_ctx.body==NULL);
		/* 读body失败,对于未知长度，如果没有设置cache_no_length,则不缓存 */
		lifeTime = -1;
		if (!TEST(rq->filter_flags,RF_CACHE_NO_LENGTH)
			|| TEST(rq->ctx->obj->index.flags,(ANSW_CHUNKED|ANSW_HAS_CONTENT_LENGTH))) { 
			SET(rq->ctx->obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
		}
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	//解析body
	parseBody(rq,header,got);
	//解析完继续处理body数据
	readBody(rq);
}
void KAsyncFetchObject::handleSendHead(KHttpRequest *rq,int got)
{
	if (got==0) {
		iovec iov[8];
		int iovc = 8;
		buffer.getRBuffer(iov,iovc);
		got = client->writev(iov,iovc,client->isSSL());
		/////////[329]
	}
	if (got<=0) {
		handleConnectError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server");
		return;
	}
	if(buffer.readSuccess(got)){
		//continue send head
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_WHEAD);
		return;
	}
	buffer.destroy();
	if (rq->left_read>0) {
		//handle post data
		if(rq->parser.bodyLen>0){
			//send pre load post data
			rq->handler = handleUpstreamSendPostResult;
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE_POST);
			return;
		}
		//read post data
		readPost(rq,TEST(rq->workModel,WORK_MODEL_SSL)==0);
		return;
	}
	//发送头成功,无post数据处理.
	client->setnodelay();
	rq->handler = handleUpstreamReadHeadResult;
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_RHEAD);
}
void KAsyncFetchObject::handleSendPost(KHttpRequest *rq,int got)
{
	if (got==0) {
		iovec iov[4];	
		int iovc = 4;
		getPostRBuffer(rq,iov,iovc);
		got = client->writev(iov,iovc,client->isSSL());
		/////////[330]
	}
	if (got<=0) {
		handleConnectError(rq,STATUS_SERVER_ERROR,"cann't send post data to remote server");
		return;
	}
	assert(rq->left_read>=0);
	bool continueSendPost = false;
	int pre_loaded_body = (int)MIN((INT64)rq->parser.bodyLen,rq->left_read);
	if (pre_loaded_body>0) {
		rq->left_read -= got;
		assert(got<=rq->parser.bodyLen);
		rq->parser.body += got;
		rq->parser.bodyLen -= got;
		pre_loaded_body -= got;
		continueSendPost = pre_loaded_body>0;
	} else {
		continueSendPost = buffer.readSuccess(got);
		if (!continueSendPost) {
			//重置buffer,准备下一次post
			buffer.destroy();
		}
	}
	if (continueSendPost) {
		rq->handler = handleUpstreamSendPostResult;
		//rq->selector->addList(rq,KGL_LIST_RW);
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE_POST);
	} else {
		//try to read post
		if (rq->left_read==0) {
			client->setnodelay();
			rq->handler = handleUpstreamReadHeadResult;
			//rq->selector->addList(rq,KGL_LIST_RW);
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_RHEAD);
			return;
		}
		readPost(rq,TEST(rq->workModel,WORK_MODEL_SSL)==0);
	}
}
void KAsyncFetchObject::handleReadPost(KHttpRequest *rq,int got)
{
	char *buf = NULL;
	if (got==0) {
		buf = getPostWBuffer(rq,got);
		got = rq->server->read(buf,got);
#ifdef KSOCKET_SSL
		if (got<=0 &&  TEST(rq->workModel,WORK_MODEL_SSL)) {
			KSSLSocket *sslsocket = static_cast<KSSLSocket *>(rq->server);
			if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_READ) {
				readPost(rq);
				return;
			}
		}
#endif
	}
	if (got<=0) {
		stageEndRequest(rq);
		return;
	}
#ifdef ENABLE_INPUT_FILTER
	if (rq->if_ctx) {
		int len;
		if (buf==NULL) {
			buf = getPostWBuffer(rq,len);
		}
		if (JUMP_DENY==rq->if_ctx->check(buf,got,rq->left_read<=got)) {
			denyInputFilter(rq);
			return;
		}
	}
#endif
	rq->left_read-=got;
	buffer.writeSuccess(got);
	sendPost(rq);
}
void KAsyncFetchObject::readPost(KHttpRequest *rq,bool useEvent)
{
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		int got = 0;
		char *tbuf = getPostWBuffer(rq,got);
		got = rq->tf->readBuffer(tbuf,got);
		if (got<=0) {
			handleError(rq,STATUS_SERVER_ERROR,"cann't read post data from temp file");
			return;
		}
		rq->left_read-=got;
		buffer.writeSuccess(got);
		sendPost(rq);
		return;
	}
#endif
	//如果没有临时文件交换，因为已经读了客户的post数据，无法重置，这种情况就禁止出错重试
	tryCount = -1;
	if (!useEvent) {
		handleReadPost(rq,0);
		return;
	}
	rq->handler = handleUpstreamReadPostResult;
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_READ_POST);
}
void KAsyncFetchObject::sendPost(KHttpRequest *rq)
{	
	//创建post
	buildPost(rq);
	buffer.startRead();
	rq->handler = handleUpstreamSendPostResult;
	//rq->selector->addList(rq,KGL_LIST_RW);
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE_POST);
}
void KAsyncFetchObject::handleReadHead(KHttpRequest *rq,int got)
{
	char *buf = hot;
	if (got==0) {
		buf = getHeadRBuffer(rq,got);
		got = client->read(buf,got);
		/////////[331]
	}
	if (got<=0) {
		handleConnectError(rq,STATUS_GATEWAY_TIMEOUT,"cann't recv head from remote server");
		return;
	}
	assert(hot);
	//headSended = true;
	badStage = BadStage_Recv;
	hot += got;
	switch(parseHead(rq,buf,got)){
		case Parse_Success:
			client->isGood();
			handleUpstreamRecvedHead(rq);
			break;
		case Parse_Failed:
			handleError(rq,STATUS_GATEWAY_TIMEOUT,"cann't parse upstream protocol");
			break;
		case Parse_Continue:
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_UPSTREAM_RHEAD);
			break;
	}
	
}
void KAsyncFetchObject::handleConnectError(KHttpRequest *rq,int error,const char *msg)
{
	char ips[MAXIPLEN];
	client->get_remote_ip(ips,sizeof(ips));
	klog(KLOG_NOTICE,"rq = %p connect to %s:%d error code=%d,msg=[%s],try count=%d,last errno=%d %s,socket=%d(%s) (%d %d)\n",
		(KSelectable *)rq,
		ips,
		client->get_remote_port(),
		error,
		msg,
		tryCount,
		errno,
		strerror(errno),
		client->get_socket(),
		(client->isNew()?"new":"pool"),
#ifndef NDEBUG
		(rq->server->shutdownFlag?1:0),
		(client->shutdownFlag?1:0)
#else
		2,2
#endif
		);
	assert(client);
	lifeTime = -1;
	client->isBad(badStage);
	SET(rq->flags,RQ_UPSTREAM_ERROR);
	if (tryCount>=0 && tryCount<=conf.errorTryCount) {
		//try again
		retryOpen(rq);
		return;
	}
	if (rq->ctx->lastModified>0 && TEST(rq->filter_flags,RF_IGNORE_ERROR)) {
		rq->ctx->obj->data->status_code = STATUS_NOT_MODIFIED;
		handleUpstreamRecvedHead(rq);
		return;
	}
	handleError(rq,error,msg);
}
