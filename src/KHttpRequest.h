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
#ifndef request_h_include
#define request_h_include
#include <string.h>
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "KSocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpProtocolParserHook.h"
#include "KHttpProtocolParser.h"
#include "KHttpHeader.h"
#include "KBuffer.h"
#include "KReadWriteBuffer.h"
#include "KDomainUser.h"
#include "KSendable.h"
#include "KString.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "KServer.h"
#include "KFlowInfo.h"
#include "KConnectionSelectable.h"

#define READ_BUFF_SZ	8192

class KFetchObject;
class KSubVirtualHost;
class KSelector;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
class KHttpSpdyContext;
#define		REQUEST_EMPTY	0
#define		REQUEST_READY	1
#define 	MIN_SLEEP_TIME	4
#define FOLLOW_LINK_ALL  1
#define FOLLOW_LINK_OWN  2
#define FOLLOW_PATH_INFO 4
/////////[344]
#define AUTH_REQUEST_HEADER "Authorization"
#define AUTH_RESPONSE_HEADER "WWW-Authenticate"
#define AUTH_STATUS_CODE 401
/////////[345]

class KManageIP {
public:
	KMutex ip_lock;
	std::map<ip_addr, unsigned> ip_map;
};
/**
关于子请求，开始一个子请求调用request->beginSubRequest,函数不返回。
带一个回调函数，和一个参数。
回调函数只会调用一次。具体处理由action指定。
sub_request_free 指示请求没有正常完成，无需继续，只要释放相关内存操作即可。
sub_request_pop  指示请求正常返回。继续相关操作。
*/
enum sub_request_action {
	sub_request_free,
	sub_request_pop
};
typedef void (* sub_request_call_back) (KHttpRequest *rq,void *data,sub_request_action action);
typedef void (WINAPI * request_clean_call_back) (void *data);
class KSubRequest;
class KContext;
class KOutputFilterContext;
class KHttpFilterContext;
class KCleanHook
{
public:
	request_clean_call_back callBack;
	void *data;
	KCleanHook *next;
};
void WINAPI free_auto_memory(void *arg);
class KHttpRequestData
{
public:
	int flags;
	int filter_flags;
	//post数据还剩多少数据没处理
	INT64 left_read;
	//post数据长度
	INT64 content_length;
	time_t if_modified_since;
	time_t min_obj_verified;
	INT64 range_from;
	INT64 range_to;
	//pre_post的长度
	int pre_post_length;
	unsigned short status_code;
	unsigned short cookie_stick;
	INT64 send_size;
};
class KHttpRequest: public KHttpProtocolParserHook,public KStream,public KHttpRequestData {
public:
	inline KHttpRequest(KConnectionSelectable *c)
	{
		stackSize = 0;
		list = KGL_LIST_NONE;
		fetchObj = NULL;
		readBuf = NULL;
		readBuf = (char *) xmalloc(READ_BUFF_SZ);
		current_size = READ_BUFF_SZ;
		svh = NULL;
		auth = NULL;
		url = NULL;
		file = NULL;
		of_ctx = NULL;
		client_ip = NULL;
		bind_ip = NULL;
		ch_request = NULL;
		ch_connect = NULL;
#ifdef ENABLE_INPUT_FILTER
		if_ctx = NULL;
#endif
		/////////[346]
		ctx = new KContext;
		slh = NULL;
#ifdef ENABLE_TF_EXCHANGE
		tf = NULL;
#endif
		sr = NULL;
		meth = 0;
		fh = NULL;
		mark = 0;
#ifdef ENABLE_REQUEST_QUEUE
		queue = NULL;
#endif
#ifdef ENABLE_KSAPI_FILTER
		http_filter_ctx = NULL;
#endif
		/////////[347]
		this->c = c;
		active_msec = kgl_current_msec;
		request_msec = active_msec;
	}
	inline ~KHttpRequest()
	{
		close();
#ifdef ENABLE_REQUEST_QUEUE
		assert(queue == NULL);
#endif
		if (c) {
			c->release(this);
		}
	}
	void close();
	void clean(bool keep_alive=true);
	void init();
	bool isBad();
	char *get_read_buf(int &size);
	char *get_write_buf(int &size);
	void get_write_buf(LPWSABUF buffer,int &bufferCount);
	/*
	 读post数据时调用这个,而不要直接调用server->read了。
	 */
	int read(char *buf, int len);
	std::string getInfo();
	char *getUrl();
	void beginRequest();
	ReadState canRead(int aio_got=0);
	WriteState canWrite(int aio_got=0);
	SOCKET getSockfd() {
		return c->socket->get_socket();
	}
	bool getPeerAddr(ip_addr *addr) {
		c->socket->get_remote_addr(addr);
		return true;
	}
	int getFollowLink()
	{
		int follow_link = 0;
		if (conf.path_info) {
			follow_link|=FOLLOW_PATH_INFO;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_OWN)) {
			follow_link|=FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_ALL)) {
			follow_link|=FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void endParse();
	bool closeConnection();
	void closeFetchObject(bool destroy=true);
	void resetFetchObject();

	void freeUrl();
	bool rewriteUrl(const char *newUrl, int errorCode = 0,const char *prefix = NULL);
	char http_major;
	char http_minor;
	char meth;
	char state;
	char *hot;
	char *readBuf;
	size_t current_size;
	INT64 request_msec;
	INT64 active_msec;
	const char *getState() {
		switch (state) {
		case STATE_IDLE:
			return "idle";
		case STATE_CONNECT:
			return "connect";
		case STATE_SEND:
			return "send";
		case STATE_RECV:
			return "recv";
		case STATE_QUEUE:
			return "queue";
		}
		return "unknow";
	}
	KConnectionSelectable *c;
#ifdef ENABLE_TF_EXCHANGE
	//临时文件
	KTempFile *tf;
	void closeTempFile()
	{
		if (tf) {
			delete tf;
			tf = NULL;
		}
		SET(flags,RQ_TEMPFILE_HANDLED);
	}
#endif
	//输出缓冲
	KReadWriteBuffer buffer;
	//数据源
	KFetchObject *fetchObj;
	//物理文件映射
	KFileName *file;
	//虚拟主机
	KSubVirtualHost *svh;
	void releaseVirtualHost();
	//子请求
	KSubRequest *sr;
	/*
	 * 原始url
	 */
	KUrl raw_url;
	KUrl *url;
	//http认证
	KHttpAuth *auth;
	KMutex urlLock;
	//输入http协议解析
	KHttpProtocolParser parser;
	//有关object及缓存上下文
	KContext *ctx;
	//发送上下文
	KResponseContext send_ctx;
#ifdef ENABLE_INPUT_FILTER
	bool hasInputFilter()
	{
		if (if_ctx==NULL) {
			return false;
		}
		return !if_ctx->isEmpty();
	}
	/************
	* 输入过滤
	*************/
	KInputFilterContext *if_ctx;
	KInputFilterContext *getInputFilterContext()
	{
		if (if_ctx == NULL && (content_length>0 || url->param)) {
			if_ctx = new KInputFilterContext(this);
		}
		return if_ctx;
	}
#endif
	/****************
	* 输出过滤
	*****************/
	KOutputFilterContext *of_ctx;
	KOutputFilterContext *getOutputFilterContext();
	void addFilter(KFilterHelper *chain);

	inline bool responseStatus(uint16_t status_code)
	{
		this->status_code = status_code;
		return true;
	}
	inline bool responseHeader(know_response_header name,const char *val,hlen_t val_len)
	{
		return true;
	}

	inline bool responseHeader(KHttpHeader *header)
	{
		return responseHeader(header->attr,header->attr_len,header->val,header->val_len);
	}
	inline bool responseHeader(kgl_str_t *name,kgl_str_t *val)
	{
		return responseHeader(name->data,name->len,val->data,val->len);
	}
	inline bool responseHeader(kgl_str_t *name,const char *val,hlen_t val_len)
	{
		return responseHeader(name->data,name->len,val,val_len);
	}
	inline bool responseHeader(const char *name,hlen_t name_len,int val)
	{
		char buf[16];
		int len = snprintf(buf,sizeof(buf)-1,"%d",val);
		return responseHeader(name,name_len,buf,len);
	}
	bool responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len);
	//发送完header开始发送body时调用
	void startResponseBody();
	inline bool needFilter() {
		return of_ctx!=NULL;
	}
	/////////[348]
	int parseHeader(const char *attr, char *val,int &val_len, bool isFirst);
	const char *getMethod();
	void getCharset(KHttpHeader *header);
	StreamState write_all(const char *buf, int len);

	//异步调用，进入子请求，返回时还是打开fetchObj->open，
	void beginSubRequest(KUrl *url,sub_request_call_back callBack,void *data);
	void endSubRequest();
	int checkFilter(KHttpObject *obj);
	u_short workModel;
	unsigned char tmo_left;
	//超时时间是(tmo+1)*conf.time_out
	unsigned char tmo;
	/*
	 * stackSize指示ssi指示内部包含次数。
	 *
	 */
	unsigned char stackSize;
	unsigned char list;
	unsigned char mark;

	KHttpRequest *prev;
	KHttpRequest *next;
	//限速(叠加)
	KSpeedLimitHelper *slh;
	void addSpeedLimit(KSpeedLimit *sl)
	{
		KSpeedLimitHelper *helper = new KSpeedLimitHelper(sl);
		helper->next = slh;
		slh = helper;
	}
	int getSleepTime(int len)
	{
		int sleepTime = 0;
		KSpeedLimitHelper *helper = slh;
		while (helper) {
			int t = helper->sl->getSleepTime(len);
			if (t>sleepTime) {
				sleepTime = t;
			}
			helper = helper->next;
		}
		return sleepTime;
	}
	//客户真实ip(有可能被替换)
	char *getClientIp()
	{
		if (client_ip) {
			return client_ip;
		}
		client_ip = (char *)malloc(MAXIPLEN);
		c->socket->get_remote_ip(client_ip,MAXIPLEN);
		return client_ip;
	}
	char *client_ip;
	//连接上游时，绑定的本机ip
	char *bind_ip;
	//清理钩子,ch为请求结束清理，ch_connect为连接结束清理
	KCleanHook *ch_request;
	KCleanHook *ch_connect;
	void registerRequestCleanHook(request_clean_call_back callBack,void *data)
	{
		KCleanHook *hook = new KCleanHook;
		hook->next = ch_request;
		hook->callBack = callBack;
		hook->data = data;
		ch_request = hook;
	}
	void registerConnectCleanHook(request_clean_call_back callBack,void *data)
	{
		KCleanHook *hook = new KCleanHook;
		hook->next = ch_connect;
		hook->callBack = callBack;
		hook->data = data;
		ch_connect = hook;
	}
