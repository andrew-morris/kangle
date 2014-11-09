#ifndef KSRCSACL_H
#define KSRCSACL_H
#include "KMultinAcl.h"
#include "KXml.h"
#include "utils.h"
class KSrcsAcl: public KMultinAcl {
public:
	KSrcsAcl() {
		icase_can_change = false;
		seticase(false);
	}
	virtual ~KSrcsAcl() {
	}
	KAcl *newInstance() {
		return new KSrcsAcl();
	}
	const char *getName() {
		return "srcs";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->client_ip==NULL) {
			rq->client_ip = (char *)malloc(MAXIPLEN);
			rq->c->socket->get_remote_ip(rq->client_ip,MAXIPLEN);
		}
		return KMultinAcl::match(rq->client_ip);
	}
protected:
	char *transferItem(char *file)
	{
		//KFileName::tripDir3(file,PATH_SPLIT_CHAR);
		return file;
	}
};
#endif
