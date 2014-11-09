#ifndef KHTTPONLYCOOKIEMARK_H
#define KHTTPONLYCOOKIEMARK_H
#include "KMark.h"
#define HTTP_ONLY_STRING     "; HttpOnly"
class KHttpOnlyCookieMark : public KMark
{
public:
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		bool result = false;
		if (obj && obj->data) {
			KHttpHeader *h = obj->data->headers;
			while (h) {
				if (strcasecmp(h->attr,"Set-Cookie")==0 || strcasecmp(h->attr,"Set-Cookie2")==0) {
					if (strstr(h->val,HTTP_ONLY_STRING)==NULL) {
						int val_len = strlen(h->val);
						if (cookie.match(h->val,val_len,0)>0) {
							int new_len = val_len + sizeof(HTTP_ONLY_STRING);
							char *buf = (char *)malloc(new_len);
							memcpy(buf,h->val,val_len);
							memcpy(buf+val_len,HTTP_ONLY_STRING,sizeof(HTTP_ONLY_STRING));
							free(h->val);
							h->val = buf;
							result = true;
						}
					}					
				}//if
				h = h->next;
			}//while
		}//if
		return result;
	}
	KMark *newInstance()
	{
		return new KHttpOnlyCookieMark;
	}
	const char *getName()
	{
		return "http_only";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "Cookie regex:<input name='cookie' value='";
		KHttpOnlyCookieMark *m = (KHttpOnlyCookieMark *)model;
		if (m) {
			s << m->cookie.getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return cookie.getModel();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		cookie.setModel(attribute["cookie"].c_str(),0);
	}
	void buildXML(std::stringstream &s)
	{
		s << " cookie='" << KXml::param(cookie.getModel()) << "'>";
	}
private:
	KReg cookie;
};
#endif
