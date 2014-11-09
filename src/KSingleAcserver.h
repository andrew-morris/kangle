/*
 * KSingleAcServer.h
 *
 *  Created on: 2010-6-4
 *      Author: keengo
 */

#ifndef KSINGLEACSERVER_H_
#define KSINGLEACSERVER_H_

#include "KAcserver.h"
#include "KSockPoolHelper.h"
#include "KListNode.h"

class KSingleAcserver: public KPoolableRedirect,public KListNode {
public:
	KSingleAcserver();
	virtual ~KSingleAcserver();
	void connect(KHttpRequest *rq);
	unsigned getPoolSize() {
		return sockHelper->getSize();
	}
	bool isChanged(KPoolableRedirect *rd)
	{
		KSingleAcserver *sa = static_cast<KSingleAcserver *>(rd);
		return sockHelper->isChanged(sa->sockHelper);
	}
	bool setHostPort(std::string host, const char *port);
public:
	void buildXML(std::stringstream &s);
	friend class KAcserverManager;
	KSockPoolHelper *sockHelper;
	time_t lastActive;

};
#endif /* KSingleAcserver_H_ */
