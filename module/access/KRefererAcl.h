#ifndef KREFERERACL_H
#define KREFERERACL_H
#include "KAcl.h"

enum Referer_State
{
	Referer_Null,
	Referer_NotNull,
	Referer_EqHost
};
class KRefererAcl : public KAcl
{
public:
	KAcl *newInstance() {
		return new KRefererAcl();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		KHttpHeader *tmp = rq->parser.getHeaders();
		while(tmp){
			if (strcasecmp(tmp->attr,"Referer")==0) {
				if (state!=Referer_EqHost) {
					return state==Referer_NotNull;
				}
				KUrl referer;
				if(!parse_url(tmp->val,&referer)){
					referer.destroy();
					return false;
				}
				bool matched = strcasecmp(referer.host,rq->raw_url.host)==0;
				referer.destroy();
				return matched;
			}
			tmp = tmp->next;
		}
		if (state!=Referer_EqHost) {
			return state==Referer_Null;
		}
		return false;
	}
	std::string getDisplay() {
		switch(state){
		case Referer_Null:
			return "Null";
		case Referer_NotNull:
			return "NotNull";
		case Referer_EqHost:
			return "EqHost";
		}
		return "";
	}
	void editHtml(std::map<std::string,std::string> &attibute)
		throw (KHtmlSupportException)
	{
		if(strcasecmp(attibute["referer"].c_str(),"Null")==0){
			state = Referer_Null;
		} else if(strcasecmp(attibute["referer"].c_str(),"NotNull")==0){
			state = Referer_NotNull;
		} else {
			state = Referer_EqHost;
		}
	}
	const char *getName() {
		return "referer";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input type='radio' name='referer' value='Null' " << (state==Referer_Null?"checked":"") << ">Null";
		s << "<input type='radio' name='referer' value='NotNull' " << (state==Referer_NotNull?"checked":"") << ">NotNull";
		s << "<input type='radio' name='referer' value='EqHost' " << (state==Referer_EqHost?"checked":"") << ">EqHost";
		return s.str();
	}
	void buildXML(std::stringstream &s) {
		s << " referer='" << getDisplay() << "'>";
	}
private:
	Referer_State state;
};
#endif
