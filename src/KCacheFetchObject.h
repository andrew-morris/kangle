#ifndef KCACHEFETCHOBJECT_H
#define KCACHEFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
#include "http.h"
#include "KSubRequest.h"
/**
* 缓存物件数据源，仅用于内部请求命中
*/
class KCacheFetchObject : public KFetchObject
{
public:
	void open(KHttpRequest *rq)
	{
		assert(TEST(rq->workModel,WORK_MODEL_INTERNAL));
		KHttpObject *obj = rq->ctx->obj;
		if (TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ)) {
			obj = rq->ctx->old_obj;
		}
		if (obj->data->bodys==NULL) {
			stageEndRequest(rq);
			return;
		}
		hot_buffer = obj->data->bodys;
		readBody(rq);
	}
	void readBody(KHttpRequest *rq)
	{
		assert(hot_buffer);
		assert(rq->sr && rq->sr->ctx->st);
		KWStream *st = rq->sr->ctx->st;
		while (hot_buffer && hot_buffer->used>0) {
			buff *buf = hot_buffer;
			hot_buffer = hot_buffer->next;
			StreamState result = st->write_all(buf->data,buf->used);
			if (result==STREAM_WRITE_FAILED) {
				SET(rq->flags,RQ_CONNECTION_CLOSE);
				break;
			}
			if (try_send_request(rq,true)) {
				return;
			}
		}
		stageEndRequest(rq);
	}
private:
	buff *hot_buffer;
};
#endif
