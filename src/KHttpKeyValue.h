/*
 * KHttpKeyValue.h
 *
 *  Created on: 2010-4-23
 *      Author: keengo
 */

#ifndef KHTTPKEYVALUE_H_
#define KHTTPKEYVALUE_H_
#include "global.h"
#include "KHttpHeader.h"
#define METH_UNSET      0
#define METH_OPTIONS    1
#define METH_GET        2
#define METH_HEAD       3
#define METH_POST       4
#define METH_PUT        5
#define METH_DELETE     6
#define METH_TRACE      7
#define METH_PROPFIND   8
#define METH_PROPPATCH  9
#define METH_MKCOL      10
#define METH_COPY       11
#define METH_MOVE       12
#define METH_LOCK       13
#define METH_UNLOCK     14
#define METH_ACL        15
#define METH_REPORT	    16
#define METH_VERSION_CONTROL  17
#define METH_CHECKIN    18
#define METH_CHECKOUT   19
#define METH_UNCHECKOUT 20
#define METH_SEARCH     21
#define METH_MKWORKSPACE 22
#define METH_UPDATE     23
#define METH_LABEL      24
#define METH_MERGE      25
#define METH_BASELINE_CONTROL 26
#define METH_MKACTIVITY 27
#define METH_CONNECT    28
#define METH_PURGE      29

#define MAX_METHOD      30

