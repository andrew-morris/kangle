#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include "global.h"
#include "KKqueueSelector.h"
#include "KUpstreamFetchObject.h"
#include "KPipeMessageFetchObject.h"
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "time_utils.h"
#include "malloc_debug.h"
#define MAXEVENT	16
#ifndef NETBSD
typedef void * kqueue_udata_t;
#else
typedef intptr_t kqueue_udata_t;
#endif
KKqueueSelector::KKqueueSelector() {
	kdpfd = kqueue() ;
}

KKqueueSelector::~KKqueueSelector() {
	close(kdpfd);
}
bool KKqueueSelector::addListenSocket(KSelectable *st) {
	addSocket(st,STAGE_OP_LISTEN);
	return true;
}
void KKqueueSelector::select() {
	struct kevent events[MAXEVENT]; 
	struct timespec tm;
	tm.tv_sec = tmo_msec/1000;
	tm.tv_nsec = tmo_msec * 1000 - tm.tv_sec * 1000000;
	for (;;) {
		checkTimeOut();
		int ret = kevent(kdpfd, NULL, 0, events, MAXEVENT, &tm);
		if(utm){
			updateTime();
        }
		if (ret == -1) {
			continue;
		}
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = (KSelectable *) events[n].udata;
#ifndef NDEBUG
            st->hs.clear();
#endif
			st->handler(st,0);
		}
	}

}

bool KKqueueSelector::addSocket(KSelectable *st,int op) {
	struct kevent changes[1]; 
	int ev = 0;
        SOCKET sockfd = -1;
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
                } else if(st->upstream_op) {
                        removeSocket(st);
                        assert(st->sockop==0);
                }
                sockfd = st->getSockfd();
                st->client_read = 1;
                ev = EVFILT_READ;
                break;
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
                } else if (st->upstream_op) {
                        removeSocket(st);
                        assert(st->sockop==0);
                }
                sockfd = st->getSockfd();
                st->client_write = 1;
                ev = EVFILT_WRITE;
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
                } else if (st->client_op) {
                        removeSocket(st);
                        assert(st->sockop==0);
                }
                sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
                st->upstream_write = 1;
                ev = EVFILT_WRITE;
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
                } else if (st->client_op) {
                        removeSocket(st);
                        assert(st->sockop==0);
                }
                sockfd = (static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj)->getSocket())->get_socket();
                st->upstream_read = 1;
                ev = EVFILT_READ;
                break;
/////////[353]
	default:
                assert(false);
                klog(KLOG_ERR,"BUG!!I cann't handle the op = %d\n",op);
        }
	
	EV_SET(&changes[0], sockfd, ev, EV_ADD, 0, 0, (kqueue_udata_t)st); 
      	if(kevent(kdpfd, changes, 1, NULL, 0, NULL)==-1){
		klog(KLOG_ERR,"cann't addSocket sockfd=%d,ev=%d\n",sockfd,ev);
		return false;
	}
	return true;
}
void KKqueueSelector::removeSocket(KSelectable *st) {
        if (st->sockop==0) {
                return;
        }
        SOCKET sockfd;
	int ev = 0;
        if(st->client_op){
                assert(st->upstream_op==0);
                sockfd = st->getSockfd();
		if(st->client_read){
			SET(ev ,EVFILT_READ);
		}
		if(st->client_write){
			SET(ev,EVFILT_WRITE);
		}
        } else {
                assert(st->client_op==0);
                KUpstreamFetchObject *fo = static_cast<KUpstreamFetchObject *>(static_cast<KHttpRequest *>(st)->fetchObj);
                if (fo==NULL) {
                        st->sockop = 0;
                        return;
                }
                sockfd = fo->getSocket()->get_socket();
		if(st->upstream_read){
			SET(ev,EVFILT_READ);
		}
		if(st->upstream_write){
			SET(ev,EVFILT_WRITE);
		}
        }
        st->sockop = 0;
	struct kevent changes[1]; 
	EV_SET(&changes[0], sockfd, ev, EV_DELETE, 0, 0, NULL); 
      	kevent(kdpfd, changes, 1, NULL, 0, NULL);
	st->sockop = 0;
}
#endif
