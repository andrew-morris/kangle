/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <errno.h>
#include "malloc_debug.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KThreadPool.h"
#include "lib.h"
#include "http.h"
#include "KVirtualHost.h"
#include "KSelector.h"
#include "KFilterHelper.h"
#include "KHttpKeyValue.h"
#include "KHttpField.h"
#include "KHttpFieldValue.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"
/////////[158]
#include "time_utils.h"
#include "KSelectorManager.h"
#include "KFilterContext.h"
#include "KUpstreamFetchObject.h"
#include "KSubRequest.h"

using namespace std;
void handleNextSubRequest(KSelectable *st,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	async_http_start(rq);
}
#ifdef ENABLE_TF_EXCHANGE
void stageTempFileWriteEnd(KHttpRequest *rq)
{
	delete rq->tf;
	rq->tf = NULL;
	stageEndRequest(rq);
}
void handleRequestTempFileWrite(KSelectable *st,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	kassert(rq->tf!=NULL);
	if (got==0) {
		for(;;){
			char *buf = rq->tf->readBuffer(got);
			if(got<=0){
				SET(rq->flags,RQ_CONNECTION_CLOSE);
				stageTempFileWriteEnd(rq);
				return;
			}
			got = rq->server->write(buf,got);
			if (got <= 0) {
				if (errno==EINTR) {
					continue;
				}
				if (errno==EAGAIN) {
					break;
				}
#ifdef KSOCKET_SSL
				if (TEST(rq->workModel,WORK_MODEL_SSL)) {
					KSSLSocket *sslsocket = static_cast<KSSLSocket *>(rq->server);
					if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_WRITE) {
						break;
					}
				}
#endif

				SET(rq->flags,RQ_CONNECTION_CLOSE);
				stageTempFileWriteEnd(rq);
				return;
			}
			rq->send_ctx.send_size += got;
			if(!rq->tf->readSuccess(got)){
				stageTempFileWriteEnd(rq);
				return;
			}
			if(rq->slh){
				break;
			}
		}
		if(!limitWrite(rq,STAGE_OP_TF_WRITE)){
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_WRITE);
		}
		return;
	}
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageTempFileWriteEnd(rq);
		return;
	}
#ifdef _WIN32
	rq->send_ctx.send_size += got;
	if (rq->tf->readSuccess(got)) {
		if(!limitWrite(rq,STAGE_OP_TF_WRITE)){
			rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_WRITE);
		}
		return;
	}
#endif
	stageTempFileWriteEnd(rq);
}
#endif
void handleRequestWrite(KSelectable *st,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	switch(rq->canWrite(got)){
		case WRITE_FAILED:
			//removeSocket(rq);
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			rq->buffer.clean();
			stageEndRequest(rq);
			break;
		case WRITE_SUCCESS:
			//removeSocket(rq);
			assert(rq->send_ctx.body == NULL);
			rq->buffer.clean();
			if (rq->fetchObj && !rq->fetchObj->isClosed()) {
				//CLR(rq->filter_flags,RQ_SET_LIMIT_SPEED);
				//读数据
				rq->fetchObj->readBody(rq);
			} else {
				stageEndRequest(rq);
			}
			break;
		default:
			if (!limitWrite(rq)) {
				rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE);
			}
	}
}
void handleRequestRead(KSelectable *st,int got)
{
	KHttpRequest *rq = (KHttpRequest *)st;
	int ret = 0;
#ifdef KSOCKET_SSL
	for (;;) {
#endif
		ret = rq->canRead(got);
#ifdef KSOCKET_SSL
		if (ret==READ_SSL_CONTINUE) {
			got = 0;
			continue;
		}
		break;
	}
#endif
	switch(ret){
	case READ_FAILED:
		delete rq;
		break;
	case READ_SUCCESS:
		handleStartRequest(st,0);
		break;
	default:
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_READ);
	}
}
void stageWriteRequest(KHttpRequest *rq,buff *buf,INT64 start,INT64 len)
{
	rq->send_ctx.body_len = len;
	assert(rq->send_ctx.body == NULL);
	while(buf){
		if(start < buf->used){
			rq->send_ctx.body = buf;
			rq->send_ctx.body_start = start;
			break;
		}
		start -= buf->used;
		buf = buf->next;
	}
	rq->hot = rq->send_ctx.init_hot();

	assert(rq->hot);
	rq->handler = handleRequestWrite;	
	if(limitWrite(rq)){
		return;
	}	
#ifdef _WIN32
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_WRITE);
#else
	handleRequestWrite(rq,0);
