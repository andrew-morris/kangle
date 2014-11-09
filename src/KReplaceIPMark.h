#ifndef KREPLACEIPMARK_H
#define KREPLACEIPMARK_H
#include "KMark.h"
class KReplaceIPMark : public KMark
{
public:
	KReplaceIPMark()
	{
		header = "X-Real-Ip";
		val = NULL;
	}
	~KReplaceIPMark()
	{
		if (val) {
			delete val;
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		KHttpHeader *h = rq->parser.headers;
		KHttpHeader *prev = NULL;
		while (h) {
			if (strcasecmp(h->attr, header.c_str()) == 0) {
				KRegSubString *sub = NULL;
				if (val) {
					sub = val->matchSubString(h->val, strlen(h->val), 0);
				}
				if (val == NULL || sub) {
					if (prev) {
						prev->next = h->next;
					} else {
						rq->parser.headers = h->next;
					}
					if (rq->client_ip) {
						free(rq->client_ip);
					}
					if (val == NULL) {
						rq->client_ip = h->val;
					} else {
						free(h->val);
						char *ip = sub->getString(1);
						if (ip) {
							rq->client_ip = strdup(ip);
						}
					}
					free(h->attr);
					free(h);
					return true;
				}
			}
			prev = h;
			h = h->next;
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
		s << "val(regex):<input name='val' value='";
		if (m && m->val) {
			s << m->val->getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << header;
		if (val) {
			s << ":" << val->getModel();
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		header = attribute["header"];
		std::string val = attribute["val"];
		if (this->val) {
			delete this->val;
			this->val = NULL;
		}
		if (!val.empty()) {
			this->val = new KReg;
			this->val->setModel(val.c_str(), PCRE_CASELESS);
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " header='" << header << "' ";
		if (val) {
			s << "val='" << KXml::param(val->getModel()) << "' ";
		}
		s << "> ";
	}
private:
	std::string header;
	KReg *val;
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
			rq->c->socket->get_self_ip(ip,sizeof(ip));
			rq->bind_ip = strdup(ip);
		} else if (ip[0] == '$') {
			rq->bind_ip = strdup(rq->getClientIp());
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

