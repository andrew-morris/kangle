#include "KVirtualHostContainer.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"
#include "KList.h"

int site_cmp(void *k1,void *k2)
{
	const char *site = (const char *)k1;
	KSubVirtualHost *svh = (KSubVirtualHost *)k2;
	return strcmp(site,svh->bind_host);
}
int wide_tree_cmp(void *k1,void *k2)
{
	KSubVirtualHost *svh = (KSubVirtualHost *)k1;
	KWideBindVirtualHost *wbvh = (KWideBindVirtualHost *)k2;
	if (wbvh->svh > svh) {
		return 1;
	}
	if (wbvh->svh == svh) {
		return 0;
	}
	return -1;
}
int site_wide_cmp(const char *site,KWideBindVirtualHost *bvh)
{
	return strncmp(site,bvh->svh->bind_host,bvh->svh->bind_host_len);
}
KVirtualHostContainer::KVirtualHostContainer()
{

	hosts = rbtree_create();
	wide_hosts = rbtree_create();
	klist_init(&wide_list);
}
KVirtualHostContainer::~KVirtualHostContainer()
{
	clear();
	rbtree_destroy(hosts);
	rbtree_destroy(wide_hosts);
}
void KVirtualHostContainer::removeVirtualHost(KVirtualHost *vh)
{
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		unbindVirtualHost((*it2));
	}
}
void KVirtualHostContainer::addVirtualHost(KVirtualHost *vh)
{
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		if(!bindVirtualHost((*it2)) ) {
			(*it2)->allSuccess = false;
		}
	}
}
static iterator_ret bindVirtualHostIterator(void *data,void *argv)
{
	return iterator_remove_continue;
}
static iterator_ret wideBindVirtualHostIterator(void *data,void *argv)
{
	KWideBindVirtualHost *wbvh = (KWideBindVirtualHost *)data;
	delete wbvh;
	return iterator_remove_continue;
}
void KVirtualHostContainer::clear()
{
	rbtree_iterator(hosts,bindVirtualHostIterator,NULL);
	rbtree_iterator(wide_hosts,wideBindVirtualHostIterator,NULL);
	klist_init(&wide_list);
}
query_vh_result KVirtualHostContainer::parseVirtualHost(KSubVirtualHost **rq_svh,const char *site)
{
	KSubVirtualHost *svh = NULL;
	rb_node *node = rbtree_find(hosts,(void *)site,site_cmp);
	if (node) {
		assert(node->data);
		svh = (KSubVirtualHost *)node->data;
		assert(svh);
	} else {
		KWideBindVirtualHost *pos;
		klist_foreach (pos,&wide_list) {
			if (site_wide_cmp(site,pos)==0) {
				svh = pos->svh;
				break;
			}
		}
	}
	if (svh==NULL) {
		return query_vh_host_not_found;
	}
	if ((*rq_svh)!=svh) {
		//虚拟主机变化了?把老的释放，引用新的
		if (*rq_svh) {
			(*rq_svh)->release();
			(*rq_svh) = NULL;
		}
#ifdef ENABLE_VH_RS_LIMIT
		if (!svh->vh->addConnection()) {
			return query_vh_connect_limit;
		}
#endif
		svh->vh->addRef();
		(*rq_svh) = svh;
	}
	return query_vh_success;	
}
bool KVirtualHostContainer::unbindVirtualHost(KSubVirtualHost *svh) {
	assert(svh->bind_host);
	if (svh->bind_host==NULL) {
		return false;
	}
	if (svh->wide) {
		return unbindWideVirtualHost(svh);
	}
	rb_node *node = rbtree_find(hosts,svh->bind_host,site_cmp);
	if (node==NULL) {
		return false;
	}
	KSubVirtualHost *bvh = (KSubVirtualHost *)node->data;
	if (bvh!=svh) {
		return false;
	}
	rbtree_remove(hosts,node);
	return true;
}
bool KVirtualHostContainer::bindVirtualHost(KSubVirtualHost *svh) {
	assert(svh->bind_host);
	if (svh->wide) {
		return bindWideVirtualHost(svh);
	}
	int new_flag;
	rb_node *node = rbtree_insert(hosts,svh->bind_host,&new_flag,site_cmp);
	if (new_flag) {
		node->data = svh;
		return true;
	}
	return svh==node->data;
}
bool KVirtualHostContainer::bindWideVirtualHost(KSubVirtualHost *svh)
{
	int new_flag;
	rb_node *node = rbtree_insert(wide_hosts,svh,&new_flag,wide_tree_cmp);
	if (!new_flag) {
		return true;
	}
	KWideBindVirtualHost *wbvh = new KWideBindVirtualHost(svh);
	node->data = (KWideBindVirtualHost *)wbvh;
	KWideBindVirtualHost *pos;
	klist_foreach (pos,&wide_list) {
		if (strcmp(svh->bind_host,pos->svh->bind_host)==0) {
			delete wbvh;
			rbtree_remove(wide_hosts,node);
			return false;
		}
		if (svh->bind_host_len > pos->svh->bind_host_len) {
			klist_insert(pos,wbvh);
			return true;
		}
	}
	klist_append(&wide_list,wbvh);
	return true;
}
bool KVirtualHostContainer::unbindWideVirtualHost(KSubVirtualHost *svh)
{
	rb_node *node = rbtree_find(wide_hosts,svh,wide_tree_cmp);
	if (node==NULL) {
		return false;
	}
	KWideBindVirtualHost *wbvh = (KWideBindVirtualHost *)node->data;
	assert(wbvh->svh == svh);
	rbtree_remove(wide_hosts,node);
	klist_remove(wbvh);
	delete wbvh;
	return true;
}
