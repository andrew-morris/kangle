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
#include "malloc_debug.h"

class KSelectable;
typedef void (*handleEvent)(KSelectable *st,int got);

class KSelector;
enum ReadState {
	READ_FAILED, READ_CONTINUE, READ_SUCCESS, READ_SSL_CONTINUE
};
enum WriteState {
	WRITE_FAILED, WRITE_CONTINUE, WRITE_SUCCESS
};

#define STATE_IDLE     0
#define STATE_CONNECT  1
#define STATE_SEND     2
#define STATE_RECV     3
#define STATE_QUEUE    4
class KSecondHandler;
class KSelectable {
public:
	KSelectable() {
/////////[348]
		selector = NULL;
		//op = 0;
		sockop = 0;
		handler = NULL;
		active_msec = 0;
		secondHandler = NULL;
#ifndef NDEBUG
		klog(KLOG_DEBUG,"new st=%p\n",this);
#endif
	}
	virtual ~KSelectable();
	virtual SOCKET getSockfd() = 0;
	INT64 active_msec;
public:
	union {
		struct {
			unsigned client_read:1;
			unsigned client_write:1;
			unsigned upstream_read:1;
			unsigned upstream_write:1;
		};
		struct {
			unsigned client_op:2;
			unsigned upstream_op:2;
		};
		unsigned sockop : 4;
	};
/////////[349]
	KSelector *selector;
	KSecondHandler *secondHandler;
	handleEvent handler;
};
class KSecondHandler : public KSelectable
{
public:
	KSecondHandler()
	{
#ifdef _WIN32
		lp.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
#endif
	}
	~KSecondHandler()
	{
#ifdef _WIN32
		CloseHandle(lp.hEvent);
#endif
	}
#ifdef _WIN32
	WSAOVERLAPPED lp;
#endif
	KSelectable *main;
	SOCKET getSockfd()
	{
		return main->getSockfd();
	}
};
class KHttpRequest;
struct KBlockRequest
{
	KHttpRequest *rq;
	KSelectable *st;
	int op;
	handleEvent handler;
	INT64 active_msec;
	KBlockRequest *next;
	KBlockRequest *prev;
};
#endif /* KSELECTABLE_H_ */