typedef struct {
	int key;
	const char *value;
} keyvalue;
class KHttpKeyValue {
public:
	KHttpKeyValue();
	virtual ~KHttpKeyValue();
	static const char *getMethod(int meth);
	static int getMethod(const char *src);
	static const char *getStatus(int status);

};
inline void getRequestLine(int status,kgl_str_t *ret)
{
	switch (status) {	
		case 100:kgl_str_set(ret, "HTTP/1.1 100 Continue\r\n");return;
		case 101:kgl_str_set(ret, "HTTP/1.1 101 Switching Protocols\r\n");return;
		case 102:kgl_str_set(ret, "HTTP/1.1 102 Processing\r\n");return; /* WebDAV */
		case 200:kgl_str_set(ret, "HTTP/1.1 200 OK\r\n");return;
		case 201:kgl_str_set(ret, "HTTP/1.1 201 Created\r\n");return;
		case 202:kgl_str_set(ret, "HTTP/1.1 202 Accepted\r\n");return;
		case 203:kgl_str_set(ret, "HTTP/1.1 203 Non-Authoritative Information\r\n");return;
		case 204:kgl_str_set(ret, "HTTP/1.1 204 No Content\r\n");return;
		case 205:kgl_str_set(ret, "HTTP/1.1 205 Reset Content\r\n");return;
		case 206:kgl_str_set(ret, "HTTP/1.1 206 Partial Content\r\n");return;
		case 207:kgl_str_set(ret, "HTTP/1.1 207 Multi-status\r\n");return; /* WebDAV */
		case 300:kgl_str_set(ret, "HTTP/1.1 300 Multiple Choices\r\n");return;
		case 301:kgl_str_set(ret, "HTTP/1.1 301 Moved Permanently\r\n");return;
		case 302:kgl_str_set(ret, "HTTP/1.1 302 Found\r\n");return;
		case 303:kgl_str_set(ret, "HTTP/1.1 303 See Other\r\n");return;
		case 304:kgl_str_set(ret, "HTTP/1.1 304 Not Modified\r\n");return;
		case 305:kgl_str_set(ret, "HTTP/1.1 305 Use Proxy\r\n");return;
		case 306:kgl_str_set(ret, "HTTP/1.1 306 (Unused)\r\n");return;
		case 307:kgl_str_set(ret, "HTTP/1.1 307 Temporary Redirect\r\n");return;
		case 400:kgl_str_set(ret, "HTTP/1.1 400 Bad Request\r\n");return;
		case 401:kgl_str_set(ret, "HTTP/1.1 401 Unauthorized\r\n");return;
		case 402:kgl_str_set(ret, "HTTP/1.1 402 Payment Required\r\n");return;
		case 403:kgl_str_set(ret, "HTTP/1.1 403 Forbidden\r\n");return;
		case 404:kgl_str_set(ret, "HTTP/1.1 404 Not Found\r\n");return;
		case 405:kgl_str_set(ret, "HTTP/1.1 405 Method Not Allowed\r\n");return;
		case 406:kgl_str_set(ret, "HTTP/1.1 406 Not Acceptable\r\n");return;
		case 407:kgl_str_set(ret, "HTTP/1.1 407 Proxy Authentication Required\r\n");return;
		case 408:kgl_str_set(ret, "HTTP/1.1 408 Request Timeout\r\n");return;
		case 409:kgl_str_set(ret, "HTTP/1.1 409 Conflict\r\n");return;
		case 410:kgl_str_set(ret, "HTTP/1.1 410 Gone\r\n");return;
		case 411:kgl_str_set(ret, "HTTP/1.1 411 Length Required\r\n");return;
		case 412:kgl_str_set(ret, "HTTP/1.1 412 Precondition Failed\r\n");return;
		case 413:kgl_str_set(ret, "HTTP/1.1 413 Request Entity Too Large\r\n");return;
		case 414:kgl_str_set(ret, "HTTP/1.1 414 Request-URI Too Long\r\n");return;
		case 415:kgl_str_set(ret, "HTTP/1.1 415 Unsupported Media Type\r\n");return;
		case 416:kgl_str_set(ret, "HTTP/1.1 416 Requested Range Not Satisfiable\r\n");return;
		case 417:kgl_str_set(ret, "HTTP/1.1 417 Expectation Failed\r\n");return;
		case 422:kgl_str_set(ret, "HTTP/1.1 422 Unprocessable Entity\r\n");return; /* WebDAV */
		case 423:kgl_str_set(ret, "HTTP/1.1 423 Locked\r\n");return; /* WebDAV */
		case 424:kgl_str_set(ret, "HTTP/1.1 424 Failed Dependency\r\n");return; /* WebDAV */
		case 426:kgl_str_set(ret, "HTTP/1.1 426 Upgrade Required\r\n");return; /* TLS */
		case 500:kgl_str_set(ret, "HTTP/1.1 500 Internal Server Error\r\n");return;
		case 501:kgl_str_set(ret, "HTTP/1.1 501 Not Implemented\r\n");return;
		case 502:kgl_str_set(ret, "HTTP/1.1 502 Bad Gateway\r\n");return;
		case 503:kgl_str_set(ret, "HTTP/1.1 503 Service Not Available\r\n");return;
		case 504:kgl_str_set(ret, "HTTP/1.1 504 Gateway Timeout\r\n");return;
		case 505:kgl_str_set(ret, "HTTP/1.1 505 HTTP Version Not Supported\r\n");return;
		case 507:kgl_str_set(ret, "HTTP/1.1 507 Insufficient Storage\r\n");return; /* WebDAV */
		default: kgl_str_set(ret, "HTTP/1.1 999 unknow\r\n");return;
	}
}
inline const char *getRequestLine(int status)
{
	switch(status){	
		case 100:return "HTTP/1.1 100 Continue\r\n";
		case 101:return "HTTP/1.1 101 Switching Protocols\r\n";
		case 102:return "HTTP/1.1 102 Processing\r\n"; /* WebDAV */
		case 200:return "HTTP/1.1 200 OK\r\n";
		case 201:return "HTTP/1.1 201 Created\r\n";
		case 202:return "HTTP/1.1 202 Accepted\r\n";
		case 203:return "HTTP/1.1 203 Non-Authoritative Information\r\n";
		case 204:return "HTTP/1.1 204 No Content\r\n";
		case 205:return "HTTP/1.1 205 Reset Content\r\n";
		case 206:return "HTTP/1.1 206 Partial Content\r\n";
		case 207:return "HTTP/1.1 207 Multi-status\r\n"; /* WebDAV */
		case 300:return "HTTP/1.1 300 Multiple Choices\r\n";
		case 301:return "HTTP/1.1 301 Moved Permanently\r\n";
		case 302:return "HTTP/1.1 302 Found\r\n";
		case 303:return "HTTP/1.1 303 See Other\r\n";
		case 304:return "HTTP/1.1 304 Not Modified\r\n";
		case 305:return "HTTP/1.1 305 Use Proxy\r\n";
		case 306:return "HTTP/1.1 306 (Unused)\r\n";
		case 307:return "HTTP/1.1 307 Temporary Redirect\r\n";
		case 400:return "HTTP/1.1 400 Bad Request\r\n";
		case 401:return "HTTP/1.1 401 Unauthorized\r\n";
		case 402:return "HTTP/1.1 402 Payment Required\r\n";
		case 403:return "HTTP/1.1 403 Forbidden\r\n";
		case 404:return "HTTP/1.1 404 Not Found\r\n";
		case 405:return "HTTP/1.1 405 Method Not Allowed\r\n";
		case 406:return "HTTP/1.1 406 Not Acceptable\r\n";
		case 407:return "HTTP/1.1 407 Proxy Authentication Required\r\n";
		case 408:return "HTTP/1.1 408 Request Timeout\r\n";
		case 409:return "HTTP/1.1 409 Conflict\r\n";
		case 410:return "HTTP/1.1 410 Gone\r\n";
		case 411:return "HTTP/1.1 411 Length Required\r\n";
		case 412:return "HTTP/1.1 412 Precondition Failed\r\n";
		case 413:return "HTTP/1.1 413 Request Entity Too Large\r\n";
		case 414:return "HTTP/1.1 414 Request-URI Too Long\r\n";
		case 415:return "HTTP/1.1 415 Unsupported Media Type\r\n";
		case 416:return "HTTP/1.1 416 Requested Range Not Satisfiable\r\n";
		case 417:return "HTTP/1.1 417 Expectation Failed\r\n";
		case 422:return "HTTP/1.1 422 Unprocessable Entity\r\n"; /* WebDAV */
		case 423:return "HTTP/1.1 423 Locked\r\n"; /* WebDAV */
		case 424:return "HTTP/1.1 424 Failed Dependency\r\n"; /* WebDAV */
		case 426:return "HTTP/1.1 426 Upgrade Required\r\n"; /* TLS */
		case 500:return "HTTP/1.1 500 Internal Server Error\r\n";
		case 501:return "HTTP/1.1 501 Not Implemented\r\n";
		case 502:return "HTTP/1.1 502 Bad Gateway\r\n";
		case 503:return "HTTP/1.1 503 Service Not Available\r\n";
		case 504:return "HTTP/1.1 504 Gateway Timeout\r\n";
		case 505:return "HTTP/1.1 505 HTTP Version Not Supported\r\n";
		case 507:return "HTTP/1.1 507 Insufficient Storage\r\n"; /* WebDAV */
		default: return "HTTP/1.1 999 unknow\r\n";
	}
}
#endif /* KHTTPKEYVALUE_H_ */
