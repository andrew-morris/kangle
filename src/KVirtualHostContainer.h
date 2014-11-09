#ifndef KVIRTUALHOSTCONTAINER_H
#define KVIRTUALHOSTCONTAINER_H
#include "rbtree.h"
class KSubVirtualHost;
class KHttpRequest;
class KVirtualHost;
enum query_vh_result
{
	query_vh_success,
	query_vh_connect_limit,
	query_vh_host_not_found,
	query_vh_unknow
};
class KWideBindVirtualHost
{
public:
	KWideBindVirtualHost(KSubVirtualHost *svh)
	{
		this->svh = svh;
	}
	KWideBindVirtualHost()
	{
		this->svh = NULL;
	}
	KSubVirtualHost *svh;
	KWideBindVirtualHost *next;
	KWideBindVirtualHost *prev;
};
class KVirtualHostContainer
{
public:
	KVirtualHostContainer();
	~KVirtualHostContainer();
	query_vh_result parseVirtualHost(KSubVirtualHost **rq_svh,const char *site);
	bool unbindVirtualHost(KSubVirtualHost *vh);	
	bool bindVirtualHost(KSubVirtualHost *vh);
	void addVirtualHost(KVirtualHost *vh);
	void removeVirtualHost(KVirtualHost *vh);
	void clear();
	bool isEmpty() {
		if (hosts->root.rb_node == NULL && wide_hosts->root.rb_node == NULL) {
			return true;
		}
		return false;
	}
private:
	rb_tree *hosts;
	rb_tree *wide_hosts;
	KWideBindVirtualHost wide_list;
	bool bindWideVirtualHost(KSubVirtualHost *svh);
	bool unbindWideVirtualHost(KSubVirtualHost *svh);
};
#endif
