/*
 * KPoolableStream.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KPoolableSocket.h"
#include "KPoolableSocketContainer.h"
#include "log.h"

KPoolableSocket::~KPoolableSocket() {
	if (container) {
		container->unbind(this);
	}
}
void KPoolableSocket::destroy(int lifeTime) {
#ifdef _WIN32
	rq = NULL;
#endif
	if (container == NULL) {
		delete this;
		return;
	}
	container->gcSocket(this,lifeTime);
}

void KPoolableSocket::isBad(BadStage stage)
{
	if (container) {
		container->isBad(this,stage);
	}
}
void KPoolableSocket::isGood()
{
	if (container) {
		container->isGood(this);
	}
}
int KPoolableSocket::getLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
/////////[62]
