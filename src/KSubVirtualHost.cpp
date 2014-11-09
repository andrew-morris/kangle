/*
 * KSubVirtualHost.cpp
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <sstream>
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KHtAccess.h"
#include "http.h"
#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
#include "malloc_debug.h"
/////////[343]
std::string htaccess_filename;
using namespace std;
KSubVirtualHost::KSubVirtualHost(KVirtualHost *vh) {
	this->vh = vh;
	host = NULL;
	dir = NULL;
	doc_root = NULL;
#ifdef ENABLE_SUBDIR_PROXY
	/////////[344]
	ip = NULL;
	dst = NULL;
	type = subdir_local;
	lifeTime = 0;
#endif	
	allSuccess = true;
	fromTemplete = false;
}
KSubVirtualHost::~KSubVirtualHost() {
	if (host) {
		xfree(host);
	}
	if (dir) {
		xfree(dir);
	}
	if (doc_root) {
		xfree(doc_root);
	}
#ifdef ENABLE_SUBDIR_PROXY
	if (dst) {
		dst->destroy();
		delete dst;
	}
#endif

}
void KSubVirtualHost::setHost(const char *host)
{
	if (*host=='*' && strcmp(host,"*")!=0) {
		host ++;
	}
	this->host = xstrdup(host);
	char *p = strchr(this->host,'|');
	if (p) {
		*p = '\0';
		this->dir = strdup(p+1);
	}
}
void KSubVirtualHost::setDocRoot(const char *doc_root, const char *dir) {
#ifndef ENABLE_SUB_VIRTUALHOST
	dir = NULL;
#endif
	if (this->dir==NULL) {
		if (dir == NULL) {
			this->dir = xstrdup("/");
		} else {
			this->dir = xstrdup(dir);
		}
	}
#ifdef ENABLE_SUBDIR_PROXY
	if (strncasecmp(this->dir,"http://",7)==0) {
		type = subdir_proxy;
		this->doc_root = strdup(doc_root);
		if (dst) {
			dst->destroy();
		} else {
			dst = new KUrl;
		}
		if (!parse_url(this->dir,dst)) {
			dst->destroy();
			delete dst;
			dst = NULL;
			klog(KLOG_ERR,"cann't parse url [%s]\n",this->dir);
		}
		if (dst && dst->param) {
			char *t = strstr(dst->param,"life_time=");
			if (t) {
				lifeTime = atoi(t+10);
			}
			ip = strstr(dst->param,"ip=");
		/////////[345]
			if (ip) {
				ip += 3;
				char *t = strchr(ip,'&');
				if (t) {
					*t = '\0';
				}
			}
		}
		return;
	}
	type = subdir_local;
#endif
	KFileName::tripDir3(this->dir,'/');
	char *sub_doc_root = KFileName::concatDir(doc_root, this->dir);
#ifdef ENABLE_VH_RUN_AS
	if(vh->add_dir.size()>0){
		this->doc_root = KFileName::concatDir(sub_doc_root,vh->add_dir.c_str());
		xfree(sub_doc_root);
	}else{
		this->doc_root = sub_doc_root;
	}
#else
	this->doc_root = sub_doc_root;
#endif

	size_t doc_len = strlen(this->doc_root);
	if(this->doc_root[doc_len-1]!='/'
#ifdef _WIN32
		&& this->doc_root[doc_len-1]!='\\'
#endif
		){
		sub_doc_root = (char *)xmalloc(doc_len+2);
		memcpy(sub_doc_root,this->doc_root,doc_len);
		sub_doc_root[doc_len] = PATH_SPLIT_CHAR;
		sub_doc_root[doc_len+1] = '\0';
		xfree(this->doc_root);
		this->doc_root = sub_doc_root;
	}
	KFileName::tripDir3(this->doc_root,PATH_SPLIT_CHAR);
}
bool KSubVirtualHost::bindFile(KHttpRequest *rq, KHttpObject *obj,bool &exsit,KAccess **htresponse,bool &handled) {
	//	char *tripedDir = KFileName::tripDir2(rq->url->path, '/');
#ifdef _WIN32
	char *c = rq->url->path + strlen(rq->url->path) - 1;
	if(*c=='.' || *c==' '){
		return false;
	}
#endif
#if 1
	if (!TEST(rq->workModel,WORK_MODEL_INTERNAL) && vh->htaccess.size()>0) {
		char *path = xstrdup(rq->url->path);
		int prefix_len = 0;
		for (;;) {
			char *hot = strrchr(path, '/');
			if (hot == NULL) {
				break;
			}
			if (prefix_len == 0) {
				prefix_len = hot - path;
			}
			*hot = '\0';
			char *apath = vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,path);
			KFileName htfile;
			bool htfile_exsit;
			if (apath) {
				htfile_exsit = htfile.setName(apath, vh->htaccess.c_str(), 0);
				xfree(apath);
			} else {
				stringstream s;
				s << doc_root << path;
				htfile_exsit
					= htfile.setName(s.str().c_str(), vh->htaccess.c_str(), 0);
			}
			if (htfile_exsit) {
				if ((*htresponse)==NULL) {
					(*htresponse) = new KAccess;
					(*htresponse)->setType(RESPONSE);
				}
				KAccess *htrequest = new KAccess;
				htrequest->setType(REQUEST);
				if (makeHtaccess(path,&htfile,htrequest,*htresponse)) {
					if (htrequest->check(rq,obj)==JUMP_DENY) {
						handled = true;
						xfree(path);
						delete htrequest;
						delete (*htresponse);
						*htresponse = NULL;
						return true;
					}
				}
				delete htrequest;
			}
			//todo:check rebind file
			//	if(filencmp(,rq->url->path)
		}
		xfree(path);
	}
#endif
#ifdef ENABLE_SUBDIR_PROXY
	if (type==subdir_proxy) {
		if (rq->fetchObj==NULL && dst && dst->host) {
			if (*(dst->host)=='-') {
				rq->fetchObj = new KHttpProxyFetchObject();
				return true;
			}
			const char *tssl = NULL;
			/////////[346]
			int tport = dst->port;
			if (dst->port == 0) {
				tport = rq->url->port;
				/////////[347]
			}
			rq->fetchObj = cdnContainer.get(ip,dst->host,tport,tssl,lifeTime);
			return true;
		}
	}
#endif
	if (rq->file) {
		//重新绑定过,因为有可能重写了
		delete rq->file;
		rq->file = NULL;
	}
	return bindFile(rq,exsit,false,true);
}
bool KSubVirtualHost::bindFile(KHttpRequest *rq,bool &exsit,bool searchDefaultFile,bool searchAlias)
{
	KFileName *file = new KFileName;
	if (!searchAlias || !vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,rq->url->path,file,exsit,rq->getFollowLink())) {
		exsit = file->setName(doc_root, rq->url->path, rq->getFollowLink());
	}
	kassert(rq->file == NULL);
	rq->file = file;
	if (searchDefaultFile && file->isDirectory()) {
		KFileName *defaultFile = NULL;
		if (vh->getIndexFile(rq,&defaultFile,NULL)) {
			delete rq->file;
			rq->file = defaultFile;
		}
	}
	return true;
}
bool KSubVirtualHost::makeHtaccess(const char *prefix,KFileName *file,KAccess *request,KAccess *response)
{
	KApacheConfig htaccess(true);
	htaccess.setPrefix(prefix);
	std::stringstream s;
	if (htaccess.load(file,s)) {
		//KAccess *access = new KAccess;
		//access->type = REQUEST;
		//access->qName = "request";
		KXml xmlParser;
		xmlParser.addEvent(request);
		xmlParser.addEvent(response);
		bool result=false;
		try {
			result = xmlParser.parseString(s.str().c_str());
		} catch(KXmlException &e) {
			fprintf(stderr,"%s",e.what());
			return false;
		}
		return true;
	}
	return false;
}
char *KSubVirtualHost::mapFile(const char *path) {
	char *new_path = vh->alias(true,path);
	if (new_path) {
		return new_path;
	}
	return KFileName::concatDir(doc_root, path);
}

