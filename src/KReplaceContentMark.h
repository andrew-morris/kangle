#ifndef KREPLACECONTENTMARK_H
#define KREPLACECONTENTMARK_H
#include "KMark.h"
#include "KReplaceContentFilter.h"
#include "KReg.h"
#include "KHttpRequest.h"
#include "KFilterContext.h"
#include "utils.h"
#include "KRewriteMarkEx.h"
class KReplaceContentMark;
struct KReplaceContentMarkParam
{
	KHttpRequest *rq;
	KReplaceContentMark *mark;
};
bool replaceContentMarkCallBack(void *param,KRegSubString *sub_string,int *ovector,KStringBuf *st);
void replaceContentMarkEndCallBack(void *param);
class KReplaceContentMark : public KMark
{
public:
	KReplaceContentMark()
	{
		replaced_stop = false;
		buffer = 1048576;
		nc = 1;
	}
	bool callBack(KHttpRequest *rq,KRegSubString *sub_string,int *ovector,KStringBuf *st)
	{
		KRewriteMarkEx::getString(NULL,charset_replace.c_str(),rq,sub_string,sub_string,st);
		return !replaced_stop;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType) 
	{
		KReplaceContentFilter *filter = new KReplaceContentFilter;
		addRef();
		KReplaceContentMarkParam *rp = new KReplaceContentMarkParam;
		rp->mark = this;
		rp->rq = rq;
		filter->setBuffer((replaced_stop?buffer:0));
		filter->setHook(replaceContentMarkCallBack,replaceContentMarkEndCallBack,rp,&this->charset_content);
		rq->getOutputFilterContext()->registerFilterStream(filter,true);
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		int len = content.size();
		char *buf = KXml::htmlEncode(content.c_str(),len,NULL);
		if (buf) {
			s << buf;
			free(buf);
		}
		s << "==>";
		len = replace.size();
		buf = KXml::htmlEncode(replace.c_str(),len,NULL);
		if (buf) {
			s << buf;
			free(buf);
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
		throw (KHtmlSupportException) {
			content = attibute["content"];
			replace = attibute["replace"];
			charset = attibute["charset"];
			nc = atoi(attibute["nc"].c_str());
			replaced_stop = (attibute["replaced_stop"] == "1");
			buffer = (int)get_size(attibute["buffer"].c_str());
			int flag = (nc?PCRE_CASELESS:0);
			bool use_charset = false;
			if (charset.size()>0 && strcasecmp(charset.c_str(),"utf-8")!=0) {
				char *str = NULL;
				str = utf82charset(content.c_str(), content.size(), charset.c_str());
				if (str) {
					charset_content.setModel(str,flag);
					use_charset = true;
					free(str);
				}
				str = utf82charset(replace.c_str(),replace.size(),charset.c_str());
				if (str) {
					charset_replace = str;
					free(str);
				}
			}
#ifdef PCRE_NO_UTF8_CHECK
			flag |= (PCRE_NO_UTF8_CHECK);
#endif
			if (!use_charset) {
				charset_content.setModel(content.c_str(),flag);
				charset_replace = replace;
			}
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KReplaceContentMark *m = (KReplaceContentMark *)model;
		s << "content(regex):<textarea name='content' rows=1>";
		if (m) {
			s << m->content;
		}
		s << "</textarea>";
		s << "<input type=checkbox name=nc value='1' ";
		if(m && m->nc){
			s << "checked";
		}
		s << ">nc ";
		s << "replace:<textarea name='replace' rows=1>";
		if (m) {
			s << m->replace;
		}
		s << "</textarea>";
		s << "charset:<input name='charset' value='";
		if(m){
			s << m->charset;
		}
		s << "'>";
		s << "buffer:<input name='buffer' size=4 value='";
		if (m) {
			s << get_size(m->buffer);
		}
		s << "'>";
		s << "<input type=checkbox name=replaced_stop value='1' ";
		if(m && m->replaced_stop){
			s << "checked";
		}
		s << ">replaced_stop";
		return s.str();
	}
	KMark *newInstance() {
		return new KReplaceContentMark;
	}
	const char *getName() {
		return "replace_content";
	}
	void buildXML(std::stringstream &s) {
		s << " content='" << KXml::param(content.c_str()) << "' replace='" << KXml::param(replace.c_str()) << "'";
		if (nc) {
			s << " nc='1'";
		}
		s << " charset='" << charset << "'";
		if (replaced_stop) {
			s << " replaced_stop='1'";
		}
		s << " buffer='" << get_size(buffer) << "'";
		s << ">";
	}
	friend class KReplaceContentFilter;
private:
	std::string content;
	std::string replace;
	std::string charset;
	KReg charset_content;
	std::string charset_replace;
	int nc;
	bool replaced_stop;
	int buffer;
};
#endif

