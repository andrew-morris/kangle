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
#ifndef KSELECTOR_H_
#define KSELECTOR_H_
#include "KSocket.h"
#include "KHttpRequest.h"
#include "KMutex.h"
#include "KSelectable.h"
#include "KServer.h"
#include "log.h"
#include "rbtree.h"
#define STAGE_OP_TIMER             0
#define STAGE_OP_READ              1
#define STAGE_OP_WRITE             2
#define STAGE_OP_NEXT              3
#define STAGE_OP_UPSTREAM_CONNECT  4
#define STAGE_OP_UPSTREAM_WHEAD    5
#define STAGE_OP_READ_POST         6
#define STAGE_OP_WRITE_POST        7
#define STAGE_OP_UPSTREAM_RHEAD    8
#define STAGE_OP_UPSTREAM_READ     9
#define STAGE_OP_TF_READ           10
#define STAGE_OP_TF_WRITE          11
#define STAGE_OP_PM_UREAD          12
#define STAGE_OP_PM_UWRITE         13
#define STAGE_OP_PM_READ           14
#define STAGE_OP_PM_WRITE          15
#define STAGE_OP_LISTEN            16
#define STAGE_OP_ASYNC_READ        17
#define STAGE_OP_TRANSMIT          17
#define STAGE_OP_BIG_HEADER        18
#define STAGE_OP_BIG_WRITE         19
#define STAGE_OP_BIG_CACHE_WRITE   20
#define STAGE_OP_UPSTREAM_SSLR     21
#define STAGE_OP_UPSTREAM_SSLW     22
#define IS_SECOND_OPERATOR(op)     (op==STAGE_OP_BIG_HEADER || op==STAGE_OP_BIG_WRITE)

#ifdef _WIN32
#define ASSERT_SOCKFD(a)        assert(a>=0)
#else
#define ASSERT_SOCKFD(a)        assert(a>=0 && a<81920)
#endif
FUNC_TYPE FUNC_CALL manageWorkThread(void *param);
FUNC_TYPE FUNC_CALL httpWorkThread(void *param);
FUNC_TYPE FUNC_CALL httpsWorkThread(void *param);
FUNC_TYPE FUNC_CALL oneWorkoneThread(void *param);

FUNC_TYPE FUNC_CALL stage_sync(void *param);
FUNC_TYPE FUNC_CALL stage_rdata(void *param);
void stage_prepare(KHttpRequest *rq);
#ifdef _WIN32
FUNC_TYPE FUNC_CALL stageRequest(void *param) ;
#endif
class KSelectable;
void handleAccept(KSelectable *st,int got);
void handleRequestRead(KSelectable *st,int got);
void handleRequestWrite(KSelectable *st,int got);
void handleRequestTempFileWrite(KSelectable *st,int got);
void handleStartRequest(KSelectable *st,int got);

void log_access(KHttpRequest *rq);
int checkHaveNextRequest(KHttpRequest *rq);
void stageEndRequest(KHttpRequest *rq);
#ifdef ENABLE_TF_EXCHANGE
void stageTempFileWriteEnd(KHttpRequest *rq);
#endif

class KSelector {
public:
	KSelector();
	virtual ~KSelector();
	//	void setMaxRequest(int maxRequest);
	//	bool addRequest(KClientSocket *socket,int model);
	virtual const char *getName() = 0;
	//void sslAccept(KHttpRequest *rq);
	/*
	其它线程调用，加list和addSocket为原子操作
	*/
	void addRequest(KHttpRequest *rq,int list,int op);
	void removeRequest(KHttpRequest *rq);
	virtual void select()=0;
	bool startSelect();
	friend class KHttpRequest;
	//update time
	bool utm;
	int tmo_msec;
	int sid;
	//超时时间，单位msec
	int timeout[KGL_LIST_BLOCK];
	void addBlock(KHttpRequest *rq,int op,INT64 sendTime);
	void addBlock(KHttpRequest *rq,KSelectable *st,int op,INT64 sendTime);
	virtual bool addListenSocket(KSelectable *st) {
		return false;
	}
	virtual void removeListenSocket(KSelectable *st) {
		
	}
	/*
	增加事件
	*/
	virtual bool addSocket(KSelectable *st,int op) = 0;	
	/*
	删除事件
	*/
	virtual void removeSocket(KSelectable *st) = 0;
#ifdef MALLOCDEBUG
	bool closeFlag;
#endif
	void addList(KHttpRequest *rq,int list);
protected:
	friend class KSelectorManager;
	friend class KAsyncFetchObject;
	KMutex listLock;
	RequestList requests[KGL_LIST_BLOCK];

	void checkTimeOut();
	int model;
	void internelAddRequest(KHttpRequest *rq);
private:	
	void removeList(KHttpRequest *rq);
	rb_root blockList;
	rb_node *blockBeginNode;
};
extern char serverData[];
#endif /*KSELECTOR_H_*/
