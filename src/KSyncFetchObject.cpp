#include "KSyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
void KSyncFetchObject::open(KHttpRequest *rq)
{
	assert(rq->list==KGL_LIST_SYNC);
	kassert(TEST(rq->flags,RQ_SYNC));
	rq->server->set_time(conf.time_out);
#ifndef _WIN32
	rq->server->setblock();
#else
	if (TEST(rq->workModel,WORK_MODEL_SSL)) {
		rq->server->setblock();
	}
#endif
	process(rq);
#ifndef _WIN32
	rq->server->setnoblock();
#else
	if (TEST(rq->workModel,WORK_MODEL_SSL)) {
		rq->server->setnoblock();
	}
#endif
	CLR(rq->flags,RQ_SYNC);
	stageEndRequest(rq);
}
