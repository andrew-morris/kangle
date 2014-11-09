/*
 * KServer.cpp
 *
 *  Created on: 2010-5-5
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

#include <stdlib.h>
#include <vector>
#include "KServer.h"
#include "log.h"
#include "malloc_debug.h"
#include "do_config.h"
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "forwin32.h"
#include "extern.h"
#include "KHttpRequest.h"
#include "KNsVirtualHost.h"
#include "malloc_debug.h"
/////////[38]
KVirtualHostContainer KServer::defaultVhc;
#ifdef ENABLE_BASED_IP_VH
KIpVirtualHost KServer::ipVhc;
#endif
KServer::KServer() {
#ifdef KSOCKET_SSL
    ssl_ctx = NULL;
	sslParsed = false;
#endif
	closed = false;
	started = false;
/////////[39]
	vhc = NULL;
	dynamic = false;
	selector = false;
}

KServer::~KServer() {
#ifdef KSOCKET_SSL
        if (ssl_ctx) {
                KSSLSocket::clean_ctx(ssl_ctx);
        }
#endif

/////////[40]
	if(vhc){
		delete vhc;
	}
}
void KServer::close() {
	closed = true;
	server.close();
}
#ifdef KSOCKET_SSL
bool KServer::load_ssl()
{
	if (!sslParsed) {
		//多进程模型ssl证书查找
		std::vector<KListenHost *>::iterator it;
		int port = server.get_self_port();
		for (it=conf.service.begin();it!=conf.service.end();it++) {
			//优先根据name查找，如果name为空，则查找port.
			if( model==(*it)->model
				&& (name.size()>0 && name==(*it)->name) || (name.size()==0 && port==(*it)->port) ) {
				certificate = (*it)->certificate;
				certificate_key = (*it)->certificate_key;
				break;
			}
		
		}
	}
	std::string certFile = certificate;
	if(certFile.size()>0 && !isAbsolutePath(certFile.c_str())){
		certFile = conf.path + certificate;
	}
	std::string privateKeyFile = certificate_key;
	if(privateKeyFile.size()>0 && !isAbsolutePath(privateKeyFile.c_str())){
		privateKeyFile = conf.path + certificate_key;
	}
	if (certFile.size()>0) {
		ssl_ctx = KSSLSocket::init_server(
						(certFile.size() > 0 ? certFile.c_str() : NULL),
						 privateKeyFile.c_str(),
						NULL);
		if (ssl_ctx == NULL) {
			klog(KLOG_ERR,
					"Cann't init ssl context certificate=[%s],certificate_key=[%s]\n",
					certificate.c_str(), certificate_key.c_str());
			return false;
		}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		if(sni && 0 == SSL_CTX_set_tlsext_servername_callback(ssl_ctx,httpSSLServerName)){
			 klog(KLOG_WARNING, "kangle was built with SNI support, however, now it is linked "
					"dynamically to an OpenSSL library which has no tlsext support, "
					"therefore SNI is not available");
		}
#endif
		return true;
	}
	return false;
}
#endif
void KServer::bindVirtualHost(KVirtualHost *vh)
{
	if (vhc==NULL) {
		vhc = new KVirtualHostContainer;
	}		
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		if(!vhc->bindVirtualHost((*it2)) ) {
			(*it2)->allSuccess = false;
		}
	}
}
void KServer::addVirtualHost(KVirtualHost *vh)
{	
#ifndef HTTP_PROXY
	u_short port = server.get_self_port();
	std::list<u_short>::iterator it;
	for(it=vh->ports.begin();it!=vh->ports.end();it++){
		if(port == (*it)){
			bindVirtualHost(vh);
			return;
		}
	}
	std::list<std::string>::iterator it2;
	for(it2=vh->binds.begin();it2!=vh->binds.end();it2++){
		const char *bind = (*it2).c_str();
		if (*bind=='@' && strcmp(name.c_str(),bind+1)==0) {
			bindVirtualHost(vh);
			return;
		}
	}
	return;
#endif
}
void KServer::removeVirtualHost(KVirtualHost *vh)
{

#ifndef HTTP_PROXY
	if(vhc==NULL){
		return;
	}
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc->unbindVirtualHost((*it2));
	}
	return;
#endif
	
}
void KServer::clear()
{
	if (vhc) {
		delete vhc;
		vhc = NULL;
	}
}
bool KServer::isEmpty()
{
	if (vhc==NULL) {
		return true;
	}
	return vhc->isEmpty();
}
void KServer::unbindAllVirtualHost()
{
	if (vhc) {
		delete vhc;
		vhc = NULL;
	}
}
void KServer::addDefaultVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_BASED_IP_VH
	ipVhc.addVirtualHost(vh);
#endif
#ifndef HTTP_PROXY
	std::list<u_short>::iterator it;
	std::list<KSubVirtualHost *>::iterator it2;
	if (vh->ports.size()==0 && vh->binds.size()==0) {
		for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
			if(!defaultVhc.bindVirtualHost((*it2)) ) {
				(*it2)->allSuccess = false;
			}
		}
		return;
	}
	for(it=vh->ports.begin();it!=vh->ports.end();it++){
		if(0 == (*it)){
			for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
				if(!defaultVhc.bindVirtualHost((*it2)) ) {
					(*it2)->allSuccess = false;
				}
			}
			return;
		}
	}
	return;
#endif
}
query_vh_result KServer::parseVirtualHost(KHttpRequest *rq,const char *site)
{
	return defaultVhc.parseVirtualHost(rq,site);
}
void KServer::removeDefaultVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_BASED_IP_VH
	ipVhc.removeVirtualHost(vh);
#endif
#ifndef HTTP_PROXY
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		defaultVhc.unbindVirtualHost((*it2));
	}
	return;
#endif
}
/////////[41]
