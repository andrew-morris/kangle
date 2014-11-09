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
#ifndef KHOSTACL_H_
#define KHOSTACL_H_

#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
class KHostAcl: public KMultiAcl {
public:
	KHostAcl() {
		icase_can_change = false;
	}
	virtual ~KHostAcl() {
	}
	KAcl *newInstance() {
		return new KHostAcl();
	}
	const char *getName() {
		return "host";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return KMultiAcl::match(rq->url->host);
	}
};
class KWideHostAcl: public KMultiAcl {
public:
	KWideHostAcl() {
		icase_can_change = false;
	}
	virtual ~KWideHostAcl() {
	}
	KAcl *newInstance() {
		return new KWideHostAcl();
	}
	const char *getName() {
		return "wide_host";
	}
	char *transferItem(char *item)
	{
		if(*item=='.'){
			return item;
		}
		char *p = strchr(item,'.');
		if (p==NULL) {
			return item;
		}
		char *nitem = strdup(p);
		free(item);
		return nitem;
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		char *site = strchr(rq->url->host,'.');
		if (site) {
			return KMultiAcl::match(site);
		}
		return false;
	}
};

#endif /*KHOSTACL_H_*/
