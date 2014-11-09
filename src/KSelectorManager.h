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
#ifndef KSELECTORMANAGER_H_
#define KSELECTORMANAGER_H_
#include<map>
#include<list>
#include<string>
#include "KSocket.h"
#include "KHttpRequest.h"
#include "KSelector.h"
#include "KMutex.h"
#include "KServer.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "KIpAclBase.h"
#define MAX_SELECTORS	2
#define ADD_REQUEST_SUCCESS                                     0
#define ADD_REQUEST_TOO_MANY_CONNECTION 	1
#define ADD_REQUEST_PER_IP_LIMIT                                2
#define ADD_REQUEST_UNKNOW_ERROR                        3
#define MAX_UNUSED_REQUEST                             1024
#define MAX_UNUSED_REQUEST_HASH                        1023
void handleMultiListen(KSelectable *st,int got);
typedef std::map<ip_addr,unsigned> intmap;
//#define RQ_LEAK_DEBUG               1
#ifdef RQ_LEAK_DEBUG
extern std::map<KHttpRequest *,bool> all_request;
#endif
extern intmap m_ip;
extern KMutex ipLock;
extern unsigned total_connect ;
struct KPerIpConnect
{
	IP_MODEL src;
	unsigned max;
	bool deny;
	KPerIpConnect *next; 
};
struct connect_per_ip_t {
        ip_addr ip;
        unsigned per_ip;
};
inline const char *getErrorMsg(int errorCode) {
        switch (errorCode) {
        case ADD_REQUEST_SUCCESS:
                return "success";
        case ADD_REQUEST_TOO_MANY_CONNECTION:
                return "too many connection";
        case ADD_REQUEST_PER_IP_LIMIT:
                return "per ip limit";
        }
        return "unknow error";
}
inline int addRequest(KHttpRequest *rq) {
        unsigned max = (unsigned)conf.max;
        if (max>0 && TEST(rq->workModel,WORK_MODEL_MANAGE)) {
                max += 100;
        }
		unsigned max_per_ip = conf.max_per_ip;
		bool deny = false;
		intmap::iterator it2;
		ip_addr ip;
        ipLock.Lock();
        if (max>0 && total_connect >= max) {
				//printf("total_connect=%d,max=%d\n",total_connect,max);
                ipLock.Unlock();
                return ADD_REQUEST_TOO_MANY_CONNECTION;
        }
		KPerIpConnect *per_ip = conf.per_ip_head;
		if (per_ip || max_per_ip>0) {
			rq->server->get_remote_addr(&ip);
		}
		while (per_ip) {
			if (KIpAclBase::matchIpModel(per_ip->src,ip)) {
				max_per_ip = per_ip->max;
				deny = per_ip->deny;
				break;
			}
			per_ip = per_ip->next;
		}
		if (deny) {
			goto max_per_ip;
		}
        if (max_per_ip==0) {
				total_connect++;
#ifdef RQ_LEAK_DEBUG
				all_request.insert(std::pair<KHttpRequest *,bool>(rq,true));
#endif
                ipLock.Unlock();
                SET(rq->workModel,WORK_MODEL_RQOK);
                return ADD_REQUEST_SUCCESS;
        }
       
        it2 = m_ip.find(ip);
        if (it2 == m_ip.end()) {
                m_ip.insert(std::pair<ip_addr, unsigned> (ip, 1));
        } else {
                if ((*it2).second >= max_per_ip) {
                        goto max_per_ip;
                }
                (*it2).second++;
        }
		total_connect++;
#ifdef RQ_LEAK_DEBUG
		all_request.insert(std::pair<KHttpRequest *,bool>(rq,true));
#endif
        ipLock.Unlock();
        SET(rq->workModel,WORK_MODEL_PER_IP|WORK_MODEL_RQOK);
        return ADD_REQUEST_SUCCESS;
        max_per_ip: ipLock.Unlock();
		/////////[335]
        return ADD_REQUEST_PER_IP_LIMIT;
}
inline void delRequest(KHttpRequest *rq) {
	if(!TEST(rq->workModel,WORK_MODEL_PER_IP)){
			if(TEST(rq->workModel,WORK_MODEL_RQOK)){
					ipLock.Lock();
					total_connect--;
#ifdef RQ_LEAK_DEBUG
					all_request.erase(rq);
#endif
					ipLock.Unlock();
			}
			return ;
	}
	ip_addr ip;
	rq->server->get_remote_addr(&ip);
	intmap::iterator it2;
	ipLock.Lock();
	total_connect--;
#ifdef RQ_LEAK_DEBUG
	all_request.erase(rq);
#endif
	it2 = m_ip.find(ip);
	assert(it2!=m_ip.end());
	(*it2).second--;
	if ((*it2).second == 0) {
			m_ip.erase(it2);
	}
	ipLock.Unlock();
}
class KSelectorManager
{
public:
	KSelectorManager();
	virtual ~KSelectorManager();
	bool addListenSocket(KServer *st);
	void destroy();
	void init(unsigned size);
	inline bool startRequest(KHttpRequest *rq)
	{
		if(rq->selector==NULL){
			rq->selector = selectors[(index++) & sizeHash];
		}
		rq->request_msec = kgl_current_msec;
		int ret = addRequest(rq);
		if (ret == ADD_REQUEST_SUCCESS) {
			rq->handler = handleRequestRead;
#ifdef KSOCKET_SSL
			if (TEST(rq->workModel,WORK_MODEL_SSL)) {
				KSSLSocket *socket = static_cast<KSSLSocket *>(rq->server);
				if (!socket->bind_fd()) {
					delete rq;
					return false;
				}
				rq->filter_flags = 0;
				//绑定rq到ssl上，SNI要用到，从SSL得到rq
				SSL_set_ex_data(socket->getSSL(),kangle_ssl_conntion_index,rq);
				rq->handler = handleSSLAccept;
			} else 
#endif
			rq->init();
#ifdef HTTP_PROXY
			if (TEST(rq->workModel,WORK_MODEL_PORTMAP)) {
				rq->meth = METH_CONNECT;
				handleStartRequest(rq,0);
				return true;
			}
#endif
			rq->selector->addRequest(rq,(conf.keep_alive>0?KGL_LIST_KA:KGL_LIST_RW),STAGE_OP_READ);
			return true;
		} else {
#ifndef _WIN32
#ifdef KSOCKET_SSL
		if (!TEST(rq->workModel,WORK_MODEL_SSL)) {
#endif
			//为什么windows不能发送呢？因为windows的socket是阻塞的。这里不能卡到。
			rq->server->write_all("HTTP/1.0 503 Service Unavailable\r\n\r\nServer is busy,try it again");
#ifdef KSOCKET_SSL
		}
#endif
#endif
			char ips[MAXIPLEN];
			rq->server->get_remote_ip(ips,sizeof(ips));
			klog(KLOG_ERR, "cann't addRequest to thread %s:%d %s\n",
					ips, rq->server->get_remote_port(),
					getErrorMsg(ret));
			delete rq;
			//closeRequest(rq);
			return false;
		}

	}
	void setTimeOut();
	int getSelectorCount()
	{
		return sizeHash+1;
	}
	void flush(time_t nowTime);
	void closeKeepAliveConnection();
	const char *getName()
	{
		return selectors[0]->getName();
	}
	static KSelector *newSelector();
	void start();
	bool isInit()
	{
		return selectors!=NULL;
	}
public:
	std::string getConnectionInfo(int &totalCount,int debug,const char *vh_name,bool translate=true);
	//void sortRequest(KHttpRequest *rq,std::list<KHttpRequest *> &sortRequest,int sortby);
private:
	void getConnectionTr(KHttpRequest *rq,std::stringstream &s,time_t now_time,bool translate=true);
	KSelector **selectors;
	int count;
	unsigned sizeHash;
	unsigned listenIndex;
	unsigned index;
	KMutex unusedLock;
	KHttpRequest *unusedRequest;
};
extern KSelectorManager selectorManager;
#endif /*KSELECTORMANAGER_H_*/
