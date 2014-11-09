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
#ifndef KCACHECONTROLMARK_H_
#define KCACHECONTROLMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
class KCacheControlMark: public KMark {
public:
	KCacheControlMark() {
		max_age = 0;
		staticUrl = false;
		lastModified = false;
		soft = false;
		must_revalidate = false;
	}
	virtual ~KCacheControlMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) {
#ifdef ENABLE_FORCE_CACHE
		if (staticUrl) {
			obj->forceCache(lastModified);
		}
#endif
		if (!TEST(obj->index.flags,ANSW_NO_CACHE)) {			
			SET(obj->index.flags,FLAG_NEED_CACHE);
			if (max_age>0) {
				obj->index.max_age = max_age;
				SET(obj->index.flags,(soft?ANSW_HAS_EXPIRES:ANSW_HAS_MAX_AGE));
			}
			if (must_revalidate) {
				SET(obj->index.flags,OBJ_MUST_REVALIDATE);
			}
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "max_age:" << max_age;
		if(staticUrl){
			s << " static";
		}
		if (lastModified) {
			s << " last_modified";
		}
		if (soft) {
			s << " soft";
		}
		if (must_revalidate) {
			s << " must_revalidate";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		max_age = atoi(attibute["max_age"].c_str());
		if(attibute["static"]=="on" || attibute["static"]=="1"){
			staticUrl = true;
		}else{
			staticUrl = false;
		}
		soft = (attibute["soft"]=="1");
		lastModified = (attibute["last_modified"]=="1");
		must_revalidate = (attibute["must_revalidate"]=="1");
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KCacheControlMark *mark = (KCacheControlMark *) model;
		s << "max_age: <input type=text name=max_age size=6 value='";
		if (mark) {
			s << mark->max_age;
		}
		s << "'><input type=checkbox name='static' value='1' ";
		if(mark && mark->staticUrl){
			s << "checked";
		}
		s << ">" << klang["static"];
		s << "<input type=checkbox name='last_modified' value='1' ";
		if(mark && mark->lastModified){
			s << "checked";
		}
		s << ">last_modified";
		s << "<input type=checkbox name='soft' value='1' ";
		if(mark && mark->soft){
			s << "checked";
		}
		s << ">soft";
		s << "<input type=checkbox name='must_revalidate' value='1' ";
		if(mark && mark->must_revalidate){
			s << "checked";
		}
		s << ">must_revalidate";
		return s.str();
	}
	KMark *newInstance() {
		return new KCacheControlMark();
	}
	const char *getName() {
		return "cache_control";
	}
public:
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute) {
		editHtml(attribute);
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " max_age='" << max_age << "'";
		if(staticUrl){
			s << " static='1'";
		}
		if (soft) {
			s << " soft='1'";
		}
		if (lastModified) {
			s << " last_modified='1'";
		}
		if (must_revalidate) {
			s << " must_revalidate='1'";
		}
		s << ">";
	}
private:
	unsigned max_age;
	bool staticUrl;
	bool lastModified;
	bool soft;
	bool must_revalidate;
};

#endif /*KRESPONSEFLAGMARK_H_*/