#endif
}
void stageWriteRequest(KHttpRequest *rq)
{
	stageWriteRequest(rq,rq->buffer.getAllBuf(),0,rq->buffer.getLen());
}
bool KHttpRequest::closeConnection()
{
	if (client_op || (secondHandler && secondHandler->sockop>0)) {
		if (server) {
			server->shutdown(SHUT_RDWR);
			SET(flags,RQ_CONNECTION_CLOSE);
			/////////[159]
		}
	} 
	if(upstream_op && fetchObj) {
		KClientSocket *socket = static_cast<KUpstreamFetchObject *>(fetchObj)->getSocket();
		if (socket) {
			socket->shutdown(SHUT_RDWR);
			/////////[160]
		}
	}
	return true;
}
void KHttpRequest::resetPostBody()
{
	left_read = content_length;
	parser.resetBody();
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		//重置读post数据
		tf->resetRead();
	}
#endif
}
int KHttpRequest::read(char *buf, int len) {
	if (left_read <= 0) {
		return 0;
	}
	len = (int)MIN((INT64)len,left_read);
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		len = tf->readBuffer(buf,len);
	} else {
#endif
		len = server->read(buf, len);
#ifdef ENABLE_INPUT_FILTER
		if (len>0) {
			if (if_ctx && JUMP_DENY==if_ctx->check(buf,len,left_read<=len)) {
				char *u = url->getUrl();
				klog(KLOG_ERR,"access denied by input filter. ip=%s url=%s\n",getClientIp(),u);
				free(u);
				SET(flags,RQ_CONNECTION_CLOSE);
				return -1;
			}
		}
#endif
#ifdef ENABLE_TF_EXCHANGE
	}
