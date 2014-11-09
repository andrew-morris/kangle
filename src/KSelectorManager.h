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
struct KOnReadyItem
{
	void (WINAPI *call_back)(void *arg);
	void *arg;
	KOnReadyItem *next;
};
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
inline int addRequest(KConnectionSelectable *c) {
        unsigned max = (unsigned)conf.max;
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
			c->socket->get_remote_addr(&ip);
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
                ipLock.Unlock();
                SET(c->st_flags,STF_RQ_OK);
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
        ipLock.Unlock();
        SET(c->st_flags,STF_RQ_OK|STF_RQ_PER_IP);
        return ADD_REQUEST_SUCCESS;
        max_per_ip: ipLock.Unlock();
		/////////[413]
        return ADD_REQUEST_PER_IP_LIMIT;
}
inline void delRequest(u_short workModel,KClientSocket *socket) {
	if(!TEST(workModel,STF_RQ_PER_IP)){
			if(TEST(workModel,STF_RQ_OK)){
					ipLock.Lock();
					total_connect--;
					ipLock.Unlock();
			}
			return ;
	}
	ip_addr ip;
	socket->get_remote_addr(&ip);
	intmap::iterator it2;
	ipLock.Lock();
	total_connect--;
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
	bool listen(KServer *st,resultEvent result);
	void destroy();
	void init(unsigned size);
	inline KSelector *getSelectorByIndex(int index)
	{
		return selectors[index & sizeHash];
	}
	inline KSelector *getSelector()
	{
		return selectors[(index++) & sizeHash];
	}
	void adjustTime(INT64 t)
	{
		for (int i=0;i<count;i++) {
			selectors[i]->adjustTime(t);
		}
	}
	inline bool startRequest(KConnectionSelectable *c)
	{
		if (c->selector == NULL){
			getSelector()->bindSelectable(c);
		}
		int ret = addRequest(c);
		if (ret == ADD_REQUEST_SUCCESS) {
#ifdef KSOCKET_SSL
			if (c->isSSL()) {
				KSSLSocket *socket = static_cast<KSSLSocket *>(c->socket);
				if (!socket->bind_fd()) {
					c->destroy();
					return false;
				}
				//SET(c->st_flags, STF_SSL | STF_ET);
				KHttpRequest *rq = new KHttpRequest(c);
				rq->workModel = c->ls->model;
				rq->filter_flags = 0;
				//绑定rq到ssl上，SNI要用到，从SSL得到rq
				SSL_set_ex_data(socket->getSSL(), kangle_ssl_conntion_index, c);
				c->read(rq, resultSSLAccept, NULL, (conf.keep_alive>0 ? KGL_LIST_KA : KGL_LIST_RW));
			}
			else {
#endif
				KHttpRequest *rq = new KHttpRequest(c);
				rq->workModel = c->ls->model;
				rq->init();
				c->read(rq, resultRequestRead, bufferRequestRead, (conf.keep_alive>0 ? KGL_LIST_KA : KGL_LIST_RW));
#ifdef KSOCKET_SSL
			}
#endif
			return true;
		}
		else {
#ifndef _WIN32
#ifdef KSOCKET_SSL
			if (!c->isSSL()) {
#endif
				//为什么windows不能发送呢？因为windows的socket是阻塞的。这里不能卡到。
				c->socket->write_all("HTTP/1.0 503 Service Unavailable\r\n\r\nServer is busy,try it again");
#ifdef KSOCKET_SSL
			}
#endif
#endif
			char ips[MAXIPLEN];
			c->socket->get_remote_ip(ips, sizeof(ips));
			klog(KLOG_ERR, "cann't addRequest to thread %s:%d %s\n",
				ips, c->socket->get_remote_port(),
				getErrorMsg(ret));
			c->destroy();
			return false;
		}

	}
#if 0
	inline bool startRequest(KHttpRequest *rq)
	{
		if(rq->c->selector==NULL){
			getSelector()->bindSelectable(rq->c);
		}
		rq->request_msec = kgl_current_msec;
		int ret = addRequest(rq->c);
		if (ret == ADD_REQUEST_SUCCESS) {
#ifdef KSOCKET_SSL
			if (TEST(rq->workModel,WORK_MODEL_SSL)) {
				KSSLSocket *socket = static_cast<KSSLSocket *>(rq->c->socket);
				if (!socket->bind_fd()) {
					delete rq;
					return false;
				}
				SET(rq->c->st_flags,STF_SSL|STF_ET);
				rq->filter_flags = 0;
				//绑定rq到ssl上，SNI要用到，从SSL得到rq
				SSL_set_ex_data(socket->getSSL(),kangle_ssl_conntion_index,rq->c);
				rq->c->read(rq,resultSSLAccept,NULL,(conf.keep_alive>0?KGL_LIST_KA:KGL_LIST_RW));
			} else {
#endif
				rq->init();
				rq->c->read(rq,resultRequestRead,bufferRequestRead,(conf.keep_alive>0?KGL_LIST_KA:KGL_LIST_RW));
#ifdef KSOCKET_SSL
			}
#endif
			return true;
		} else {
#ifndef _WIN32
#ifdef KSOCKET_SSL
		if (!TEST(rq->workModel,WORK_MODEL_SSL)) {
#endif
			//为什么windows不能发送呢？因为windows的socket是阻塞的。这里不能卡到。
			rq->c->socket->write_all("HTTP/1.0 503 Service Unavailable\r\n\r\nServer is busy,try it again");
#ifdef KSOCKET_SSL
		}
#endif
#endif
			char ips[MAXIPLEN];
			rq->c->socket->get_remote_ip(ips,sizeof(ips));
			klog(KLOG_ERR, "cann't addRequest to thread %s:%d %s\n",
					ips, rq->c->socket->get_remote_port(),
					getErrorMsg(ret));
			delete rq;
			//closeRequest(rq);
			return false;
		}

	}
#endif
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
	void onReady(void (WINAPI *call_back)(void *arg),void *arg);
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
	KOnReadyItem *onReadyList;
};
extern KSelectorManager selectorManager;
#endif /*KSELECTORMANAGER_H_*/
