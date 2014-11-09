/*
 * KSockPoolHelper.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
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
#include <sstream>
#include "KSockPoolHelper.h"
#include "utils.h"
#include "KAsyncFetchObject.h"
#include "KThreadPool.h"
struct KSockPoolDns
{
	KHttpRequest *rq;
	KPoolableSocket *socket;
	KSockPoolHelper *sh;
	bool isSSL;
};
using namespace std;
static KTHREAD_FUNCTION asyncSockPoolDnsCallBack(void *data)
{
	KSockPoolDns *spdns = (KSockPoolDns *)data;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (kgl_current_msec - rq->active_msec > conf.time_out * 1000 
		|| !spdns->sh->real_connect(rq,spdns->socket,spdns->isSSL)) {
		delete spdns->socket;
		spdns->socket = NULL;
	}
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->connectCallBack(rq,spdns->socket,true);
	spdns->sh->release();
	delete spdns;
	KTHREAD_RETURN;
}
FUNC_TYPE FUNC_CALL checkNodeActive(void *param)
{
	KSockPoolHelper *sockHelper = (KSockPoolHelper *)param;
	sockHelper->syncCheckConnect();
	sockHelper->release();
	KTHREAD_RETURN;
}
KSockPoolHelper::KSockPoolHelper() {
	auto_detected_ip = false;
	isIp = false;
	ip = NULL;
	tryTime = 0;
	error_count = 0;
	isUnix = false;
	max_error_count = 5;
	hit = 0;
/////////[375]

}
KSockPoolHelper::~KSockPoolHelper() {
	/////////[376]
	if (ip) {
		free(ip);
	}
}
void KSockPoolHelper::checkActive()
{
	//TODO:现在采用最简单的线程去检测，以后可以加到主循环里面去检测，节省资源。
	addRef();
	if (!m_thread.start(this,checkNodeActive)) {
		release();
	}
}
void KSockPoolHelper::syncCheckConnect()
{
	KClientSocket socket;
	bool result = false;
	int tmo = 5;
	//TODO:已知问题,如果检测过程中对host修改，可能导致异常。所以未来一定要放到主循环中去检测
#ifdef KSOCKET_UNIX
	if (isUnix) {
		result = socket.connect(host.c_str(),tmo);
	} else {
#endif
		result = socket.connect(this->host.c_str(),this->port,tmo);
#ifdef KSOCKET_UNIX
	}
#endif
	if (result) {
		enable();
	}
}
KPoolableSocket *KSockPoolHelper::getConnection(KHttpRequest *rq,bool &half,bool &need_name_resolved,bool &isSSL)
{
	int sid = 0;
	KPoolableSocket *socket = NULL;
	if (!TEST(rq->flags,RQ_UPSTREAM_ERROR)) {
		//如果是发生错误重连，则排除连接池
		 socket = getPoolSocket();
		 if (socket) {
			half = false;
			return socket;
		}
	}
	isSSL = false;
/////////[377]
		socket = new KPoolableSocket();
	bind(socket);
#ifdef KSOCKET_UNIX
	if (isUnix) {
		if(!socket->halfconnect(host.c_str())){
			delete socket;
			return NULL;
		}
	} else {
#endif
		if (isIp) {
			if (!real_connect(rq,socket,isSSL)) {
				delete socket;
				return NULL;
			}
		} else {
			need_name_resolved = true;
		}
#ifdef KSOCKET_UNIX
	}
#endif
	half = true;
	return socket;
}
void KSockPoolHelper::connect(KHttpRequest *rq)
{
	assert(rq->fetchObj);	
	bool half;
	bool need_name_resolved=false;
	bool isSSL;
	KPoolableSocket *socket = getConnection(rq,half,need_name_resolved,isSSL);
	if (!need_name_resolved || socket==NULL) {
		KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
		fo->connectCallBack(rq,socket,half);
		return;
	}
	//异步解析
	rq->selector->removeRequest(rq);
	KSockPoolDns *spdns = new KSockPoolDns;
	addRef();
	spdns->socket = socket;
	spdns->rq = rq;
	spdns->sh = this;
	spdns->isSSL = isSSL;
	conf.dnsWorker->start(spdns,asyncSockPoolDnsCallBack);
}
bool KSockPoolHelper::real_connect(KHttpRequest *rq,KPoolableSocket *socket,bool isSSL)
{
	sockaddr_i addr;
	bool try_ip = false;
	if (!isIp && !auto_detected_ip) {
		//try
		try_ip = true;
	}
	bool result = KSocket::getaddr(this->host.c_str(),this->port,&addr,AF_UNSPEC,((try_ip||isIp)?AI_NUMERICHOST:0));
	if (try_ip) {
		auto_detected_ip = true;
		if (result) {
			this->isIp = true;
		} else {
			result = KSocket::getaddr(this->host.c_str(),this->port,&addr,AF_UNSPEC,0);
		}
	}
	if (!result) {
		return false;
	}
	sockaddr_i *bind_addr = NULL;
	const char *bind_ip = ip?ip:rq->bind_ip;
	if (bind_ip) {
		bind_addr = new sockaddr_i;
		if (!KSocket::getaddr(bind_ip,0,bind_addr,AF_UNSPEC,AI_NUMERICHOST)) {
			delete bind_addr;
			return false;	
		}
	}
	result = socket->halfconnect(addr,bind_addr,TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)>0);
	if (bind_addr) {
		delete bind_addr;
	}
/////////[378]
	return result;
}
bool KSockPoolHelper::setHostPort(std::string host,int port,const char *ssl)
{
	lock.Lock();
	auto_detected_ip = false;
	bool destChanged = false;
	if(this->host != host || this->port!=port){
		destChanged = true;
	}
	this->host = host;
	this->port = port;
	/////////[379]
	if (destChanged) {
		//clean();
	}
#ifdef KSOCKET_UNIX
	isUnix = false;
	if(strncasecmp(this->host.c_str(),"unix:",5)==0){
		isUnix = true;
		this->host = this->host.substr(5);
	}
#endif
	lock.Unlock();
	return true;
}
bool KSockPoolHelper::setHostPort(std::string host, const char *port) {
	return setHostPort(host,atoi(port),strchr(port,'s'));
}
void KSockPoolHelper::disable() {
	if (error_try_time==0) {
		tryTime = kgl_current_sec + ERROR_RECONNECT_TIME;
	} else {
		tryTime = kgl_current_sec + error_try_time;
	}
}
bool KSockPoolHelper::isEnable() {
	if (tryTime == 0) {
		return true;
	}
	if (tryTime < kgl_current_sec) {
		tryTime += MAX(error_try_time,10);
		checkActive();
		return false;
	}
	return false;
}
void KSockPoolHelper::enable() {
	tryTime = 0;
	error_count = 0;
}
bool KSockPoolHelper::parse(std::map<std::string,std::string> &attr)
{
	setHostPort(attr["host"],attr["port"].c_str());
	setLifeTime(atoi(attr["life_time"].c_str()));
/////////[380]
	return true;
}
void KSockPoolHelper::buildXML(std::stringstream &s)
{
	s << " host='" ;
	if(isUnix){
		s << "unix:" << host ;
	} else {
		s << host << "' port='" << port ;
		/////////[381]
	}
	s << "' life_time='" << getLifeTime() << "' ";
/////////[382]

}
/////////[383]

