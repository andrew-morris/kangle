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
/////////[196]
#include "time_utils.h"
#include "KSelectorManager.h"
#include "KFilterContext.h"
#include "KUpstreamFetchObject.h"
#include "KSubRequest.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpFilterContext.h"
using namespace std;
void WINAPI free_auto_memory(void *arg)
{
	xfree(arg);
}
void resultNextSubRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	async_http_start(rq);
}
#ifdef ENABLE_TF_EXCHANGE
void stageTempFileWriteEnd(KHttpRequest *rq)
{
	delete rq->tf;
	rq->tf = NULL;
	stageEndRequest(rq);
}
void resultTempFileRequestWrite(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageTempFileWriteEnd(rq);
		return;
	}
	rq->send_size += got;
	if (rq->tf->readSuccess(got)) {
		startTempFileWriteRequest(rq);
		return;
	}
	stageTempFileWriteEnd(rq);
}
void bufferTempFileRequestWrite(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	bufCount = 1;
	int len;
	buf[0].iov_base = (char *)rq->tf->readBuffer(len);
	buf[0].iov_len = len;
}
void startTempFileWriteRequest(KHttpRequest *rq)
{
	assert(rq->tf);
	if (rq->slh) {
		int len = 0;
		rq->tf->readBuffer(len);
		int sendTime = rq->getSleepTime(len);
		if (sendTime>0) {
			rq->c->delayWrite(rq,resultTempFileRequestWrite,bufferTempFileRequestWrite,sendTime);
			return;
		}
	}
	rq->c->write(rq,resultTempFileRequestWrite,bufferTempFileRequestWrite);
}
#endif
void bufferRequestWrite(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->get_write_buf(buf,bufCount);
}

void resultRequestWrite(void *arg,int got);

void startWriteRequest(KHttpRequest *rq)
{
	if (rq->slh) {
		int len = 0;
		rq->get_write_buf(len);
		int sendTime = rq->getSleepTime(len);

		if (sendTime>0) {
			rq->c->delayWrite(rq,resultRequestWrite,bufferRequestWrite,sendTime);
			return;
		}
	}
	rq->c->write(rq,resultRequestWrite,bufferRequestWrite);
}
void resultRequestWrite(void *arg,int got)
{

	KHttpRequest *rq = (KHttpRequest *)arg;
	switch(rq->canWrite(got)){
		case WRITE_FAILED:
			//removeSocket(rq);
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			rq->buffer.clean();
			stageEndRequest(rq);
			break;
		case WRITE_SUCCESS:
			rq->buffer.clean();
			if (rq->fetchObj && !rq->fetchObj->isClosed()) {
				//读数据
				rq->fetchObj->readBody(rq);
			} else {
				stageEndRequest(rq);
			}
			break;
		default:
			startWriteRequest(rq);
	}
}
void bufferRequestRead(void *arg,iovec *buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	int len;
	buf[0].iov_base = rq->get_read_buf(len);
	buf[0].iov_len = len;
	bufCount = 1;
}
void resultRequestRead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	switch(rq->canRead(got)){
	case READ_FAILED:
		delete rq;
		break;
	case READ_SUCCESS:
		handleStartRequest(rq,0);
		break;
	default:
		rq->c->read(rq,resultRequestRead,bufferRequestRead);
	}
}

