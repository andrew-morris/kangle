#include "KWriteBack.h"
#include "KHttpObject.h"
#include "KHttpObjectParserHook.h"
#include "malloc_debug.h"
using namespace std;

std::string KWriteBack::getMsg()
{
	std::stringstream s;
	s << getRequestLine(status_code);
	KHttpHeader *h = header;
	while (h) {
		s << h->attr << ": " << h->val << "\r\n";
		h = h->next;
	}
	s << "\r\n";
	if (body.getSize()) {
		s << body.getString();
	}
	return s.str();
}
void KWriteBack::setMsg(std::string msg)
{
	if (header) {
		free_header(header);
		header = NULL;
	}
	body.clean();
	KWriteBackParserHook hook;
	KHttpProtocolParser parser;
	char *buf = strdup(msg.c_str());
	parser.parse(buf,msg.size(),&hook);
	status_code = hook.status_code;
	header = parser.stealHeaders(NULL);
	if (parser.bodyLen>0) {
		body.write_all(parser.body,parser.bodyLen);
	}
	free(buf);
}
void KWriteBack::buildRequest(KHttpRequest *rq)
{
	rq->responseStatus(status_code);
	KHttpHeader *h = header;
	while (h) {
		rq->responseHeader(h->attr,h->attr_len,h->val,h->val_len);
		h = h->next;
	}
	rq->responseHeader(kgl_expand_string("Content-Length"),body.getSize());
/////////[115]
		//connection
		if (TEST(rq->flags,RQ_CONNECTION_CLOSE) || !TEST(rq->flags,RQ_HAS_KEEP_CONNECTION)) {
			rq->responseHeader(kgl_expand_string("Connection"),kgl_expand_string("close"));
		} else {
			rq->responseHeader(kgl_expand_string("Connection"),kgl_expand_string("keep-alive"));
		}
/////////[116]
	rq->buffer.write_all(body.getBuf(),body.getSize());
}
