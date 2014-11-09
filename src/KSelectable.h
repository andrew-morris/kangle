/*
 * KSelectable.h
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

#ifndef KSELECTABLE_H_
#define KSELECTABLE_H_
#include "forwin32.h"
#include "KSocket.h"
#include "global.h"
#ifdef _WIN32
#include "mswsock.h"
#define ENABLE_IOCP   1
#ifdef  ENABLE_IOCP
#define KSELECTOR_AIO           1
#endif
#endif
#include <list>
#include "log.h"
#include "KMutex.h"
#include "KSelector.h"
#include "ksapi.h"
#include "malloc_debug.h"
#ifdef ENABLE_ATOM
#include "katom.h"
#endif
#define STF_READ       0x1
#define STF_WRITE      0x2
#define STF_CLOSED     0x4
#define STF_SSL        0x8
#define STF_ET         0x10
#define STF_EV         0x20
#define STF_ONE_SHOT   0x40
#define STF_RQ_OK      0x80
#define STF_RQ_PER_IP  0x100
#define STF_READ_LOCK  0x200
#define STF_WRITE_LOCK 0x400
#define STF_MANAGE     0x800
#define STF_ERR        0x1000
struct kgl_event
{
#ifdef _WIN32
	WSAOVERLAPPED lp;
#endif
	void *arg;
	bufferEvent buffer;
	resultEvent result;
};
class KSelectable {
public:
	KSelectable() {
#ifndef NDEBUG
		klog(KLOG_DEBUG,"new st=%p\n",this);
#endif
		memset(this,0,sizeof(*this));
	}
	virtual ~KSelectable();
	virtual KSocket *getSocket() = 0;
public:
	void set_flag(int flag)
	{
		SET(st_flags,flag);
	}
	bool isClosed()
	{
		return TEST(st_flags,STF_CLOSED)>0;
	}
	bool isSSL()
	{
		return TEST(st_flags,STF_SSL)>0;
	}
	uint16_t st_flags;
	uint16_t stack_level;
/////////[426]
	KSelector *selector;
	kgl_event e[2];
	friend class KHttpSpdy;
	friend class KEpollSelector;
	friend class KIOCPSelector;
	friend class KKqueueSelector;
	friend class KPortSelector;
protected:
	void asyncRead(void *arg,resultEvent result,bufferEvent buffer)
	{
		if (TEST(st_flags,STF_ET) && buffer && stack_level++<256) {
			//ssl有可能在ssl缓冲层还有数据可读,所以要一直读,直到读到了want_read错误
			eventRead(arg,result,buffer);
			return;
		}
		if (!selector->read(this,result,buffer,arg)) {
			result(arg,-1);
		}
	}
	void asyncWrite(void *arg,resultEvent result,bufferEvent buffer)
	{
		if (TEST(st_flags,STF_ET) && buffer && stack_level++<256) {
			eventWrite(arg,result,buffer);
			return;
		}
		if (!selector->write(this,result,buffer,arg)) {
			result(arg,-1);
		}
	}
	void eventRead(void *arg,resultEvent result,bufferEvent buffer);
	void eventWrite(void *arg,resultEvent result,bufferEvent buffer);
};
class KHttpRequest;
struct KBlockRequest
{
	KHttpRequest *rq;
	void *arg;
	int op;
	timer_func func;
	INT64 active_msec;	
	KBlockRequest *next;
	KBlockRequest *prev;
};
#endif /* KSELECTABLE_H_ */
