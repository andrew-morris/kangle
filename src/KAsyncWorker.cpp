#include "KAsyncWorker.h"
#include "KThreadPool.h"
FUNC_TYPE FUNC_CALL kasyncWorkerThread(void *param)
{
	KAsyncWorker *worker = (KAsyncWorker *)param;
	worker->workThread();
	KTHREAD_RETURN;
}
void KAsyncWorker::start(void *data,asyncWorkerCallBack callBack)
{
	KAsyncParam *rq = new KAsyncParam;
	rq->callBack = callBack;
	rq->data = data;
	rq->next = NULL;
	lock.Lock();
	queue++;
	if (last==NULL) {
		assert(head==NULL);
		head = rq;
		last = rq;
	} else {
		last->next = rq;
		last = rq;
	}
	if (worker>=maxWorker) {
		lock.Unlock();
		return;
	}
	worker++;
	lock.Unlock();
	addRef();
	if (!m_thread.start(this,kasyncWorkerThread)) {
		lock.Lock();
		worker--;
		lock.Unlock();
		release();
	}
}
void KAsyncWorker::workThread()
{
	for (;;) {
		KAsyncParam *rq = NULL;
		lock.Lock();
		if (head==NULL) {
			worker--;
			lock.Unlock();
			break;
		}
		queue--;
		rq = head;
		head = head->next;
		if (head==NULL) {
			last = NULL;
		}
		lock.Unlock();
		rq->callBack(rq->data);
		delete rq;
	}
	release();
}
