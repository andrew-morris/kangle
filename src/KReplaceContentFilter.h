#ifndef KREPLACECONTENTFILTER_H
#define KREPLACECONTENTFILTER_H
#include "KHttpStream.h"
#include "KHttpRequest.h"
class KReplaceContentMark;
typedef bool (*replaceContentCallBack)(void *param,KRegSubString *sub_string,int *ovector,KStringBuf *st);
typedef void (*replaceContentEndCallBack)(void *param);
class KReplaceContentFilter : public KHttpStream
{
public:
	KReplaceContentFilter();
	~KReplaceContentFilter();
	StreamState write_all(const char *buf, int len);
	StreamState write_end();
	void setBuffer(int max_buffer)
	{
		this->max_buffer = max_buffer;
	}
	void setHook(replaceContentCallBack callBack,replaceContentEndCallBack endCallBack,void *param,KReg *reg)
	{
		this->callBack = callBack;
		this->endCallBack = endCallBack;
		this->param = param;
		this->reg = reg;
	}
private:
	bool writeBuffer(const char *str,int len);
	bool dumpBuffer();
	KReg *reg;
	char *prevData;
	int prevDataLength;
	bool stoped;
	int max_buffer;
	KBuffer b;
	replaceContentCallBack callBack;
	replaceContentEndCallBack endCallBack;
	void *param;
};
#endif
