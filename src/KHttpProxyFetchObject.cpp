/*
 * KHttpProxyFetchObject.cpp
 *
 *  Created on: 2010-4-20
 *      Author: keengo
 */
#include "do_config.h"
#include "KHttpProxyFetchObject.h"
#include "lib.h"
#include "http.h"
#include "log.h"
#include "KHttpProtocolParser.h"
#include "KHttpObjectParserHook.h"
#include "KPoolableSocketContainer.h"
#include "KRewriteMarkEx.h"
#include "malloc_debug.h"
#include "lang.h"
void print_buff(buff *buf) {
	while (buf && buf->used > 0) {
		char *s = (char *) xmalloc(buf->used+1);
		memcpy(s, buf->data, buf->used);
		s[buf->used] = '\0';
		printf("%s", s);
		xfree(s);
		buf = buf->next;
	}
}


Parse_Result KHttpProxyFetchObject::parseHead(KHttpRequest *rq,char *buf,int len)
{
	//fwrite(buf,1,len,stdout);
	assert(header && hot);
	switch(parser.parse(header,hot-header,&hook)){
		case HTTP_PARSE_FAILED:
			//重置hot,这里一定要hot=NULL,因为nextBody里面要用到。
			hot = NULL;
			return Parse_Failed;
		case HTTP_PARSE_SUCCESS:
			if (rq->ctx->obj->data->status_code==100) {
				parser.setStarted(false);
				rq->ctx->obj->data->status_code = 0;
				if (parser.bodyLen>0) {
					return parseHead(rq,buf,len);
				}
				return Parse_Continue;
			}
			rq->ctx->obj->data->headers = parser.stealHeaders(rq->ctx->obj->data->headers);
			//重置hot
			hot = NULL;			
			return Parse_Success;
	}
	return Parse_Continue;
}
void KHttpProxyFetchObject::buildHead(KHttpRequest *rq)
{
	hook.init(rq->ctx->obj,rq);
	KSocketBuffer &s = buffer;
	struct KHttpHeader *av;
	const char *connectionState = "close";
	const char *meth = rq->getMethod();
	char ips[MAXIPLEN];
	KUrl *url = rq->url;
	if (TEST(rq->filter_flags,RF_PROXY_RAW_URL)) {
		url = &rq->raw_url;
	}
	if (meth == NULL)
		return;
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	int defaultPort = 80;
	char *path = url->path;
	if (url==rq->url && TEST(rq->flags,RQ_URL_ENCODE)) {
		size_t path_len;
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	s << meth << " ";
	/////////[95]
	s << path;
	if (url->param) {
		if (TEST(rq->flags,RQ_URL_VARIED) && url==rq->url) {
			char *orig_param = url->getVariedOrigParam();
			if (*orig_param) {
				s << "?" << orig_param;
			}
			free(orig_param);
		} else {
			s << "?" << url->param;
		}
	}
	s << " HTTP/" << (int)rq->http_major << "." << (int)rq->http_minor << "\r\n";
	s << "Host: " << url->host;
	if (TEST(url->proto,PROTO_HTTPS)) {
		defaultPort = 443;
	}
	if (url->port != defaultPort) {
		s << ":" << url->port;
	}
	s << "\r\n";
	av = rq->parser.getHeaders();
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif
		if (TEST(rq->filter_flags,RF_X_REAL_IP) && is_attr(av, "X-Real-IP")) {
			goto do_not_insert;
		}
		if (is_attr(av,"Connection")) {
			goto do_not_insert;
		}
		if (!TEST(rq->filter_flags,RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			s << "X-Forwarded-For: " << av->val << ",";
			rq->server->get_remote_ip(ips,sizeof(ips));
			s << ips << "\r\n";
			goto do_not_insert;

		}
		if (is_attr(av,"Host")) {
			goto do_not_insert;
		}
		if (TEST(rq->flags,RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
/////////[96]
		if (is_attr(av, "Via") && TEST(rq->filter_flags,RF_VIA)) {
			if (via_inserted) {
				goto do_not_insert;
			}
			insert_via(rq, s, av->val);
			via_inserted = true;
			goto do_not_insert;
		}
		//*/
		s << av->attr << ": " << av->val << "\r\n";
		do_not_insert: av = av->next;
	}
	if (rq->ctx->lastModified != 0) {
		char mk1123buff[50];
		mk1123time(rq->ctx->lastModified, mk1123buff, sizeof(mk1123buff));
		if (rq->ctx->mt == modified_if_range) {
			s << "If-Range: ";
		} else {
			s << "If-Modified-Since: ";
		}
		s << mk1123buff << "\r\n";
	}
/////////[97]
	if (TEST(rq->workModel,WORK_MODEL_INTERNAL)) {
		s << "User-Agent: " << PROGRAM_NAME << "/" << VERSION << "\r\n";
	}
	if (!TEST(rq->filter_flags,RF_UPSTREAM_NOKA) && client->getLifeTime()>0) {
		connectionState = "keep-alive";
	}
	s << "Connection: " << connectionState << "\r\n";
	if (!TEST(rq->filter_flags,RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
		rq->server->get_remote_ip(ips,sizeof(ips));
		s << "X-Forwarded-For: " << ips << "\r\n";
	}
	if (TEST(rq->filter_flags,RF_VIA) && !via_inserted) {
		insert_via(rq, s, NULL);
	}
	if (TEST(rq->filter_flags,RF_X_REAL_IP)) {
		s << "X-Real-IP: " << rq->getClientIp() << "\r\n";
	}
	/////////[98]
	s << "\r\n";
	//s.print();
	//	s.add("\r\n", 3);
	if (path != url->path) {
		xfree(path);
	}
}

