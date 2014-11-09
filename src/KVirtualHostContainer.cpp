#include "KVirtualHostContainer.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"
inline rb_node *insertVirtualHost(rb_root *root,const char *site,bool &newobj)
{
	struct rb_node **n = &(root->rb_node), *parent = NULL;
	KSubVirtualHost *svh = NULL;
	newobj = true;
	while (*n) {
		svh = (KSubVirtualHost *)((*n)->data);
		int result = strcasecmp(site,svh->host);
		parent = *n;
		if (result < 0)
			n = &((*n)->rb_left);
		else if (result > 0)
			n = &((*n)->rb_right);
		else{
			newobj = false;
		    break;
		}
	}
	if (newobj) {
		rb_node *node = new rb_node;
		node->data = NULL;
		rb_link_node(node, parent, n);
		rb_insert_color(node, root);
		return node;
	}
	return *n;
}
inline rb_node *findVirtualHost(rb_root *root,const char *site)
{
	assert(site && root);
	struct rb_node *node = root->rb_node;
	while (node) {
        KSubVirtualHost *data = (KSubVirtualHost *)(node->data);
		assert(data && data->host);
        int result = strcasecmp(site,data->host);
        if (result < 0)
                node = node->rb_left;
        else if (result > 0)
                node = node->rb_right;
        else
                return node;
    }
	return NULL;
}
KVirtualHostContainer::KVirtualHostContainer()
{
	hosts.rb_node = NULL;
	wideHosts.rb_node = NULL;
	defaultVh = NULL;
}
KVirtualHostContainer::~KVirtualHostContainer()
{
	clear();
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
void KVirtualHostContainer::clear()
{
	 struct rb_node *node;
	 for(;;){
		 node = hosts.rb_node;
		 if(node==NULL){
			 break;
		 }
		 rb_erase(node,&hosts);
		 delete node;
	 }
	 for(;;){
		 node = wideHosts.rb_node;
		 if(node==NULL){
			 break;
		 }
		 rb_erase(node,&wideHosts);
		 delete node;
	 }
	 defaultVh = NULL;
}
query_vh_result KVirtualHostContainer::parseVirtualHost(KHttpRequest *rq,const char *site)
{
	KSubVirtualHost *svh = NULL;
	rb_node *node = findVirtualHost(&hosts,site);
	if(node==NULL){
		site = strchr(site, '.');
		if (site) {
			node = findVirtualHost(&wideHosts,site);
		}	
	}
	if(node){
		assert(node->data);
		svh = (KSubVirtualHost *)node->data;
	} else {
		svh = defaultVh;
	}
	if (svh) {
		if (rq->svh!=svh) {
			//虚拟主机变化了?把老的释放，引用新的
			rq->releaseVirtualHost();
#ifdef ENABLE_VH_RS_LIMIT
			if (!svh->vh->addConnection()) {
				return query_vh_connect_limit;
			}
#endif
			svh->vh->addRef();
			rq->svh = svh;
		}
#ifdef ENABLE_VH_RS_LIMIT
		if (svh->vh->sl) {
			rq->addSpeedLimit(svh->vh->sl);
		}
#endif
#ifdef ENABLE_VH_FLOW
		if (svh->vh->flow) {
			rq->addFlowInfo(svh->vh->flow);
		}
#endif		
		return query_vh_success;
	}
	return query_vh_host_not_found;
}
bool KVirtualHostContainer::unbindVirtualHost(KSubVirtualHost *vh) {
	bool result = false;
	char *site = vh->host;
	rb_node *node = NULL;
	if (strcmp(site, "*") == 0 || strcmp(site,"default")==0) {
		if (vh == defaultVh) {
			defaultVh = NULL;
			return true;
		}
		return false;
	}
	if (site[0] == '.') {
		//site = site + 1;
		node = findVirtualHost(&wideHosts,site);
		if(node && node->data == vh){
			result = true;
			rb_erase(node,&wideHosts);
			delete node;
		}
	} else {
		node = findVirtualHost(&hosts,site);
		if(node && node->data == vh){
			result = true;
			rb_erase(node,&hosts);
			delete node;
		}		
	}
	return result;
}
bool KVirtualHostContainer::bindVirtualHost(KSubVirtualHost *svh) {
	bool result = false;
	bool newNode = false;
	char *site = svh->host;
	rb_node *node = NULL;
	if (strcmp(site, "*") == 0 || strcmp(site, "default") == 0) {
		if (defaultVh == NULL) {
			defaultVh = svh;
			return true;
		}
		if (defaultVh == svh) {
			return true;
		}
		return false;
	}
	if (site[0] == '.') {
		//site = site + 1;
		node = insertVirtualHost(&wideHosts,site,newNode);
		if(newNode){
			node->data = svh;
			result = true;
		} else if(node->data == svh) {
			result = true;
		}
		assert(node->data);	
	} else {
		node = insertVirtualHost(&hosts,site,newNode);
		if(newNode){
			node->data = svh;
			result = true;
		} else if(node->data == svh) {
			result = true;
		}
		assert(node->data);	
	}
	return result;
}
