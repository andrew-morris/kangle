#ifndef KCDNCONTAINER_H
#define KCDNCONTAINER_H
#include <map>
#include "global.h"
#include "KList.h"
#include "KSingleAcserver.h"
#include "KMutex.h"

struct CdnTarget
{
	const char *ip;
	const char *host;
	int port;
	/////////[279]
	Proto_t proto;
	int lifeTime;
};
struct less_target {
	bool operator()(const CdnTarget * __x, const CdnTarget * __y) const {
		if (__x->ip) {
			if (__y->ip==NULL) {
				return false;
			}
			int ret = strcmp(__x->ip,__y->ip);
			if (ret<0) {
				return true;
			}
			if (ret>0) {
				return false;
			}
		} else {
			if (__y->ip) {
				return true;
			}
		}
		int ret=strcasecmp(__x->host,__y->host);
		if (ret<0) {
			return true;
		}
		if (ret>0) {
			return false;
		}
		ret = __x->port - __y->port;
		if (ret<0) {
			return true;
		}
		if (ret>0) {
			return false;
		}
		ret = __x->lifeTime - __y->lifeTime;
		if (ret<0) {
			return true;
		}
		if (ret>0) {
			return false;
		}
		/////////[280]
		return __x->proto < __y->proto;
	}
};
class KCdnContainer
{
public:
	KCdnContainer();
	~KCdnContainer();
	KFetchObject *get(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto=Proto_http);
	KSingleAcserver *refsRedirect(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto,bool isIp=false);
	void flush(time_t nowTime);
private:
	KMutex lock;
	std::map<CdnTarget *,KSingleAcserver *,less_target> serverMap;
	KList serverList;
};
extern KCdnContainer cdnContainer;
#endif

