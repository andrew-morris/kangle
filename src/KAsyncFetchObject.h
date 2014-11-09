#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KUpstreamFetchObject.h"
#include "KSocketBuffer.h"
#include "KPoolableSocket.h"
#include "KHttpObject.h"
#include "KSelector.h"
#include "KSelectorManager.h"
enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
/**
* 异步调用扩展，所以支持异步调用的扩展从该类继承
*/
class KAsyncFetchObject : public KUpstreamFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		header = NULL;
		hot = NULL;
		current_size = 0;
		badStage = BadStage_Connect;
		//默认短连接
		lifeTime = -1;
		tryCount = 0;
	}
	void close(KHttpRequest *rq)
	{
		if (client) {
			if (TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| (rq->ctx->obj && TEST(rq->ctx->obj->index.flags,ANSW_CLOSE))) {
				lifeTime = -1;
			}
			rq->selector->removeSocket(rq);
			client->destroy(lifeTime);
			client = NULL;
		}
		KFetchObject::close(rq);

	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->destroy(-1);
		}
		if (header) {
			free(header);
		}
	}
	//期望中的完成，长连接中用于标识此连接还可用
	virtual void expectDone()
	{
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
	void open(KHttpRequest *rq);
	void sendHead(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);

	void handleReadBody(KHttpRequest *rq,int got);
	void handleReadHead(KHttpRequest *rq,int got);
	void handleSendHead(KHttpRequest *rq,int got);
	void handleSendPost(KHttpRequest *rq,int got);
	void handleReadPost(KHttpRequest *rq,int got);
	
	//得到post读缓冲，发送到upstream
	void getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int &bufCount)
	{		
		int pre_loaded_body = (int)(MIN(rq->parser.bodyLen,rq->left_read));
		if (pre_loaded_body > 0) {
			assert(rq->left_read>0);
			bufCount = 1;
#ifdef _WIN32
			buf[0].len= pre_loaded_body;
			buf[0].buf = rq->parser.body;
#else
			buf[0].iov_len= pre_loaded_body;
			buf[0].iov_base = rq->parser.body;
#endif
			return;
		}
		buffer.getRBuffer(buf,bufCount);
	}
	//得到post写缓冲，从client接收post数据
	char *getPostWBuffer(KHttpRequest *rq,int &len)
	{
		assert(rq->parser.bodyLen==0);
		assert(rq->left_read>0);
		char *buf = buffer.getWBuffer(len);
		len = (int)(MIN((INT64)len,rq->left_read));
		assert(len>0);
		return buf;
	}
	//得到head缓冲，从upstream读head
	char *getHeadRBuffer(KHttpRequest *rq,int &len)
	{
		if (header==NULL) {
			len = current_size = NBUFF_SIZE;
			header = (char *)malloc(current_size);
			hot = header;
			return hot;
		}
		assert(hot);
		unsigned used = hot - header;
		assert(used<=current_size);
		if (used>=current_size) {
			int new_size = current_size * 2;
			char *n = (char *)malloc(2 * current_size);
			memcpy(n,header,current_size);
			adjustBuffer(n - header);
			free(header);
			header = n;
			hot = header + current_size;
			current_size = new_size;
		}
		len = current_size - used;
		return hot;
	}
	//得到body缓冲,从upstream读
	char *getBodyBuffer(KHttpRequest *rq,int &len)
	{
		//读body的时候，用回header的缓冲
		len = current_size;
		assert(len>0);
		return header;
	}
	KClientSocket *getSocket()
	{
		return client;
	}
#ifdef _WIN32
	KSelectable *getBindData()
	{
		client->bindcpio_flag = true;
		return client;
	}
#endif
	SOCKET getSockfd()
	{
		if (client) {
			return client->get_socket();
		}
		return INVALID_SOCKET;
	}
	void connectCallBack(KHttpRequest *rq,KPoolableSocket *client,bool half_connection = true);
	void handleConnectError(KHttpRequest *rq,int error,const char *msg);
	void handleConnectResult(KHttpRequest *rq,int got);
	KSocketBuffer buffer;
	KPoolableSocket *client;
	BadStage badStage;
protected:
	//header重新分配过时要重新调整偏移量
	virtual void adjustBuffer(INT64 offset)
	{
	}
	//reopen调用
	virtual void reset()
	{
	}
	int lifeTime;
	char *header;
	char *hot;
	unsigned current_size;
	//创建发送头到buffer中。
	virtual void buildHead(KHttpRequest *rq) = 0;
	//解析head
	virtual Parse_Result parseHead(KHttpRequest *rq,char *data,int len) = 0;
	//创建post数据到buffer中。
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//检查是否还要继续读body,一般长连接需要。
	//如果本身有content-length则不用该函数
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	//读取body数据,返回 
	virtual char *nextBody(KHttpRequest *rq,int &len) = 0;
	//解析body
	virtual Parse_Result parseBody(KHttpRequest *rq,char *data,int len) = 0;
private:
	void continueReadBody(KHttpRequest *rq);
	void readPost(KHttpRequest *rq,bool useEvent=true);
	void sendPost(KHttpRequest *rq);
	int tryCount;
	void retryOpen(KHttpRequest *rq);
};
/////////[116]
#endif
