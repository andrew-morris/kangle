/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kanglesoft.com/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "KServerListen.h"
#include "log.h"
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "malloc_debug.h"
/////////[184]
using namespace std;
inline void serverListenWork(KServer *server)
{
#ifdef _WIN32
	bool noblocking = false;
#endif
	KClientSocket *socket;
#ifdef KSOCKET_SSL
	if(TEST(server->model,WORK_MODEL_SSL)){
		socket = new KSSLSocket(server->ssl_ctx);
#ifdef _WIN32
		//windows ssl 要工作在非阻塞模式
		noblocking = true;
#endif
	} else 
#endif
		socket = new KClientSocket;

#ifdef _WIN32
	if (!server->server.accept(socket,noblocking)) {
#else
	if (!server->server.accept(socket,true)) {
#endif
		klog(KLOG_ERR, "cann't accept connect,errno=%s\n", strerror(errno));		
		delete socket;
	} else {
#ifndef NDEBUG
		//klog(KLOG_DEBUG,"new client %s:%d connect to %s:%d sockfd=%d\n", socket->get_remote_ip().c_str(), socket->get_remote_port(),socket->get_self_ip().c_str(),socket->get_self_port(),socket->get_socket());
#endif
		KHttpRequest *rq = new KHttpRequest;
		rq->workModel = server->model;
		rq->ls = server;
		server->addRef();
		rq->server = socket;
		selectorManager.startRequest(rq);
	}
#ifdef SOLARIS
	//solaris port need every time call addSocket
	st->selector->addListenSocket(st);
#endif
}
void handleServerListen(KSelectable *st,int got)
{
	KServer *server = static_cast<KServer *>(st);
#ifndef _WIN32
  	if (server->isClosed()) {
		st->selector->removeSocket(st);
		server->started = false;
		server->release();
		return ;
	}	
#endif
	serverListenWork(server);
}
void handleMultiListen(KSelectable *st,int got)
{
	//KMultiServer *ms = static_cast<KMultiServer *>(st);
	//serverListen(ms->server,ms->selector);	
}
KServerListen::KServerListen() {
}
KServerListen::~KServerListen() {
}
void KServerListen::start(std::vector<KServer *> &serverList) {
	vector<KServer *>::iterator it;
	for (it = serverList.begin(); it != serverList.end();) {

		if (!start(*it)) {
			it = serverList.erase(it);
		} else {
			it++;
		}
	}
}
bool KServerListen::start(KServer *server) {
#ifdef _WIN32
	/////////[185]
#else
	server->handler = handleServerListen;
#endif
	bool result = false;
	server->addRef();
	if ((server->dynamic||server->event_driven) 
		&& conf.worker==1) {
		//动态侦听端口，或端口映射类型，或管理端口，使用事件驱动
		result = selectorManager.addListenSocket(server);
	}
	server->started = true;
	if (!result) {
		//启动线程侦听
		if (!m_thread.start((void *) server, KServerListen::serverThread)) {
			klog(KLOG_ERR, "cann't start serverThread,errno=%d\n", errno);
			server->release();
			server->started = false;
			return false;
		}		
	}
/////////[186]
	return true;
}
FUNC_TYPE FUNC_CALL KServerListen::serverThread(void *param) {
	KServer *server = (KServer *) param;
	for (;;) {
		if (server->isClosed()) {
			server->started = false;
			server->release();
			break;
		}
		serverListenWork(server);
	}
	KTHREAD_RETURN;
}
