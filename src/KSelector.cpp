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
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "KSelector.h"
#include "utils.h"
#include "log.h"
#include "KThreadPool.h"
#include "malloc_debug.h"
#include "KHttpRequest.h"
#include "http.h"
#include "KHttpManage.h"
#include "KRequestQueue.h"
#include "time_utils.h"
inline rb_node *rbInsertRequest(rb_root *root,KBlockRequest *brq,bool &isfirst)
{
	struct rb_node **n = &(root->rb_node), *parent = NULL;
	KBlockRequest *tmp = NULL;
	while (*n) {
		tmp = (KBlockRequest *)((*n)->data);
		INT64 result = brq->active_msec - tmp->active_msec;
		parent = *n;
		if (result < 0) {
			n = &((*n)->rb_left);
		} else if (result > 0) {
			n = &((*n)->rb_right);
			isfirst = false;	
		} else {
			isfirst = false;
			brq->next = tmp;
			//prev指向最后一个
			brq->prev = tmp->prev;
			(*n)->data = brq;
		    return *n;
		}
	}
	rb_node *node = new rb_node;
	node->data = brq;
	brq->next = NULL;
	brq->prev = brq;
	rb_link_node(node, parent, n);
	rb_insert_color(node, root);
	return node;
}
FUNC_TYPE FUNC_CALL selectorThread(void *param) {
	KSelector *selector = (KSelector*) param;
	selector->selectThread();
	KTHREAD_RETURN;
}
void KSelector::selectThread()
{
	thread_id = pthread_self();
#ifdef MALLOCDEBUG
	closeFlag = false;
#endif
	select();
}
KSelector::KSelector() {
	utm = false;
	tmo_msec = 1000;
	blockBeginNode = NULL;
	blockList.rb_node = NULL;
#ifdef MALLOCDEBUG
	closeFlag = true;
#endif
}
KSelector::~KSelector() {
	
}
bool KSelector::startSelect() {
	return m_thread.start((void *) this, selectorThread);
}
void KSelector::adjustTime(INT64 t)
{
	listLock.Lock();
	rb_node *node = rb_first(&blockList);
	while (node) {
		KBlockRequest *brq = (KBlockRequest *)node->data;
		assert(brq);
		brq->active_msec += t;
		node = rb_next(node);
	}
	listLock.Unlock();
}
void KSelector::addTimer(KHttpRequest *rq,timer_func func,void *arg,int msec)
{
	KBlockRequest *brq = new KBlockRequest;
	brq->rq = rq;
	brq->op = STAGE_OP_TIMER;
	brq->active_msec = kgl_current_msec + msec;
	brq->func = func;
	brq->arg = arg;
	listLock.Lock();
	if (rq && rq->list!=KGL_LIST_NONE) {		
		requests[rq->list].remove(rq);
		rq->list = KGL_LIST_NONE;
	}
	bool isFirst = true;
	rb_node *node = rbInsertRequest(&blockList,brq,isFirst);
	if (isFirst) {
		blockBeginNode = node;
	}
	listLock.Unlock();
}
void KSelector::addList(KHttpRequest *rq,int list)
{
	rq->active_msec = kgl_current_msec;
	rq->tmo_left = rq->tmo;
	assert(list>=0 && list<KGL_LIST_NONE);
	listLock.Lock();
	if (rq->list!=KGL_LIST_NONE) {
		requests[rq->list].remove(rq);
	}
	requests[list].pushBack(rq);
	rq->list = list;
	listLock.Unlock();
}
void KSelector::removeList(KHttpRequest *rq)
{	
	listLock.Lock();
	int list = rq->list;
	if (list == KGL_LIST_NONE) {
		listLock.Unlock();
		return;
	}
	assert(list>=0 && list<KGL_LIST_NONE);	
	requests[list].remove(rq);
	rq->list = KGL_LIST_NONE;
	listLock.Unlock();	
}
void KSelector::checkTimeOut() {
	listLock.Lock();
	for(int i=0;i<KGL_LIST_SYNC;i++){		
		KHttpRequest *rq = requests[i].getHead();
		KHttpRequest *next;
		while (rq) {
			if ((kgl_current_msec - rq->active_msec) > (time_t)timeout[i]) {
				next = requests[i].remove(rq);
				if (rq->tmo_left>0) {
					//还有额外超时时间
					rq->tmo_left--;
					rq->active_msec = kgl_current_msec;
					requests[i].pushBack(rq);
					rq = next;
					continue;
				}
#ifndef NDEBUG
				klog(KLOG_DEBUG,"request timeout st=%p\n",(KSelectable *)rq);
#endif
				
				rq->closeConnection();
				assert(rq->list==i);
				rq->list = KGL_LIST_NONE;
#ifdef _WIN32
				CLR(rq->c->st_flags,STF_EV);			
#endif
				rq = next;
				continue;
			}
			break;
		}
		
	}
	KBlockRequest *activeRequest = NULL;
	KBlockRequest *last = NULL;
	while (blockBeginNode) {
		KBlockRequest *rq = (KBlockRequest *)blockBeginNode->data;
		assert(rq);
		if (kgl_current_msec<rq->active_msec) {
			break;
		}
		rb_node *next = rb_next(blockBeginNode);
		rb_erase(blockBeginNode,&blockList);
		delete blockBeginNode;
		blockBeginNode = next;
		if (activeRequest==NULL) {
			activeRequest = rq;
		} else {
			last->next = rq;
		}
		last = rq->prev;
		assert(last && last->next==NULL);
	}
	listLock.Unlock();
	while (activeRequest) {
		last = activeRequest->next;
		//debug("%p is active\n",activeRequest);
		if (activeRequest->rq) {
			addList(activeRequest->rq,KGL_LIST_RW);
		}
		int op = activeRequest->op;
		assert(op==STAGE_OP_TIMER);
		if (op==STAGE_OP_TIMER) {
			//自定义的定时器
			activeRequest->func(activeRequest->arg);
			delete activeRequest;
			activeRequest = last;
			continue;
		}
		delete activeRequest;
		activeRequest = last;
	}
}
void KSelector::bindSelectable(KSelectable *st)
{
	st->selector = this;
}
