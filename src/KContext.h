#ifndef KCONTEXT_H
#define KCONTEXT_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
class KHttpTransfer;
class KHttpObject;
class KRequestQueue;
class KHttpRequest;
enum modified_type
{
	modified_if_modified,
	modified_if_range,
	modified_if_unmodified
};
class KContext
{
public:
	inline KContext()
	{
		memset(this,0,sizeof(KContext));
	}
	~KContext()
	{
		assert(obj==NULL && old_obj==NULL);
		assert(st == NULL);
	}
	/////////[383]
	void pushObj(KHttpObject *obj);
	void popObj();
	KHttpObject *obj;
	KHttpObject *old_obj;
	//bool last_status;
	bool haveStored;
	bool new_object;
	//lastModified类型
	modified_type mt;
	time_t lastModified;
	INT64 content_range_length;
	int keepAlive;
	//异步读文件时需要的数据
	INT64 left_read;
	KWStream *st;
	void clean();
	void store_obj(KHttpRequest *rq);
	void clean_obj(KHttpRequest *rq);
};

#endif
