#ifndef KURL_H
#define KURL_H
#ifdef _WIN32
#pragma warning(disable:4003)
#include <direct.h>
#endif
#include "lib.h"
#include "KString.h"
#include "forwin32.h"
#include "md5.h"

class KUrl {
public:
	~KUrl() {

	}
	KUrl() {
		memset(this, 0, sizeof(KUrl));
	}
	void destroy() {
		IF_FREE(host);
		IF_FREE(path);
		IF_FREE(param);
	}
	int cmpn(const KUrl *a,int n) const {
		int ret = strcasecmp(host,a->host);
		if (ret<0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		return strncmp(path,a->path,n);
	}
	int operator <(const KUrl &a) const {
		int ret = strcasecmp(host, a.host);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		ret = strcmp(path, a.path);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		if (port < a.port) {
			return -1;
		}else if(port > a.port){
			return 1;
		}
		if(param==NULL){
			if(a.param==NULL){
				return 0;
			}else{
				return 1;
			}
		}
		if(a.param==NULL){
			return -1;
		}
		return strcmp(param,a.param);
	}
	KUrl *clone() {
		KUrl *url = new KUrl;
		clone_to(url);
		return url;
	}
	//clone this to url
	void clone_to(KUrl *url) {
		url->host = xstrdup(host);
		url->path = xstrdup(path);
		if (param)
			url->param = xstrdup(param);		
		url->port = port;
		url->proto = proto;
	}
	char *getUrl() {
		KStringBuf s(128);
		if (!getUrl(s)) {
			return NULL;
		}
		return s.stealString();
	}
	char *getVariedOrigParam()
	{
		char *orig_param = strdup(param);
		char *p = strrchr(orig_param,VARY_URL_KEY);
		if (p) {
			*p = '\0';
		}
		return orig_param;
	}
	bool getUrl(KStringBuf &s,bool urlEncode=false) {
		if (host == NULL || path == NULL) {
			return false;
		}
		int defaultPort = 80;
		if(TEST(proto,PROTO_HTTPS)){
			s << "https://";
			defaultPort = 443;
		}else if(TEST(proto,PROTO_FTP)){
			s << "ftp://";
			defaultPort = 21;
		}else{
			s << "http://";
		}
		if(TEST(proto,PROTO_IPV6)){
			s << "[" << host << "]";
		}else{
			s << host;
		}
		if (port != defaultPort) {
			s << ":" << port;
		}
		if (urlEncode) {
			size_t len = strlen(path);
			char *newPath = url_encode(path,len,&len);
			if (newPath) {
				s.write_all(newPath,len);
				free(newPath);
			}
		} else {
			s << path;
		}
		if (param && *param) {
			if (urlEncode) {
				size_t len = strlen(param);
				char *newParam = url_value_encode(param,len,&len);
				if (newParam) {
					s.write_all("?",1);
					s.write_all(newParam,len);
					free(newParam);
				}
			} else {
				s << "?" << param;
			}
		}
		return true;
	}	
	char *host;
	char *path;
	char *param;
	u_short port;
	unsigned char proto;
};
void free_url(KUrl *url);
#endif
