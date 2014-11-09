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
#include "KEpollSelector.h"
#include <stdio.h>
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "malloc_debug.h"
#include "time_utils.h"
#include "KUpstreamFetchObject.h"
#include "KPipeMessageFetchObject.h"
#ifdef HAVE_SYS_EPOLL_H
#ifdef HTTP_PROXY
#define MAXEVENT        1
#else
#define MAXEVENT	256
#endif
KEpollSelector::KEpollSelector() {
	kdpfd = epoll_create(128);
}

KEpollSelector::~KEpollSelector() {
	close(kdpfd);
}
void KEpollSelector::select() {
	epoll_event events[MAXEVENT];
	for (;;) {
#ifdef MALLOCDEBUG
		if (closeFlag) {
			delete this;
			return;
		}
#endif
		checkTimeOut();
		int ret = epoll_wait(kdpfd, events, MAXEVENT,tmo_msec);
		if (utm) {
			updateTime();
		}
		/*
		if (ret == -1) {
			continue;
		}
		*/
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = ((KSelectable *) events[n].data.ptr);
#ifndef NDEBUG
			assert(st->sockop>0);
			klog(KLOG_DEBUG,"select st=%p,sockop=%d\n",st,st->sockop);
			if (TEST(events[n].events,EPOLLERR|EPOLLHUP)) {
				klog(KLOG_DEBUG,"st=%p happen error event=%d\n",st,events[n].events);
			}
#endif
			st->handler(st,(TEST(events[n].events,EPOLLIN|EPOLLOUT)?0:-1));
		}
	}

}

bool KEpollSelector::addSocket(KSelectable *st,int op) {
	assert(st->handler);
	int poll_op = EPOLL_CTL_ADD;
	struct epoll_event ev;
	SOCKET sockfd = -1;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"try addSocket st=%p,op=%d\n",st,op);
#endif
	switch(op){
	case STAGE_OP_LISTEN:
	case STAGE_OP_READ:
	case STAGE_OP_READ_POST:
	case STAGE_OP_TF_READ:
		if (st->client_read) {
			return true;
		}
		if (st->client_op) {
			assert(st->upstream_op==0);
			st->client_write = 0;
			poll_op = EPOLL_CTL_MOD;
		} else if(st->upstream_op) {
			removeSocket(st);
			assert(st->sockop==0);
		}
		sockfd = st->getSockfd();
		st->client_read = 1;
		ev.events = EPOLLIN;
		break;
	case STAGE_OP_BIG_HEADER:
	case STAGE_OP_BIG_WRITE:
		assert(st->secondHandler);
		st = st->secondHandler;
	case STAGE_OP_BIG_CACHE_WRITE:
	case STAGE_OP_WRITE:
	case STAGE_OP_TF_WRITE:
	case STAGE_OP_NEXT:
		if (st->client_write) {
			assert(st->client_read==0);
			assert(st->upstream_op==0);
			return true;
		}
		if (st->client_op) {
			st->client_read = 0;
			poll_op = EPOLL_CTL_MOD;
		} else if (st->upstream_op) {
			removeSocket(st);
			assert(st->sockop==0);
		}
		sockfd = st->getSockfd();
		st->client_write = 1;
		ev.events = EPOLLOUT;
		break;
	case STAGE_OP_UPSTREAM_CONNECT:
	case STAGE_OP_UPSTREAM_WHEAD:
	case STAGE_OP_WRITE_POST:
	case STAGE_OP_UPSTREAM_SSLW:
		if (st->upstream_write) {
			assert(st->upstream_read==0);
			assert(st->client_op == 0);
			return true;
		}
		if (st->upstream_op) {
			st->upstream_read = 0;
			poll_op = EPOLL_CTL_MOD;
		} else if (st->client_op) {
			removeSocket(st);
			assert(st->sockop==0);
		}
		sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
		st->upstream_write = 1;
		ev.events = EPOLLOUT;
		break;	
	case STAGE_OP_UPSTREAM_RHEAD:
	case STAGE_OP_UPSTREAM_READ:
	case STAGE_OP_UPSTREAM_SSLR:
		if (st->upstream_read) {
			assert(st->upstream_write==0);
			assert(st->client_op == 0);
			return true;
		}
		if (st->upstream_op) {
			st->upstream_write = 0;
			poll_op = EPOLL_CTL_MOD;
		} else if (st->client_op) {
			removeSocket(st);
			assert(st->sockop==0);
		}
		sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
		st->upstream_read = 1;
		ev.events = EPOLLIN;
		break;
/////////[244]
	default:
		assert(false);
		klog(KLOG_ERR,"BUG!!I cann't handle the op = %d\n",op);
	}
	assert(st->client_op==0 || st->upstream_op==0);
	assert(st->sockop>0);
#ifndef NDEBUG
	klog(KLOG_DEBUG,"addSocket st=%p,sockfd=%d,op=%d\n",st,sockfd,op);
#endif
	ev.data.ptr = st;
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0 && op != STAGE_OP_LISTEN) {
		klog(KLOG_ERR, "epoll set insertion error: fd=%d,errno=%d\n", sockfd,errno);
		return false;
	}
	return true;
}
void KEpollSelector::removeSocket(KSelectable *st) {
	if (st->sockop==0) {
		return;
	}
	SOCKET sockfd;
	if(st->client_op){
		assert(st->upstream_op==0);
		sockfd = st->getSockfd();
	} else {
		assert(st->client_op==0);
		KUpstreamFetchObject *fo = static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj);
		if (fo==NULL) {
			st->sockop = 0;
			return;
		}
		sockfd = fo->getSocket()->get_socket();
	}
#ifndef NDEBUG
	klog(KLOG_DEBUG,"removeSocket st=%p,sockfd=%d\n",st,sockfd);
#endif
	struct epoll_event ev;
	st->sockop = 0;
	if (epoll_ctl(kdpfd, EPOLL_CTL_DEL,sockfd, &ev) != 0) {
		klog(KLOG_ERR, "epoll del sockfd error: fd=%d,errno=%d\n", sockfd,errno);
		return;
	}
	assert(st->client_op==0 && st->upstream_op==0);
}
#endif
