#ifndef KSTATICFETCHOBJECT_H
#define KSTATICFETCHOBJECT_H
#include <stdlib.h>
#include "global.h"
#include "KFetchObject.h"
#include "KFile.h"
class KAsyncData
{
public:
	KAsyncData()
	{
		memset(this,0,sizeof(KAsyncData));
	}
	char buf[8192];
#ifdef _WIN32
	OVERLAPPED ol;
#endif
};
class KStaticFetchObject : public KFetchObject 
{
public:	
	KStaticFetchObject()
	{
		ad = NULL;
	}
	~KStaticFetchObject()
	{
		if (ad) {
			delete ad;
		}
	}
	void open(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);
	void handleAsyncReadBody(KHttpRequest *rq,int got);	
	void asyncReadBody(KHttpRequest *rq);
	void syncReadBody(KHttpRequest *rq);
private:
	KFile fp;
	KAsyncData *ad;
};
#endif