#ifdef ENABLE_KSAPI_FILTER
	KHttpFilterContext *http_filter_ctx;
	void init_http_filter();
#endif
	//流量统计
	KFlowInfoHelper *fh;
	void addFlow(INT64 flow,int flowFlag)
	{
		KFlowInfoHelper *helper = fh;
		while (helper) {
			helper->fi->addFlow(flow,flowFlag);
			helper = helper->next;
		}
	}
	void addFlowInfo(KFlowInfo *fi)
	{
		KFlowInfoHelper *helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
	/////////[349]
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue;
#endif
	//从堆上分配内存，在rq删除时，自动释放。
	char *alloc_connect_memory(int size)
	{
		char *buf = (char *)xmalloc(size);
		registerConnectCleanHook(free_auto_memory,buf);
		return buf;
	}
	char *alloc_request_memory(int size)
	{
		char *buf = (char *)xmalloc(size);
		registerRequestCleanHook(free_auto_memory,buf);
		return buf;
	}
	bool sync_send_header();
	bool sync_send_buffer();
private:
	bool parseMeth(const char *src);
	bool parseConnectUrl(char *src);
	bool parseHttpVersion(char *ver);
	int parseHost(char *val);
};
struct RequestError
{
	int code;
	const char *msg;
	void set(int code,const char *msg)
	{
		this->code = code;
		this->msg = msg;
	}
};
inline u_short string_hash(const char *p, u_short res) {
        int i = 8;
        while(*p && i){
                --i;
                res *= *p;
                p++;
        }
        return res;
        /*
        if (p && *p) {
                //p = p + strlen(p) - 1;
                i = 8;
                while ((p >= str) && i) {
                        i--;
                        res += *p * *p;
                        p--;
                }
        }
        return res;
        */
}
/**
* 进入发送数据，发送rq->buffer
*/
void stageWriteRequest(KHttpRequest *rq);
/**
* 进入发送数据，发送指定的buff
*/
void stageWriteRequest(KHttpRequest *rq,buff *buf,int start,int len);
void startTempFileWriteRequest(KHttpRequest *rq);
#endif