void stageWriteRequest(KHttpRequest *rq,buff *buf,int start,int len)
{
	 if (rq->fetchObj) {
		KConnectionSelectable *st = rq->fetchObj->getSelectable();
		if (st) {
			st->removeSocket();
		}
	}
	rq->send_ctx.append(buf,(int)start,(int)len);
	if (rq->send_ctx.getBufferSize()==0) {
		stageEndRequest(rq);
	} else {
		startWriteRequest(rq);
	}
}
void stageWriteRequest(KHttpRequest *rq)
{
	if (TEST(rq->flags,RQ_SYNC)) {
		//同步模式发送header
		if (rq->send_ctx.getBufferSize()>0) {
			if (!rq->sync_send_header()) {
				return;
			}
			rq->send_ctx.clean();
		}
		//同步模式发送buffer
		if (rq->buffer.getLen()>0) {
			rq->sync_send_buffer();
		}
		return;
	}
	stageWriteRequest(rq,rq->buffer.getHead(),0,rq->buffer.getLen());
}
bool KHttpRequest::closeConnection()
{
	if (TEST(c->st_flags,STF_READ|STF_WRITE)) {
		if (c->socket) {
			c->socket->shutdown(SHUT_RDWR);
			SET(c->st_flags, STF_ERR);
			SET(flags,RQ_CONNECTION_CLOSE);
			/////////[197]
		}
	} 
	if (fetchObj) {
		KSelectable *s = fetchObj->getSelectable();
		if (s) {
			SET(s->st_flags, STF_ERR);
			KSocket *socket = s->getSocket();
			socket->shutdown(SHUT_RDWR);
			/////////[198]
		}
	}
	return true;
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
/////////[199]
		len = c->read(this,buf, len);
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
/////////[200]
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
/////////[201]
#ifdef ENABLE_REQUEST_QUEUE
	if (queue) {		
		async_queue_destroy(queue);
		queue->release();
	}
#endif
}
StreamState KHttpRequest::write_all(const char *buf, int len) {
/////////[202]
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
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
		send_size += len;
		if(c->write_all(this,buf, len)){
			return STREAM_WRITE_SUCCESS;
		}
		return STREAM_WRITE_FAILED;
	} else {
		//异步发送
		buffer.write_all(buf,len);
		return STREAM_WRITE_SUCCESS;
	}
}
void KHttpRequest::get_write_buf(iovec *buffer,int &bufferCount)
{
	if (slh) {
		bufferCount = 1;		
	}
	send_ctx.getReadBuffer(buffer,bufferCount);
	return;
}
char *KHttpRequest::get_write_buf(int &size)
{
	iovec buffer;
	int bufferCount = 1;
	send_ctx.getReadBuffer(&buffer,bufferCount);
	size = buffer.iov_len;
	return (char *)buffer.iov_base;
}
char *KHttpRequest::get_read_buf(int &size)
{
	int used = hot - readBuf;
	if ((unsigned) used >= (current_size-1)) {
		//readBuf不够读
		//要重新分配大一点的
		current_size += current_size;
		char *nb = (char *) xmalloc(current_size);
		/* resize buf */
		if (!nb) {
			size = 0;
			return NULL;
		}
		memcpy(nb, readBuf, used);
		xfree(readBuf);
		readBuf = nb;
		hot = readBuf + used;		
	}
	size = current_size - used - 1;
	return hot;
}
WriteState KHttpRequest::canWrite(int got)
{
	if (got<=0) {
		return WRITE_FAILED;
	}
	send_size += got;
	if (send_ctx.readSuccess(got)) {
		return WRITE_CONTINUE;
	}
	return WRITE_SUCCESS;
}
ReadState KHttpRequest::canRead(int got) {
	if (got<=0) {
		return READ_FAILED;
	}
	hot += got;
	int status;
	/////////[203]
		status = parser.parse(readBuf, hot - readBuf, this);	
	if (status == HTTP_PARSE_CONTINUE && current_size >= MAX_HTTP_HEAD_SIZE) {
		//head is too large
		return READ_FAILED;
	}
	if (status == HTTP_PARSE_FAILED) {
		//debug("Failed to check headers,client=%s:%d.\n",
		//		server->get_remote_ip().c_str(), server->get_remote_port());
		SET(raw_url.flags,KGL_URL_BAD);
	}
	if (status != HTTP_PARSE_CONTINUE) {
		return READ_SUCCESS;
	}
	return READ_CONTINUE;
}
void KHttpRequest::beginRequest() {
	assert(url==NULL);
	mark = 0;
	url = new KUrl;
	url->flags = raw_url.flags;
	if(raw_url.host){
		url->host = xstrdup(raw_url.host);
	}
	if(raw_url.path){
		url->path = xstrdup(raw_url.path);
		url_decode(url->path,0,url,false);
		KFileName::tripDir3(url->path, '/');
	}
	if (auth) {
		if (!auth->verifySession(this)) {
			delete auth;
			auth = NULL;
		}
	}
	if (raw_url.param) {
		/////////[204]
		url->param = xstrdup(raw_url.param);
		/////////[205]
	}
	url->port = raw_url.port;
	/*
	 计算还有多少post数据
	 */
	left_read = content_length;
	pre_post_length = (int)(MIN(left_read,(INT64)parser.bodyLen));
	request_msec = kgl_current_msec;
#ifdef MALLOCDEBUG
	if (quit_program_flag!=PROGRAM_NO_QUIT) {
		SET(flags,RQ_CONNECTION_CLOSE);
	}
#endif
/////////[206]
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
/////////[207]
bool KHttpRequest::rewriteUrl(const char *newUrl, int errorCode, const char *prefix) {
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
		s << errorCode << ";";
		if (TEST(url->flags, KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		s << "://" << url->host	<< ":" << url->port << url->path;
		if (url->param) {
			s << "?" << url->param;
		}
	}
	if (url2.host == NULL) {
		url2.host = strdup(url->host);
	}
	url_decode(url2.path, 0, &url2);
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
	if (url2.flags > 0) {
		url->flags = url2.flags;
	}
	SET(raw_url.flags,KGL_URL_REWRITED);
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
/////////[208]
	raw_url.getUrl(s,true);
/////////[209]
	urlLock.Unlock();
	return s.getString();
}
void KHttpRequest::init() {
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
	if (c) {
		c->endResponse(this,keep_alive);
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
/////////[210]
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
	while (ch_request) {
		KCleanHook *nch = ch_request->next;
		ch_request->callBack(ch_request->data);
		delete ch_request;
		ch_request = nch;
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
		svh->release();
		svh = NULL;
	}
}
void KHttpRequest::close()
{
#ifdef ENABLE_KSAPI_FILTER
	/* http filter end connection hook */
	if (svh && svh->vh->hfm) {
		KHttpFilterHookCollect *end_connection = svh->vh->hfm->hook.end_connection;
		if (end_connection) {
			end_connection->process(this,KF_NOTIFY_END_CONNECT,NULL);
		}
	}
	if (conf.gvm->globalVh.hfm) {
		KHttpFilterHookCollect *end_connection = conf.gvm->globalVh.hfm->hook.end_connection;
		if (end_connection) {
			end_connection->process(this,KF_NOTIFY_END_CONNECT,NULL);
		}
	}
#endif
	if (list!=KGL_LIST_NONE) {
		assert(c->selector);
		c->selector->removeList(this);
	}
	ctx->clean_obj(this);
	clean(false);
	while (ch_connect) {
		KCleanHook *nch = ch_connect->next;
		ch_connect->callBack(ch_connect->data);
		delete ch_connect;
		ch_connect = nch;
	}
	
	xfree(readBuf);
	if (auth) {
		delete auth;
	}
	delete ctx;
	
	freeUrl();
	releaseVirtualHost();
#ifdef ENABLE_KSAPI_FILTER
	if (http_filter_ctx) {
		delete http_filter_ctx;
	}
#endif
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
	CLR(flags,RQ_HAVE_RANGE|RQ_HAS_GZIP|RQ_HAS_IF_MOD_SINCE|RQ_OBJ_STORED);
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
	c->next(this,resultNextSubRequest);
	//c->handler = handleNextSubRequest;
	//c->selector->addRequest(this,KGL_LIST_RW,STAGE_OP_NEXT);
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
int KHttpRequest::parseHost(char *val)
{
	urlLock.Lock();
	if (raw_url.host == NULL) {
		char *p = NULL ;
		if(*val == '['){
			SET(raw_url.flags,KGL_URL_IPV6);
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
		if (p) {
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
	return PARSE_HEADER_SUCCESS;
}
int KHttpRequest::parseHeader(const char *attr, char *val,int &val_len, bool isFirst) {
/////////[211]
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
		return parseHost(val);
	}
	if (!strcasecmp(attr, "Connection")
/////////[212]
	)
	{
		if (!strncasecmp(val, "keep-alive", 10) && conf.keep_alive>0)
			flags |= RQ_HAS_KEEP_CONNECTION;
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		/////////[213]
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
		return PARSE_HEADER_NO_INSERT;
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
	if (
/////////[214]
	!strcasecmp(attr, AUTH_REQUEST_HEADER)) {
/////////[215]
		flags |= RQ_HAS_AUTHORIZATION;
/////////[216]
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
/////////[217]
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
#ifdef ENABLE_KSAPI_FILTER
void KHttpRequest::init_http_filter()
{
	if (http_filter_ctx==NULL) {
		http_filter_ctx = new KHttpFilterContext;
		http_filter_ctx->init(this);
	}
}
#endif
bool KHttpRequest::responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len)
{
	/////////[218]
	int len = name_len + val_len + 4;
	char *buf = (char *)malloc(len);
	char *hot = buf;
	memcpy(hot,name,name_len);
	hot += name_len;
	memcpy(hot,": ",2);
	hot += 2;
	memcpy(hot,val,val_len);
	hot += val_len;
	memcpy(hot,"\r\n",2);
	send_ctx.pushEnd(buf,len);
	return true;
}
//发送完header开始发送body时调用
void KHttpRequest::startResponseBody()
{
	assert(!TEST(flags,RQ_HAS_SEND_HEADER));
	if (TEST(flags,RQ_HAS_SEND_HEADER)) {
		return;
	}
	assert(send_size==0);
	SET(flags,RQ_HAS_SEND_HEADER);
	/////////[219]
	kgl_str_t request_line;
	getRequestLine(status_code,&request_line);
	send_ctx.insert(request_line.data,request_line.len);
	send_ctx.append("\r\n",2);
	c->startResponse(this);
	return;
	
}
bool KHttpRequest::sync_send_buffer()
{
	for (;;) {
		iovec iov[16];
		int bufCount = 16;
		buffer.getReadBuffer(iov, bufCount);
		int got = c->write(this, iov, bufCount);
		if (got <= 0) {
			buffer.clean();
			return false;
		}
		if (!buffer.readSuccess(got)) {
			break;
		}
	}
	buffer.clean();
	return true;
}
bool KHttpRequest::sync_send_header()
{
	//同步模式发送header
	for (;;) {
		iovec iov[16];
		int bufCount = 16;
		send_ctx.getReadBuffer(iov, bufCount);
		int got = c->write(this, iov, bufCount);
		if (got <= 0) {
			send_ctx.clean();
			return false;
		}
		if (!send_ctx.readSuccess(got)) {
			break;
		}
	}
	send_ctx.clean();
	return true;
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
}
RequestList::RequestList() {
	head = end = NULL;
}
