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
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "log.h"
#include "lang.h"
#include "malloc_debug.h"
#include "KEpollSelector.h"
#include "KKqueueSelector.h"
#include "KPortSelector.h"
/////////[360]
#include "KVirtualHost.h"
#include "time_utils.h"
intmap m_ip;
KMutex ipLock;
std::map<KHttpRequest *,bool> all_request;
unsigned total_connect = 0;
KSelectorManager selectorManager;
using namespace std;
void adjustTime(INT64 diffTime)
{
	selectorManager.adjustTime(diffTime);
}
string get_connect_per_ip() {

	stringstream s;
	list<connect_per_ip_t> m_per_ip;
	list<connect_per_ip_t>::iterator it;
	bool have_insert = false;
	connect_per_ip_t m_tmp;
	s
			<< "<html><head><LINK href=/kangle.css type='text/css' rel=stylesheet></head><body>";
	s << LANG_MAX_CONNECT_PER_IP << conf.max_per_ip;
	int total = 0;
	ipLock.Lock();
	intmap::iterator it2;
	for (it2 = m_ip.begin(); it2 != m_ip.end(); it2++) {
		have_insert = false;
		m_tmp.ip = (*it2).first;
		m_tmp.per_ip = (*it2).second;
		total += m_tmp.per_ip;
		for (it = m_per_ip.begin(); it != m_per_ip.end(); it++) {
			if (m_tmp.per_ip > (*it).per_ip) {
				m_per_ip.insert(it, m_tmp);
				have_insert = true;
				break;
			}
		}
		if (!have_insert) {
			m_per_ip.push_back(m_tmp);
		}
	}
	s << "<table border=1><tr><td>" << LANG_IP << "</td><td>"
			<< LANG_CONNECT_COUNT << "</td></tr>";
	for (it = m_per_ip.begin(); it != m_per_ip.end(); it++) {

		s << "<tr";
		/*if ((*it).per_ip > conf.max_per_ip) {
			s << " bgcolor=#CCFF99";
		}
		*/
		char ip[MAXIPLEN];
		KSocket::make_ip(&(*it).ip,ip,sizeof(ip));
		s << "><td>" << ip << "</td><td>" << (*it).per_ip << "</td>";
		s << "</tr>";
	}
	ipLock.Unlock();
	s << "</table>\n" ;
	s << "<!-- total connect = " << total << " -->\n";
	s << endTag() << "</body></html>";
	return s.str();
	//	return "";
}

int set_max_per_ip(int value) {
	conf.max_per_ip = value;
	return value;
}
KSelector *KSelectorManager::newSelector() {
#ifdef HAVE_SYS_EPOLL_H
	return new KEpollSelector();
#elif BSD_OS
	return new KKqueueSelector();
#elif HAVE_PORT_H
	return new KPortSelector();
	/////////[361]
#else
	return NULL;
#endif
}
KSelectorManager::KSelectorManager() {
	selectors = NULL;
	listenIndex = 0;
	unusedRequest = NULL;
	sizeHash = 0;
	onReadyList = NULL;
}
void KSelectorManager::init(unsigned size)
{
/////////[362]
	//保证listenIndex为2的n次方，最大为512.
	for (int i=0;i<9;i++) {
		count = (1<<i);
		if(count>=(int)size){
			break;
		}
		
	}
	sizeHash = count - 1;
	listenIndex = sizeHash;
	//printf("sizeHash = %d\n",sizeHash);
#ifndef _WIN32
	if (conf.worker<=1 && conf.select_count>1) {
		count++;
		listenIndex++;
	}
#endif
	selectors = (KSelector **)xmalloc(sizeof(KSelector *)*count);
	for(int i=0; i<count; i++){
		selectors[i] = newSelector();
		selectors[i]->sid = i;
	}
	setTimeOut();
	//call onReadyList
	while (onReadyList) {
		onReadyList->call_back(onReadyList->arg);
		KOnReadyItem *n = onReadyList->next;
		delete onReadyList;
		onReadyList = n;
	}
}
void KSelectorManager::start()
{
	for(int i=0;i<count;i++){
	      selectors[i]->startSelect();
	}
}
void KSelectorManager::setTimeOut()
{
	if (conf.connect_time_out <= 0) {
		conf.connect_time_out = conf.time_out;
	}
	for (int i=0; i<count; i++) {
		//设置超时时间
		selectors[i]->timeout[KGL_LIST_KA] = (conf.keep_alive>0?conf.keep_alive:conf.time_out) * 1000;
		selectors[i]->timeout[KGL_LIST_RW] = conf.time_out * 1000;
		selectors[i]->timeout[KGL_LIST_CONNECT] = conf.connect_time_out * 1000;
		selectors[i]->tmo_msec = 100;
	}
	selectors[listenIndex]->utm = true;
	
}
void KSelectorManager::closeKeepAliveConnection()
{
	for(int i=0; i<count; i++){
		selectors[i]->timeout[KGL_LIST_KA] = 0;
	}
}
KSelectorManager::~KSelectorManager() {

}
void KSelectorManager::destroy() {
#ifdef MALLOCDEBUG
	for(int i=0; i<count; i++){
		selectors[i]->closeFlag = true;
	}
	sleep(1);
	xfree(selectors);
#endif
}
void KSelectorManager::flush(time_t nowTime) {
/////////[363]
}

