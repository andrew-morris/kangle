#ifndef KVIRTUALHOSTCONTAINER_H
#define KVIRTUALHOSTCONTAINER_H
#include "rbtree.h"
class KHttpRequest;
class KSubVirtualHost;
class KVirtualHost;
enum query_vh_result
{
	query_vh_success,
	query_vh_connect_limit,
	query_vh_host_not_found
};
class KVirtualHostContainer
{
public:
	KVirtualHostContainer();
	~KVirtualHostContainer();
	query_vh_result parseVirtualHost(KHttpRequest *rq,const char *site);
	bool unbindVirtualHost(KSubVirtualHost *vh);	
	bool bindVirtualHost(KSubVirtualHost *vh);
	void addVirtualHost(KVirtualHost *vh);
	void removeVirtualHost(KVirtualHost *vh);
	void clear();
	bool isEmpty() {
		if (hosts.rb_node == NULL && wideHosts.rb_node == NULL && defaultVh == NULL) {
			return true;
		}
		return false;
	}
private:
	rb_root hosts;
	rb_root wideHosts;
	KSubVirtualHost *defaultVh;
};
#endif