#endif
	if (len > 0) {
		left_read -= len;
	}
	return len;
}
void KHttpRequest::resetFetchObject()
{
	if (fetchObj) {
		KFetchObject *fo = fetchObj->clone(this);
		closeFetchObject(true);
		fetchObj = fo;
	}
}
void KHttpRequest::closeFetchObject(bool destroy)
{
/////////[161]
	if (fetchObj) {
		fetchObj->close(this);
		if (destroy) {
			delete fetchObj;
			fetchObj = NULL;
		}
	}
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue = this->queue;
	this->queue = NULL;
#endif
/////////[162]
#ifdef ENABLE_REQUEST_QUEUE
	if (queue) {		
		async_queue_destroy(queue);
		queue->release();
	}
#endif
}
void KHttpRequest::addSendHeader(KBuffer *hbuf)
{
	if (send_ctx.header) {
		hbuf->clean();
		return;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (tf && tf->getSize()>0) {
		//body has send
		hbuf->clean();
		return;
	}
	//*/
#endif
	send_ctx.header_size = (int)hbuf->getLen();
	send_ctx.header = hbuf->stealBuffFast();
}
StreamState KHttpRequest::write_all(const char *buf, int len) {
/////////[163]
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		if (send_ctx.header) {
			//send header
			buff *header = send_ctx.header;
			do {
				buff *n = header->next;
				tf->writeBuffer(this,header->data,header->used);
				free(header->data);
				free(header);
				header = n;
			} while(header);
			send_ctx.header = NULL;
		}
		if(!tf->writeBuffer(this,buf,len)){
			return STREAM_WRITE_FAILED;
		}
		return STREAM_WRITE_SUCCESS;
	}
#endif
	if(TEST(flags,RQ_SYNC)){
		//同步发送
		int sleepTime = getSleepTime(len);
		if (sleepTime>0) {
			my_msleep(sleepTime);
		}
		send_ctx.send_size += len;
		if(server->write_all(buf, len)){
			return STREAM_WRITE_SUCCESS;
		}
		return STREAM_WRITE_FAILED;
	} else {
		//异步发送
		return buffer.write_all(buf,len);
	}
}
#ifdef _WIN32
void KHttpRequest::get_write_buf(LPWSABUF buffer,int &bufferCount)
{
	if (slh) {
		bufferCount = 1;
		buffer[0].len = send_ctx.getSendSize(hot);
		buffer[0].buf = hot;
		return;
	}
	send_ctx.getSendBuffer(buffer,bufferCount,hot);
}
#endif
char *KHttpRequest::get_write_buf(int &size)
{
	size = send_ctx.getSendSize(hot);
	return hot;
}
char *KHttpRequest::get_read_buf(int &size)
{
	int used = hot - readBuf;
	if ((unsigned) used >= current_size) {
		//readBuf不够读
		//要重新分配大一点的
		char *nb = (char *) xmalloc(2*current_size);
		/* resize buf */
		if (!nb) {
			size = 0;
			return NULL;
		}
		memcpy(nb, readBuf, current_size);
		xfree(readBuf);
		//INT64 offset = nb - readBuf;
		//parser.adjustHeader(offset);
		readBuf = nb;
		hot = readBuf + current_size;
		current_size += current_size;
	}
	size = current_size - used;
	return hot;
}
WriteState KHttpRequest::canWrite(int got)
{
#ifdef HAVE_WRITEV
#ifdef KSOCKET_SSL
	if(!TEST(workModel,WORK_MODEL_SSL)){
#endif
	WSABUF wbuf[16];
	int bufferCount = 16;
	if (slh) {
		bufferCount = 1;
		wbuf[0].iov_len = send_ctx.getSendSize(hot);
		wbuf[0].iov_base = hot;
	} else {
		send_ctx.getSendBuffer(wbuf,bufferCount,hot);
	}
retry:
	got = writev(server->get_socket(),wbuf,bufferCount);
	if(got<=0){
	 	if (errno==EINTR) {
			goto retry;
		}
		if (errno==EAGAIN) {
		    return WRITE_CONTINUE;
		}
		return WRITE_FAILED;
	}
	hot = send_ctx.next(hot,got);
	if(hot == NULL){
		assert(send_ctx.body == NULL);
		return WRITE_SUCCESS;
	}
	return WRITE_CONTINUE;
#ifdef KSOCKET_SSL
	}
#endif
#endif
	if (got==0) {
		for (;;) {
			int aio_size;
			char *aio_buf = get_write_buf(aio_size);
			if(aio_buf==NULL){
				return WRITE_FAILED;
			}
			got = server->write(aio_buf,aio_size);
			if (got <= 0) {
				if (errno==EINTR) {
					continue;
				}
				if (errno==EAGAIN) {
					return WRITE_CONTINUE;
				}
#ifdef KSOCKET_SSL
				if (TEST(workModel,WORK_MODEL_SSL)) {
					KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
					if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_WRITE) {
						return WRITE_CONTINUE;
					}
				}
#endif

				return WRITE_FAILED;
			}
			hot = send_ctx.step_next(hot,got);
			if(hot == NULL){
				assert(send_ctx.body == NULL);
				return WRITE_SUCCESS;
			}
			if (slh) {
				return WRITE_CONTINUE;
			}
		}
	}
	if (got <= 0) {
		return WRITE_FAILED;
	}
#ifdef _WIN32
	hot = send_ctx.next(hot,got);
	if(hot == NULL){
		assert(send_ctx.body == NULL);
		return WRITE_SUCCESS;
	}
#endif
	return WRITE_CONTINUE;
}
ReadState KHttpRequest::canRead(int got) {
	if (got==0) {
		char *aio_buf;
		aio_buf = get_read_buf(got);
		if(aio_buf==NULL){
			return READ_FAILED;
		}
		got = server->read(aio_buf, got);
#ifdef KSOCKET_SSL
		if (got<=0 &&  TEST(workModel,WORK_MODEL_SSL)) {
			KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
			if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_READ) {
				return READ_CONTINUE;
			}
		}
#endif
	} 
	if (got<=0) {
		return READ_FAILED;
	}
	//fwrite(hot,got,1,stdout);
	hot += got;	
	int status = parser.parse(readBuf, hot - readBuf, this);
	if (status == HTTP_PARSE_CONTINUE && current_size >= MAX_HTTP_HEAD_SIZE) {
		//head is too large
		return READ_FAILED;
	}
	if (status == HTTP_PARSE_FAILED) {
		//debug("Failed to check headers,client=%s:%d.\n",
		//		server->get_remote_ip().c_str(), server->get_remote_port());
		SET(flags,RQ_IS_BAD);
	}
	if (status != HTTP_PARSE_CONTINUE) {
		return READ_SUCCESS;
		/*
		 selector->removeRequest(this, false);
		 if (!m_thread.start(this, httpWorkThread))
		 selector->removeWork(this);
		 */
	}
