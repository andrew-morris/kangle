/*
 * KPoolableStream.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KUpstreamSelectable.h"
#include "KPoolableSocketContainer.h"
#include "log.h"
#include "KConnectionSelectable.h"
#include "KHttpRequest.h"
#include "KSelector.h"
/////////[227]
KUpstreamSelectable::KUpstreamSelectable()
{
	expireTime = 0;
	container = NULL;
	/////////[228]
}
KUpstreamSelectable::~KUpstreamSelectable() {
	/////////[229]
	if (container) {
		container->unbind(this);
	}
}
void KUpstreamSelectable::gc(int lifeTime) {
	/////////[230]
	if (container == NULL) {
		delete this;
		return;
	}	
#ifndef _WIN32
	/* linux下确保 upstream在主线程上 */
	assert(TEST(st_flags,STF_READ|STF_WRITE|STF_EV)==0);
	this->selector = NULL;
#endif
	container->gcSocket(this,lifeTime);
}

void KUpstreamSelectable::isBad(BadStage stage)
{
	if (container) {
		container->isBad(this,stage);
	}
}
void KUpstreamSelectable::isGood()
{
	if (container) {
		container->isGood(this);
	}
}
int KUpstreamSelectable::getLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
void KUpstreamSelectable::connect(KHttpRequest *rq,resultEvent result)
{
	rq->c->selector->addList(rq, KGL_LIST_CONNECT);
	if (!this->selector->connect(this,result,rq)) {
		result(rq,-1);
	}
}
/* 异步读 */
void KUpstreamSelectable::upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	/////////[231]
	rq->c->selector->addList(rq,KGL_LIST_RW);
	asyncRead(rq,result,buffer);
}
/* 异步写 */
void KUpstreamSelectable::upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	rq->c->selector->addList(rq,KGL_LIST_RW);
	/////////[232]
	asyncWrite(rq,result,buffer);
}
