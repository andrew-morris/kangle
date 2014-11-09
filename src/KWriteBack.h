#ifndef WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#define WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#include <vector>
#include <string.h>
#include <stdlib.h>
#include<list>
#include<string>
#include "global.h"
#include "KMutex.h"
#include "KJump.h"
#include "KXml.h"
#include "server.h"
#include "ksapi.h"
#include "KString.h"
#include "KHttpProtocolParserHook.h"
#ifdef ENABLE_WRITE_BACK
class KHttpRequest;
class KWriteBackParserHook : public KHttpProtocolParserHook
{
public:
	KWriteBackParserHook()
	{
		status_code = STATUS_OK;
	}
	int parseHeader(const char *attr, char *val, int &val_len, bool isFirst)
	{
		if (isFirst) {
			status_code = atoi(val);
			return PARSE_HEADER_NO_INSERT;
		}
		if (strcasecmp(attr, "Status") == 0) {
			status_code = atoi(val);
			return PARSE_HEADER_NO_INSERT;
		}
		if (strcasecmp(attr, "Connection") == 0
			|| strcasecmp(attr, "Content-Length") == 0
			|| strcasecmp(attr, "Transfer-Encoding") == 0) {
			return PARSE_HEADER_NO_INSERT;
		}
		return PARSE_HEADER_SUCCESS;
	}
	int status_code;
};
class KWriteBack:public KJump
{
public:
	KWriteBack()
	{
		ext = cur_config_ext;
		header = NULL;
	}
	void buildRequest(KHttpRequest *rq);
	std::string getMsg();
	void setMsg(std::string msg);
	void buildXML(std::stringstream &s) {
		s << "\t<writeback name='" << name << "'>";
		s << CDATA_START << KXml::encode(getMsg()) << CDATA_END << "</writeback>\n";
	}
	bool ext;
	int status_code;
	KHttpHeader *header;
	KStringBuf body;
};
#endif
#endif
