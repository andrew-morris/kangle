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
#ifndef KSPEEDLIMITMARK_H_
#define KSPEEDLIMITMARK_H_
#include<string>
#include<map>
#include "KMark.h"
#include "do_config.h"
#include "KSpeedLimit.h"

class KSpeedLimitMark : public KMark {
public:
	KSpeedLimitMark() {
		min_limit_size=0;
		speed_limit=0;
	}	
	virtual ~KSpeedLimitMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) {
		KSpeedLimit *sl = new KSpeedLimit;
		sl->setSpeedLimit(min_limit_size,speed_limit);
		rq->addSpeedLimit(sl);
		sl->release();
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << min_limit_size << "/" << speed_limit;
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		min_limit_size=(int)get_size(attibute["min_size"].c_str());
		speed_limit=(int)get_size(attibute["limit"].c_str());
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "min_size:<input name=min_size size=10 value='";
		KSpeedLimitMark *mark=(KSpeedLimitMark *)(model);
		if (mark) {
			s << mark->min_limit_size;
		}
		s << "'>,limit:<input name='limit' size=10 value='";
		if (mark) {
			s << mark->speed_limit;
		}
		s << "'>";
		return s.str();
	}
	KMark *newInstance() {
		return new KSpeedLimitMark();
	}
	const char *getName() {
		return "speed_limit";
	}
public:
	void buildXML(std::stringstream &s) {
		s << " min_size='" << min_limit_size << "' limit='"
				<< speed_limit << "'>";
	}
private:
	int min_limit_size;
	int speed_limit;
};
#endif /*KSPEEDLIMITMARK_H_*/
