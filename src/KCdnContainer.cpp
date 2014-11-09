#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
KCdnContainer cdnContainer;
using namespace std;
KCdnContainer::KCdnContainer()
{
}
KCdnContainer::~KCdnContainer()
{
}
KSingleAcserver *KCdnContainer::refsRedirect(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto,bool isIp)
{
	CdnTarget ttarget;
	ttarget.ip = ip;
	ttarget.host = host;
	ttarget.port = port;
	ttarget.proto = proto;
	ttarget.lifeTime = life_time;
	/////////[354]
	KSingleAcserver *server = NULL;

	lock.Lock();
	std::map<CdnTarget *,KSingleAcserver *,less_target>::iterator it = serverMap.find(&ttarget);
	if (it!=serverMap.end()) {
		server = (*it).second;
		serverList.remove(server);
	} else {
		server = new KSingleAcserver;
		server->proto = proto;
		server->sockHelper->setHostPort(host,port,ssl);
		server->sockHelper->setLifeTime(life_time);
		server->sockHelper->setIp(ip);
		server->sockHelper->isIp = isIp;
		CdnTarget *target = new CdnTarget;
		target->ip = server->sockHelper->getIp();
		target->host = server->sockHelper->host.c_str();
		target->port = port;
		target->proto = proto;
		target->lifeTime = life_time;
		/////////[355]
		serverMap.insert(pair<CdnTarget *,KSingleAcserver *>(target,server));
	}
	serverList.push_back(server);
	server->lastActive = kgl_current_sec;
	server->addRef();
	lock.Unlock();
	return server;
}
KFetchObject *KCdnContainer::get(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto)
{
	KSingleAcserver *server = refsRedirect(ip,host,port,ssl,life_time,proto);
	KBaseRedirect *brd = new KBaseRedirect(server,false);
	KFetchObject *fo = new KHttpProxyFetchObject();
	fo->bindBaseRedirect(brd);
	brd->release();	
	return fo;
}
void KCdnContainer::flush(time_t nowTime)
{
	lock.Lock();
	KSingleAcserver *server = static_cast<KSingleAcserver *>(serverList.getHead());
	while(server){
		if (nowTime-server->lastActive < 60) {
			break;
		}
		KSingleAcserver *next = static_cast<KSingleAcserver *>(server->next);
		if (server->getRefFast()>1) {
			server = next;
			continue;
		}		
		serverList.remove(server);
		CdnTarget ttarget;
		ttarget.ip = server->sockHelper->getIp();
		ttarget.host = server->sockHelper->host.c_str();
		ttarget.port = server->sockHelper->port;
		ttarget.proto = server->proto;
		ttarget.lifeTime = server->sockHelper->getLifeTime();
		/////////[356]
		std::map<CdnTarget *,KSingleAcserver *,less_target>::iterator it = serverMap.find(&ttarget);
		assert(it!=serverMap.end());
		CdnTarget *target = (*it).first;
		serverMap.erase(it);
		delete target;
		server->release();
		server = next;
	}
	lock.Unlock();
}

