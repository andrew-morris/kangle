/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAM_H_
#define KPOOLABLESTREAM_H_
#include <time.h>
#include "global.h"
#include "KStream.h"
#include "KSocket.h"
#include "KSelectable.h"
#include "KSSLSocket.h"
#define KPoolableStream KStream
enum BadStage
{
	BadStage_Connect,
	BadStage_Send,
	BadStage_Recv
};
class KPoolableSocketContainer;
class KHttpRequest;
/*
 * 可被重复使用的连接。
 */
class KPoolableSocket: public KClientSocket 
#ifdef _WIN32
,public KSelectable
#endif
{
public:
	KPoolableSocket() {
		expireTime = 0;
		container = NULL;
	}
	virtual ~KPoolableSocket();
#ifdef _WIN32
	SOCKET getSockfd()
	{
		return sockfd;
	}
	KHttpRequest *rq;
	//selector id
#ifdef ENABLE_MULTI_POOLS
	int sid;
#endif
#endif
	/*
	 * 连接是否是新的
	 */
	virtual bool isNew() {
		return expireTime == 0;
	}
	virtual bool isSSL()
	{
		return false;
	}
	int getLifeTime();
	void isBad(BadStage stage);
	void isGood();
	/*
	 * 删除连接，看情况是否放入连接池中。
	 * lifeTime = -1 close the connection
	 * lifeTime = 0  use the default lifeTime
	 */
	void destroy(int lifeTime);
	/*
	 * 连接过期时间
	 */
	INT64 expireTime;
	int use_count;
	/*
	 * 关连的连接池容器
	 */
	KPoolableSocketContainer *container;
};
/////////[313]
#endif /* KPOOLABLESTREAM_H_ */
