/*
 * KPoolableStreamContainer.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAMCONTAINER_H_
#define KPOOLABLESTREAMCONTAINER_H_
#include <list>
#include "global.h"
#include "KPoolableSocket.h"
#include "KString.h"
#include "KMutex.h"
#include "KCountable.h"
#include "time_utils.h"
/*
 * 连接池容器类
 */
class KPoolableSocketContainer: public KCountableEx {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();
	KPoolableSocket *getPoolSocket();
	/*
	回收连接
	close,是否关闭
	lifeTime 连接时间
	*/
	virtual void gcSocket(KPoolableSocket *st,int lifeTime);
	void bind(KPoolableSocket *st);
	void unbind(KPoolableSocket *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * 设置连接超时时间
	 */
	void setLifeTime(int lifeTime);
	/*
	 * 定期刷新删除过期连接
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * 清除所有连接
	 */
	void clean();
	/*
	 * 得到连接数
	 */
	unsigned getSize() {
		lock.Lock();
		unsigned size = pools.size();	
		lock.Unlock();
		return size;
	}
	//isBad,isGood用于监控连接情况
	virtual void isBad(KPoolableSocket *st,BadStage stage)
	{
	}
	virtual void isGood(KPoolableSocket *st)
	{
	}
#ifdef HTTP_PROXY
	virtual void buildHttpAuth(KWStream &s)
	{
	}
#endif
protected:
	/*
	 * 把连接真正放入池中
	 */
	void putPoolSocket(KPoolableSocket *st);
	/*
		通知事件.
		ev = 0 关闭
		ev = 1 放入pool
	*/
	//virtual void noticeEvent(int ev,KPoolableSocket *st)
	//{

	//}

	KPoolableSocket *internalGetPoolSocket();
	int lifeTime;
	std::list<KPoolableSocket *> pools;
	KMutex lock;
private:
	void refreshPool(std::list<KPoolableSocket *> *pools)
	{
		std::list<KPoolableSocket *>::iterator it2;
		for (it2 = pools->end(); it2 != pools->begin();) {
			it2--;
			if ((*it2)->expireTime <= kgl_current_msec) {
				assert((*it2)->container == NULL);
				delete (*it2);
				it2 = pools->erase(it2);
			} else {
				break;
			}
		}
	}

};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