bool KSelectorManager::listen(KServer *st,resultEvent result)
{
	st->selector = selectors[listenIndex];
	return selectors[listenIndex]->listen(st,result);
}
std::string KSelectorManager::getConnectionInfo(int &totalCount, int debug,const char *vh_name,bool translate) {
	time_t now_time = kgl_current_sec;
	totalCount = 0;
	stringstream s;
	if (debug==2) {
#ifdef RQ_LEAK_DEBUG
		ipLock.Lock();
		std::map<KHttpRequest *,bool>::iterator it;
		for(it=all_request.begin();it!=all_request.end();it++){
			s << "rqs.push(new Array(";
			getConnectionTr((*it).first,s,now_time);
			s << "));\n";
			totalCount++;
		}
		ipLock.Unlock();
#endif
	} else if(debug==3) {
#ifdef RQ_LEAK_DEBUG
		ipLock.Lock();
		std::map<KHttpRequest *,bool>::iterator it;
		for(it=all_request.begin();it!=all_request.end();it++){
			s << (KSelectable *)((*it).first) << "\n";
			totalCount++;
		}
		ipLock.Unlock();
#endif
	} else {
		for (unsigned i=0;i<=sizeHash;i++) {
			KSelector *selector = selectors[i];
			selector->listLock.Lock();
			for(int l = 0;l<KGL_LIST_BLOCK;l++){
				//selector->listLock[l].Lock();
				KHttpRequest *rq = selector->requests[l].getHead();
				//s << "//selector=" << l << "\n";
				while (rq) {
					if (vh_name == NULL || (rq->svh && strcmp(rq->svh->vh->name.c_str(),vh_name)==0)){
						s << "rqs.push(new Array(";
						getConnectionTr(rq,s,now_time,translate);
						s << "));\n";
						totalCount++;
					}
					rq = rq->next;
				}
				//selector->listLock[l].Unlock();
			}
			s << "//selector=block;\n";
			//selector->listLock[KGL_LIST_BLOCK].Lock();
			rb_node *node = selector->blockBeginNode;
			while (node) {
				KBlockRequest *brq = (KBlockRequest *)node->data;
				KHttpRequest *rq = brq->rq;
				if (rq) {
				/////////[364]
						if (vh_name == NULL || (rq->svh && strcmp(rq->svh->vh->name.c_str(),vh_name)==0)){
							s << "rqs.push(new Array(";
							getConnectionTr(rq,s,now_time,translate);
							s << "));\n";
							totalCount++;
						}	
/////////[365]
				}
				node = rb_next(node);
			}			
			selector->listLock.Unlock();
		}
	}

	return s.str();
}
void KSelectorManager::getConnectionTr(KHttpRequest *rq, stringstream &s,
		time_t now_time,bool translate) {
	//s << "<tr><td>";
	
	s << "' ";
	if (translate) {
		s << "rq=" << (void *)(KSelectable *)rq->c << ",st_flags=" << (int)rq->c->st_flags;
		s << ",meth=" << (int)rq->meth;
#ifdef ENABLE_REQUEST_QUEUE
		s << ",queue=" << rq->queue;
#endif
		s << ",list=" << (int)rq->list;
		s << ",last=" << rq->active_msec;
	}
	s << "','";
	char ips[MAXIPLEN];
	rq->c->socket->get_remote_ip(ips,sizeof(ips));
	s << ips << ":"	<< rq->c->socket->get_remote_port();
	s << "','" << (now_time - rq->request_msec/1000);
	s << "','";
	if (translate) {
		s << klang[rq->getState()];
	} else {
		s << rq->getState();
	}
	s << "','";
	s << rq->getMethod();
	if (rq->mark!=0) {
		s << " " << (int)rq->mark;
	}
	s << "','";
	string url = rq->getInfo();
	if (url.size() > 0) {
		if (url.find('\'')!=string::npos) {
			s << "bad url";
		} else {
			bool href = false;
			if(strncasecmp(url.c_str(),"http://",7)==0 || strncasecmp(url.c_str(),"https://",8)==0){
				href = true;
			}
			if(href){
				s << "<a title=\\'" << url << "\\' href=\"" << url << "\" target=_blank>" ;
			}
			if(url.size()>80){
				s << url.substr(0,80) << "...";
			}else{
				s << url;
			}
			if(href){
				s << "</a>";
			}
		}
	} else {
		s << klang["wait"] << "...";
	}
	s << "','";
	if (!translate) {
		sockaddr_i self_addr;
		rq->c->socket->get_self_addr(&self_addr);
		KSocket::make_ip(&self_addr,ips,sizeof(ips));
		s << ips << ":" << self_addr.get_port();
	}
	s << "'";
}
void KSelectorManager::onReady(void (WINAPI *call_back)(void *arg),void *arg)
{
	if (isInit()) {
		call_back(arg);
		return;
	}
	KOnReadyItem *onReadyItem = new KOnReadyItem();
	onReadyItem->call_back = call_back;
	onReadyItem->arg = arg;
	onReadyItem->next = onReadyList;
	onReadyList = onReadyItem;
}
