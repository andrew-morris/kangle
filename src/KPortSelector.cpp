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
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include "global.h"
#include "KPortSelector.h"
#ifdef HAVE_PORT_H
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>

#include "utils.h"
#include "log.h"
#include "KThreadPool.h"
#include "malloc_debug.h"
#include "KHttpRequest.h"
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "time_utils.h"
#include "malloc_debug.h"
#include "KUpstreamFetchObject.h"
#include "KPipeMessageFetchObject.h"
#define MAXEVENT	16
KPortSelector::KPortSelector() {
	kdpfd = port_create() ;
}
KPortSelector::~KPortSelector() {
	close(kdpfd);
}
bool KPortSelector::addListenSocket(KSelectable *st) {
	addSocket(st,STAGE_OP_LISTEN);
	return true;
}

void KPortSelector::select() {
	port_event_t events; 
	struct timespec tm;
	tm.tv_sec = tmo_msec/1000;
	tm.tv_nsec = tmo_msec * 1000 - tm.tv_sec * 1000000;
	for (;;) {
		checkTimeOut();
		int ret = port_get(kdpfd,&events,&tm);
		if(utm){
			updateTime();
		}
		if(ret==-1){
			continue;
		}
		KSelectable *st = (KSelectable *)events.portev_user;
#ifndef NDEBUG
		st->hs.clear();
#endif
		st->handler(st,0);
	}
}
bool KPortSelector::addSocket(KSelectable *st,int op) {
        int ev = 0;
        SOCKET sockfd = -1;
	st->sockop = 0;
	switch(op){
        case STAGE_OP_LISTEN:
        case STAGE_OP_READ:
        case STAGE_OP_READ_POST:
	case STAGE_OP_TF_READ:
                sockfd = st->getSockfd();
                st->client_read = 1;
                ev = POLLIN;
                break;
        case STAGE_OP_WRITE:
	case STAGE_OP_TF_WRITE:
        case STAGE_OP_NEXT:
		sockfd = st->getSockfd();
                st->client_write = 1;
                ev = POLLOUT;
                break;
	case STAGE_OP_UPSTREAM_CONNECT:
        case STAGE_OP_UPSTREAM_WHEAD:
        case STAGE_OP_WRITE_POST:
		case STAGE_OP_UPSTREAM_SSLW:
                sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
                st->upstream_write = 1;
                ev = POLLOUT;
                break;
        case STAGE_OP_UPSTREAM_RHEAD:
        case STAGE_OP_UPSTREAM_READ:
		case STAGE_OP_UPSTREAM_SSLR:
                sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
                st->upstream_read = 1;
                ev = POLLIN;
                break;
/////////[27]
	default:
		assert(false); 
               klog(KLOG_ERR,"BUG!!I cann't handle the op = %d\n",op);
        }
	return 0 == port_associate(kdpfd,PORT_SOURCE_FD, sockfd,ev,st);	
}
void KPortSelector::removeSocket(KSelectable *st) {
	st->sockop = 0;
}
#endif
