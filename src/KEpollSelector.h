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
#ifndef KEPOLLSELECTOR_H_
#define KEPOLLSELECTOR_H_

#include "global.h"

#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#include "KSelector.h"
#include "malloc_debug.h"

class KEpollSelector : public KSelector
{
public:
	const char *getName()
	{
		return "epoll";
	}
	KEpollSelector();
	virtual ~KEpollSelector();
	void select();
	bool addListenSocket(KSelectable *st)
	{
		//int flag = 1;
		//setsockopt(st->getSockfd(), IPPROTO_TCP, TCP_NODELAY,(const char *) &flag, sizeof(int));
		//setnoblock(st->getSockfd());
		addSocket(st,STAGE_OP_LISTEN);
		return true;
	}
protected:
	bool addSocket(KSelectable *rq,int op);
	void removeSocket(KSelectable *rq);
private:
	int kdpfd;

};
#endif
#endif /*KEPOLLSELECTOR_H_*/
