#include "KSyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
void KSyncFetchObject::open(KHttpRequest *rq)
{
	assert(rq->list==KGL_LIST_SYNC);
	kassert(TEST(rq->flags,RQ_SYNC));
	/////////[29]
		rq->c->socket->set_time(conf.time_out);
#ifndef _WIN32
		rq->c->socket->setblock();
#else
		if (TEST(rq->workModel, WORK_MODEL_SSL)) {
			rq->c->socket->setblock();
		}
#endif
		/////////[30]
	process(rq);
	/////////[31]
#ifndef _WIN32
		rq->c->socket->setnoblock();
#else
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			rq->c->socket->setnoblock();
		}
#endif
	/////////[32]
	CLR(rq->flags,RQ_SYNC);
	stageEndRequest(rq);
}
