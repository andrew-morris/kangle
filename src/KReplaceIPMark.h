#ifndef KREPLACEIPMARK_H
#define KREPLACEIPMARK_H
#include "KMark.h"
class KReplaceIPMark : public KMark
{
public:
	KReplaceIPMark()
	{
		header = "X-Real-Ip";
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		KHttpHeader *h = rq->parser.removeHeader(header.c_str());
		if (h) {
			if (rq->client_ip) {
				free(rq->client_ip);
			}
			rq->client_ip = h->val;
			free(h->attr);
			free(h);
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KReplaceIPMark;
	}
	const char *getName()
	{
		return "replace_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "header:<input name='header' value='";
		KReplaceIPMark *m = (KReplaceIPMark *)model;
		if (m) {
			s << m->header;
		} else {
			s << "X-Real-Ip";
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return header;
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		header = attribute["header"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " header='" << header << "'>";
	}
private:
	std::string header;
};
class KSelfIPMark : public KMark
{
public:
	KSelfIPMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		if (rq->bind_ip) {
			free(rq->bind_ip);
		}
		if (ip.size()==0) {
			char ip[MAXIPLEN];
			rq->server->get_self_ip(ip,sizeof(ip));
			rq->bind_ip = strdup(ip);
		} else {
			rq->bind_ip = strdup(ip.c_str());
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KSelfIPMark;
	}
	const char *getName()
	{
		return "self_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "ip:<input name='ip' value='";
		KSelfIPMark *m = (KSelfIPMark *)model;
		if (m) {
			s << m->ip;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return ip;
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		ip = attribute["ip"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " ip='" << ip << "'>";
	}
private:
	std::string ip;
};
#endif