#ifdef KSOCKET_SSL
	if (TEST(workModel,WORK_MODEL_SSL)) {
		//有可能SSL_read会返回部分数据，所以我们要一直读，直到SSL_read没有数据返回
		return READ_SSL_CONTINUE;
	}
#endif
	return READ_CONTINUE;
}
void KHttpRequest::beginRequest() {
	assert(url==NULL);
	mark = 0;
	url = new KUrl;
	//assert(url==NULL && url->host==NULL && url->path==NULL && url->param==NULL);
	if(raw_url.host){
		url->host = xstrdup(raw_url.host);
	}
	if(raw_url.path){
		url->path = xstrdup(raw_url.path);
		url_decode(url->path,0,&flags,false);
		KFileName::tripDir3(url->path, '/');
	}
	if (auth) {
		if (!auth->verifySession(this)) {
			delete auth;
			auth = NULL;
		}
	}
	if (raw_url.param) {
		/////////[164]
		url->param = xstrdup(raw_url.param);
		/////////[165]
	}
	url->port = raw_url.port;
	url->proto = raw_url.proto;
	/*
	 计算还有多少post数据
	 */
	left_read = content_length;
	/*
	 * parse failed or success
	 * if parse failed then new a thread send failed msg to client.
	 */
	/*
	if (svh) {
#ifdef ENABLE_VH_RS_LIMIT
		svh->vh->releaseConnection();
#endif
		svh->vh->destroy();
		svh = NULL;
	}
	*/
	request_msec = kgl_current_msec;
}
const char *KHttpRequest::getMethod() {
	return KHttpKeyValue::getMethod(meth);
}
bool KHttpRequest::isBad() {
	if (url==NULL || url->host == NULL || url->path == NULL) {
		//host或者path是空,请求错误
		//	status=REQUEST_CLOSE;
		return true;
	}
	if(meth == METH_UNSET){
		return true;
	}
	return false;
}
void KHttpRequest::freeUrl() {
	urlLock.Lock();
	raw_url.destroy();
	urlLock.Unlock();
}
/////////[166]
bool KHttpRequest::rewriteUrl(char *newUrl, int errorCode, const char *prefix) {
	KUrl url2;
	if (!parse_url(newUrl, &url2)) {
		KStringBuf nu;
		if (prefix) {
			if (*prefix!='/') {
				nu << "/";
			}
			nu << prefix;
			int len = strlen(prefix);
			if (len>0 && prefix[len-1]!='/') {
				nu << "/";
			}
			nu << newUrl;
		} else {
			char *basepath = strdup(url->path);
			char *p = strrchr(basepath,'/');
			if (p) {
				p++;
				*p = '\0';
			}
			nu << basepath;			
			free(basepath);		
			nu << newUrl;
		}	
		if(!parse_url(nu.getString(),&url2)){
			url2.destroy();
			return false;
		}
	}
	if (url2.path == NULL) {
		url2.destroy();
		return false;
	}
	KStringBuf s;
	if (errorCode > 0) {
		s << errorCode << ";" << buildProto(url->proto) << "://" << url->host
				<< ":" << url->port << url->path;
		if (url->param) {
			s << "?" << url->param;
		}
	}
	if (url2.host == NULL) {
		url2.host = strdup(url->host);
	}
	url_decode(url2.path, 0, &flags);
	if (ctx->obj && ctx->obj->url==url && !TEST(ctx->obj->index.flags,FLAG_URL_FREE)) {
		//写时拷贝
		SET(ctx->obj->index.flags,FLAG_URL_FREE);
		ctx->obj->url = url->clone();
	}
	url->destroy();
	url->host = url2.host;
	url->path = url2.path;
	if (errorCode > 0) {
		url->param = s.stealString();
		if (url2.param) {
			xfree(url2.param);
			url2.param = NULL;
		}
	} else {
		url->param = url2.param;
	}
	if (url2.port > 0) {
		url->port = url2.port;
	}
	if (url2.proto > 0) {
		url->proto = url2.proto;
	}
	SET(flags,RQ_IS_REWRITED);
	return true;
}
char *KHttpRequest::getUrl() {
	if (url==NULL) {
		return strdup("");
	}
	return url->getUrl();
}
std::string KHttpRequest::getInfo() {
	KStringBuf s;
	urlLock.Lock();
/////////[167]
	raw_url.getUrl(s,true);
/////////[168]
	urlLock.Unlock();
	return s.getString();
}
void KHttpRequest::init() {
	server->setdelay();
	KHttpRequestData *data = static_cast<KHttpRequestData *>(this);
	memset(data,0,sizeof(KHttpRequestData));
	tmo = 0;
	http_major = 0;
	http_minor = 0;
	state = STATE_IDLE;
	hot = readBuf;
	parser.start();
}
void KHttpRequest::clean(bool keep_alive) {
	if (keep_alive) {
		server->setnodelay();
		SET(workModel,WORK_MODEL_KA);
	}
	closeFetchObject();
	while (sr) {
		KSubRequest *nsr = sr->next;
		sr->destroy(this);
		delete sr;
		sr = nsr;
	}
	if (file){
		delete file;
		file = NULL;
	}
	assert(ctx);
	ctx->clean();
	parser.destroy();
	buffer.clean();
	if (of_ctx) {
		delete of_ctx;
		of_ctx = NULL;
	}
#ifdef ENABLE_INPUT_FILTER
	if (if_ctx) {
		delete if_ctx;
		if_ctx = NULL;
	}
#endif
/////////[169]
	if (url) {
		url->destroy();
		delete url;
		url = NULL;
	}
	send_ctx.clean();
	while (slh) {
		KSpeedLimitHelper *slh_next = slh->next;
		delete slh;
		slh = slh_next;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		delete tf;
		tf = NULL;
	}
#endif
	if (client_ip) {
		free(client_ip);
		client_ip = NULL;
	}
	if (bind_ip) {
		free(bind_ip);
		bind_ip = NULL;
	}
	while (ch) {
		KCleanHook *nch = ch->next;
		ch->callBack(this,ch->data);
		delete ch;
		ch = nch;
	}
	while (fh) {
		KFlowInfoHelper *fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
}
void KHttpRequest::releaseVirtualHost()
{
	if (svh) {
#ifdef ENABLE_VH_RS_LIMIT
		svh->vh->releaseConnection();
#endif
		svh->vh->destroy();
		svh = NULL;
	}
}
void KHttpRequest::close(bool reuse)
{
	if (list!=KGL_LIST_NONE) {
		assert(selector);
		selector->removeList(this);
	}
	ctx->clean_obj(this);
	clean(false);
	while (ch_connect) {
		KCleanHook *nch = ch_connect->next;
		ch_connect->callBack(this,ch_connect->data);
		delete ch_connect;
		ch_connect = nch;
	}
	if(!reuse){
		xfree(readBuf);
		if (auth) {
			delete auth;
		}
		delete ctx;
	} else {
		sockop = 0;
	}
	freeUrl();
	releaseVirtualHost();	
	if (server) {
#ifndef NDEBUG
		//debug("client %s close connection\n",server->get_remote_ip().c_str());
#endif
		delRequest(this);
		selector->removeSocket(this);
		server->shutdown(SHUT_RDWR);
		if (reuse) {
			server->close();
			workModel = 0;
		} else {
#ifdef KSOCKET_SSL
			if (TEST(workModel,WORK_MODEL_SSL)) {
				delete static_cast<KSSLSocket *>(server);
			} else 
#endif
				delete server;
		}
	}
}
void KHttpRequest::endSubRequest()
{
	stackSize--;
	ctx->clean_obj(this);
	if (conf.log_sub_request) {
		log_access(this);
	}
	kassert(sr!=NULL);
	//pop fetchObj
	if (fetchObj) {
		delete fetchObj;
	}
	fetchObj = sr->fetchObj;
	//pop file
	if (file) {
		delete file;
	}
	file = sr->file;
	//pop ctx
	ctx->clean();
	delete ctx;
	ctx = sr->ctx;
	ctx->st->preventWriteEnd = false;
	//pop url,must after pop ctx
	if (url) {
		url->destroy();
		delete url;
	}
	url = sr->url;
	meth = sr->meth;
	flags = sr->flags;
	KSubRequest *nsr = sr->next;
	sub_request_call_back callBack = sr->callBack;
	void *data = sr->data;
	delete sr;
	sr = nsr;
	if (!sr) {
		CLR(workModel,WORK_MODEL_INTERNAL);
	}
	kassert(fetchObj!=NULL);
	callBack(this,data,sub_request_pop);
}
void KHttpRequest::beginSubRequest(KUrl *url,sub_request_call_back callBack,void *data)
{
	if (stackSize>32) {
		//调用层数太多了
		url->destroy();
		delete url;
		callBack(this,data,sub_request_free);
		stageEndRequest(this);
		return;
	}
	stackSize++;
	kassert(url!=NULL);
	KSubRequest *nsr = new KSubRequest;
	nsr->next = sr;
	nsr->fetchObj = fetchObj;
	nsr->file = file;
	nsr->flags = flags;
	CLR(flags,RQ_HAVE_RANGE|RQ_HAS_GZIP|RQ_URL_ENCODE|RQ_HAS_IF_MOD_SINCE|RQ_OBJ_STORED);
	nsr->url = this->url;
	nsr->ctx = ctx;
	assert(ctx);
	//阻止write_end
	ctx->st->preventWriteEnd = true;
	nsr->callBack = callBack;
	nsr->data = data;
	nsr->meth = meth;
	meth = METH_GET;
	fetchObj = NULL;
	file = NULL;
	ctx = new KContext;
	SET(workModel,WORK_MODEL_INTERNAL);
	this->url = url;
	sr = nsr;
	//在调用子请求太多时，要切断堆栈调用,否则会堆栈溢出
	handler = handleNextSubRequest;
	selector->addRequest(this,KGL_LIST_RW,STAGE_OP_NEXT);
}
bool KHttpRequest::parseMeth(const char *src) {
	meth = KHttpKeyValue::getMethod(src);
	if (meth >= 0) {
		return true;
	}
	return false;
}
void KHttpRequest::endParse() {
	if (!TEST(flags,RQ_HAS_AUTHORIZATION|RQ_HAS_PROXY_AUTHORIZATION)) {
		if (auth) {
			delete auth;
			auth = NULL;
		}
	}
}
bool KHttpRequest::parseConnectUrl(char *src) {
	char *ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	*ss = 0;
	raw_url.host = strdup(src);
	raw_url.port = atoi(ss + 1);
	return true;
}
int KHttpRequest::parseHeader(const char *attr, char *val, bool isFirst) {
	if (isFirst) {
		freeUrl();
		if (!parseMeth(attr)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return 0;
		}
		char *space = strchr(val, ' ');
		if (space == NULL) {
			klog(
					KLOG_DEBUG,
					"httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n",
					val);
			return 0;
		}
		*space = 0;
		space++;

		while (*space && IS_SPACE((unsigned char)*space))
			space++;
		bool result;
		//assert((url.host && url.path && TEST(flags,RQ_URL_MOVED)) || (url.host==NULL && url.path==NULL && !TEST(flags,RQ_URL_MOVED)));
		//		url = new KUrl;
		if (meth == METH_CONNECT) {
			result = parseConnectUrl(val);
		} else {
			urlLock.Lock();
			result = parse_url(val, &raw_url);
			urlLock.Unlock();
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return 0;
		}
		if (!parseHttpVersion(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n",
					space);
			return 0;
		}
		return 2;
	}
	if (!strcasecmp(attr, "Host")) {
		urlLock.Lock();
		if (raw_url.host == NULL) {
			char *p = NULL ;
			if(*val == '['){
				SET(raw_url.proto,PROTO_IPV6);
				val++;
				raw_url.host = strdup(val);
				p = strchr(raw_url.host,']');				
				if(p){
					*p = '\0';
					p = strchr(p+1,':');
				}
			}else{
				raw_url.host = strdup(val);
				p = strchr(raw_url.host, ':');
				if(p){
					*p = '\0';
				}
			}
			if(p){
				raw_url.port = atoi(p+1);
			} else {
				if (TEST(workModel,WORK_MODEL_SSL)) {
					raw_url.port = 443;
				} else {
					raw_url.port = 80;
				}
			}
		}
		urlLock.Unlock();
		//flags |= RQ_HAS_HOST;
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Connection")
/////////[170]
	)
	{
		if (!strncasecmp(val, "keep-alive", 10) && conf.keep_alive>0)
			flags |= RQ_HAS_KEEP_CONNECTION;
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		if (strstr(val, "gzip") != NULL) {
				flags |= RQ_HAS_GZIP;
		}
		if (conf.removeAcceptEncoding) {
			return PARSE_HEADER_NO_INSERT;
		}
		return 1;
	}
	if (!strcasecmp(attr,"If-Range")) {
		if_modified_since = parse1123time(val);
		flags |= RQ_IF_RANGE;
		return 2;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		if_modified_since = parse1123time(val);
		flags |= RQ_HAS_IF_MOD_SINCE;
		return 2;
	}

	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		content_length = string2int(val);
		left_read = content_length;
		//flags |= RQ_HAS_CONTENT_LEN;
		return 1;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			SET(flags,RQ_INPUT_CHUNKED);
			return PARSE_HEADER_NO_INSERT;
		}
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			flags |= RQ_HAVE_EXPECT;
		}
		return 2;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			flags |= RQ_HAS_NO_CACHE;
		return PARSE_HEADER_SUCCESS;
	}
	/*if (!strcasecmp(attr, "Proxy-Authorization")) {
	 flags |= RQ_HAS_PROXY_AUTHORIZATION;
	 return PARSE_HEADER_SUCCESS;
	 }
	 */
	if (
/////////[171]
	!strcasecmp(attr, AUTH_REQUEST_HEADER)) {
/////////[172]
		flags |= RQ_HAS_AUTHORIZATION;
/////////[173]
#ifdef ENABLE_TPROXY
		if (!TEST(workModel,WORK_MODEL_TPROXY)) {
#endif
			char *p = val;
			while (*p && !IS_SPACE((unsigned char)*p))
				p++;
			char *p2 = p;
			while (*p2 && IS_SPACE((unsigned char)*p2))
				p2++;

			KHttpAuth *tauth = NULL;
			if (strncasecmp(val, "basic", p - val) == 0) {
				tauth = new KHttpBasicAuth;
			} else if (strncasecmp(val, "digest", p - val) == 0) {
#ifdef ENABLE_DIGEST_AUTH
				tauth = new KHttpDigestAuth;
#endif
			}
			if (tauth) {
				if (!tauth->parse(this, p2)) {
					delete tauth;
					tauth = NULL;
				}
			}
			if (auth) {
				delete auth;
			}
			auth = tauth;
/////////[174]
#ifdef ENABLE_TPROXY
		}
#endif
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-store") || field.is("no-cache")) {
				flags |= RQ_HAS_NO_CACHE;
			} else if (field.is("only-if-cached")) {
				flags |= RQ_HAS_ONLY_IF_CACHED;
		/*
			} else if (field.is("max-age=", &max_age)) {
				flags |= RQ_HAS_MAX_AGE;
			} else if (field.is("min-fresh=", &min_fresh)) {
				flags |= RQ_HAS_MIN_FRESH;
			} else if (field.is("max-stale=", &max_stale)) {
				flags |= RQ_HAS_MAX_STALE;
		*/
			}
		} while (field.next());
		return 1;
	}
	if (!strcasecmp(attr, "Range")) {
		if (!strncasecmp(val, "bytes=", 6)) {
			val += 6;
			range_from = -1;
			range_to = -1;
			range_from = string2int(val);
			char *p = strchr(val, '-');
			if (p && *(p+1)) {
				range_to = string2int(p + 1);
			}
			char *next_range = strchr(val,',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO,"cut multi_range %s\n",val);
				//SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		flags |= RQ_HAVE_RANGE;
		return 1;
	}
	if (!strcasecmp(attr,"Content-Type")) {
#ifdef ENABLE_INPUT_FILTER
		if (if_ctx==NULL) {
			if_ctx = new KInputFilterContext(this);
		}
#endif
		if(strncasecmp(val,"multipart/form-data",19)==0){
			SET(flags,RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			if_ctx->parseBoundary(val+19);
#endif
		}
		return 1;
	}
	return 1;
}
bool KHttpRequest::parseHttpVersion(char *ver) {
	char *dot = strchr(ver,'.');
	if (dot==NULL) {
		return false;
	}
	http_major = *(dot - 1) - 0x30;//major;
	http_minor = *(dot + 1) - 0x30;//minor;
	return true;
}

int KHttpRequest::checkFilter(KHttpObject *obj) {
	int action = JUMP_ALLOW;
	if (TEST(obj->index.flags,FLAG_NO_BODY)) {
		return action;
	}
	if (of_ctx) {
		if(of_ctx->charset == NULL){
			of_ctx->charset = obj->getCharset();
		}
		buff *bodys = obj->data->bodys;
		bool unziped = false;
		INT64 len;
		if (TEST(obj->index.flags,FLAG_GZIP)) {
			//压缩过的数据
			//解压它再进行内容过滤
			bodys = inflate_buff(obj->data->bodys, len, true);
			unziped = true;
		}
		buff *head = bodys;
		while (head && head->used > 0) {
			action = of_ctx->checkFilter(this,head->data, head->used);
			if (action == JUMP_DENY) {
				break;
			}
			head = head->next;
		}
		if (unziped) {
			KBuffer::destroy(bodys);
		}
	}
	return action;
}
void KHttpRequest::addFilter(KFilterHelper *chain) {
	getOutputFilterContext()->addFilter(chain);
}
KOutputFilterContext *KHttpRequest::getOutputFilterContext()
{
	if (of_ctx==NULL) {
		of_ctx = new KOutputFilterContext;
	}
	return of_ctx;
}
KHttpRequest *RequestList::getHead() {
	return head;
}
KHttpRequest *RequestList::getEnd() {
	return end;
}
void RequestList::pushBack(KHttpRequest *rq) {
	rq->next = NULL;
	if (head == NULL) {
		head = end = rq;
		rq->prev = NULL;
	} else {
		end->next = rq;
		rq->prev = end;
		end = end->next;
	}
}
void RequestList::pushFront(KHttpRequest *rq) {
	rq->prev = NULL;
	if (head == NULL) {
		head = end = rq;
		rq->next = NULL;
	} else {
		head->prev = rq;
		rq->next = head;
		head = head->prev;
	}
}
KHttpRequest *RequestList::popBack() {
	KHttpRequest *rq = end;
	if (rq == NULL)
		return NULL;
	end = end->prev;
	if (end) {
		end->next = NULL;
	} else {
		head = NULL;
	}
	return rq;
}
KHttpRequest *RequestList::popHead() {
	KHttpRequest *rq = head;
	if (rq == NULL)
		return NULL;
	head = head->next;
	if (head) {
		head->prev = NULL;
	} else {
		end = NULL;
	}
	return rq;
}

KHttpRequest *RequestList::remove(KHttpRequest *rq) {
	KHttpRequest *rqNext = rq->next;
	assert(rq);
	if (rq->next) {
		rq->next->prev = rq->prev;
	} else {
		//rq是end
		assert(rq==end);
		end = end->prev;
	}
	if (rq->prev) {
		rq->prev->next = rq->next;
	} else {
		//rq是head
		assert(rq==head);
		head = head->next;
	}
	return rqNext;
#if 0
	KHttpRequest *rqNext = rq->next;
	assert(rq);
	if (rq->next) {
		rq->next->prev = rq->prev;
	}
	if (rq->prev) {
		rq->prev->next = rq->next;
	}
	if (rq == end) {
		end = end->prev;
	}
	if (rq == head) {
		head = head->next;
	}
	return rqNext;
#endif
}
RequestList::RequestList() {
	head = end = NULL;
}
