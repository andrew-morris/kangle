/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KUPSTREAMSELECTABLE_H
#define KUPSTREAMSELECTABLE_H
#include <time.h>
#include "global.h"
#include "KStream.h"
#include "KSocket.h"
#include "KConnectionSelectable.h"
#include "KSSLSocket.h"
#define KPoolableStream KStream
enum BadStage
{
	/* BadStage_Connect BadStage_TrySend 可以重试 */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess 不可以重试 */
	BadStage_SendSuccess,
};
class KPoolableSocketContainer;
/////////[272]
class KHttpRequest;
/*
 * 可被重复使用的连接。
 */
class KUpstreamSelectable : public KConnectionSelectable
{
public:
	KUpstreamSelectable();
	
	/*
	 * 连接是否是新的
	 */
	bool isNew() {
		return expireTime == 0;
	}
	int getLifeTime();
	void isBad(BadStage stage);
	void isGood();
	void connect(KHttpRequest *rq,resultEvent result);
	/* 异步读 */
	void upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	/* 异步写 */
	void upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);

	/*
	 * 删除连接，看情况是否放入连接池中。
	 * lifeTime = -1 close the connection
	 * lifeTime = 0  use the default lifeTime
	 */
	void gc(int lifeTime);
	/*
	 * 连接过期时间
	 */
	INT64 expireTime;
	int use_count;
	/*
	 * 关连的连接池容器
	 */
	KPoolableSocketContainer *container;
	/////////[273]
protected:
	~KUpstreamSelectable();
};
#endif /* KPOOLABLESTREAM_H_ */
