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
	KUpstreamSelectable *socket;
	KSockPoolHelper *sh;
};
using namespace std;
static void monitor_connect_result(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KSockPoolHelper *sph = (KSockPoolHelper *)rq->hot;
	if (got < 0 || TEST(rq->c->st_flags, STF_ERR)) {
		sph->disable();
	} else {
		if (rq->c->socket->write("H", 1) <= 0) {			
			sph->disable();
		} else {
			sph->enable();
		}
	}
	rq->c->socket->close();
	delete rq;
	sph->monitorNextTick();
}
static void WINAPI start_monitor_call_back(void *arg)
{
	KSockPoolHelper *sph = (KSockPoolHelper *)arg;
	sph->start_monitor_call_back();
}
static KTHREAD_FUNCTION asyncSockPoolDnsMonitorCallBack(void *data)
{
	KSockPoolDns *spdns = (KSockPoolDns *)data;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (kgl_current_msec - rq->active_msec > conf.connect_time_out * 1000
		|| !spdns->sh->real_connect(rq, spdns->socket)) {
		delete rq;
		spdns->socket->destroy();
		spdns->sh->disable();
		spdns->sh->monitorNextTick();
		delete spdns;
		KTHREAD_RETURN;
	}
	spdns->sh->monitorConnectStage(rq, spdns->socket);
	delete spdns;
}
static KTHREAD_FUNCTION asyncSockPoolDnsCallBack(void *data)
{
	KSockPoolDns *spdns = (KSockPoolDns *)data;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (kgl_current_msec - rq->active_msec > conf.connect_time_out * 1000
		|| !spdns->sh->real_connect(rq,spdns->socket)) {
		spdns->socket->isBad(BadStage_Connect);
		spdns->socket->destroy();
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
void KSockPoolHelper::monitorConnectStage(KHttpRequest *rq, KUpstreamSelectable *us)
{
	rq->c = us;
	rq->c->selector = selectorManager.getSelector();
	rq->hot = (char *)this;
	us->connect(rq, monitor_connect_result);
}
void KSockPoolHelper::start_monitor_call_back()
{
	if (!monitor) {
		release();
		return;
	}
	KHttpRequest *rq = new KHttpRequest(NULL);
	rq->init();
	bool need_name_resolved = false;
	KUpstreamSelectable *us = newConnection(rq, need_name_resolved);
	if (us == NULL) {
		disable();
		delete rq;
		monitorNextTick();
		return;
	}
	if (need_name_resolved) {
		KSockPoolDns *spdns = new KSockPoolDns;
		spdns->socket = us;
		spdns->rq = rq;
		spdns->sh = this;
		conf.dnsWorker->start(spdns, asyncSockPoolDnsMonitorCallBack);
		return;
	}
	monitorConnectStage(rq, us);
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
	weight = 1;
	monitor = false;
/////////[449]

}
KSockPoolHelper::~KSockPoolHelper() {
	/////////[450]
	if (ip) {
		free(ip);
	}
}
void KSockPoolHelper::monitorNextTick()
{
	selectorManager.getSelectorByIndex(0)->addTimer(NULL, ::start_monitor_call_back,this,this->error_try_time*1000);
}
void KSockPoolHelper::startMonitor()
{
	if (monitor) {
		return;
	}
	monitor = true;
	addRef();
	selectorManager.onReady(::start_monitor_call_back,this);
}
void KSockPoolHelper::checkActive()
{
	//TODO:现在采用最简单的线程去检测，以后可以加到主循环里面去检测，节省资源。
	if (monitor) {
		return;
	}
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
	sockaddr_i *bind_addr = NULL;
	if (ip) {
		bind_addr = new sockaddr_i;
		if (!KSocket::getaddr(ip,0,bind_addr,AF_UNSPEC,AI_NUMERICHOST)) {
			delete bind_addr;
			return;
		}
	}
	//TODO:已知问题,如果检测过程中对host修改，可能导致异常。所以未来一定要放到主循环中去检测
#ifdef KSOCKET_UNIX
	if (isUnix) {
		result = socket.connect(host.c_str(),tmo);
	} else {
#endif
		result = socket.connect(this->host.c_str(),this->port,tmo,bind_addr);
#ifdef KSOCKET_UNIX
	}
#endif
	if (bind_addr) {
		delete bind_addr;
	}
	if (result) {
		enable();
	}
}
KUpstreamSelectable *KSockPoolHelper::getConnection(KHttpRequest *rq, bool &half, bool &need_name_resolved)
{
	KUpstreamSelectable *socket = NULL;
	if (!TEST(rq->flags, RQ_UPSTREAM_ERROR)) {
		//如果是发生错误重连，则排除连接池
		socket = getPoolSocket();
		if (socket) {
			half = false;
			return socket;
		}
	}
	half = true;
	return newConnection(rq, need_name_resolved);
}
KUpstreamSelectable *KSockPoolHelper::newConnection(KHttpRequest *rq, bool &need_name_resolved)
{
	KUpstreamSelectable *socket = new KUpstreamSelectable();
/////////[451]
	socket->socket = new KClientSocket;
	bind(socket);
#ifdef KSOCKET_UNIX
	if (isUnix) {
		if(!socket->socket->halfconnect(host.c_str())){
			socket->destroy();
			return NULL;
		}
	} else {
#endif
		if (isIp) {
			if (!real_connect(rq,socket)) {
				socket->destroy();
				return NULL;
			}
		} else {
			need_name_resolved = true;
		}
#ifdef KSOCKET_UNIX
	}
#endif
	return socket;
}
void KSockPoolHelper::connect(KHttpRequest *rq)
{
	assert(rq->fetchObj);	
	bool half;
	bool need_name_resolved=false;
	KUpstreamSelectable *socket = getConnection(rq,half,need_name_resolved);
	if (!need_name_resolved || socket==NULL) {
		KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
		fo->connectCallBack(rq,socket,half);
		return;
	}
	//异步解析
	rq->c->removeRequest(rq);
	KSockPoolDns *spdns = new KSockPoolDns;
	addRef();
	spdns->socket = socket;
	spdns->rq = rq;
	spdns->sh = this;
	conf.dnsWorker->start(spdns,asyncSockPoolDnsCallBack);
}
bool KSockPoolHelper::real_connect(KHttpRequest *rq,KUpstreamSelectable *socket)
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
	result = socket->socket->halfconnect(addr,bind_addr,TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)>0);
	if (bind_addr) {
		delete bind_addr;
	}
/////////[452]
	return result;
}
bool KSockPoolHelper::setHostPort(std::string host,int port,const char *ssl)
{
	bool destChanged = false;
	lock.Lock();
	if(this->host != host || this->port!=port){
		destChanged = true;
		isIp = false;
		auto_detected_ip = false;
	}
	this->host = host;
	this->port = port;
	/////////[453]
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
/////////[454]
	setIp(attr["self_ip"].c_str());
	return true;
}
void KSockPoolHelper::buildXML(std::stringstream &s)
{
	s << " host='" ;
	if(isUnix){
		s << "unix:" << host ;
	} else {
		s << host << "' port='" << port ;
		/////////[455]
	}
	s << "' life_time='" << getLifeTime() << "' ";
/////////[456]
	if (ip && *ip) {
		s << "self_ip='" << ip << "' ";
	}
}
/////////[457]

