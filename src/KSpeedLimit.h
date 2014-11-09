#ifndef KSPEEDLIMIT_H
#define KSPEEDLIMIT_H
#include "KCountable.h"
#include "time_utils.h"
class KSpeedLimit : public KCountableEx
{
public:
	KSpeedLimit()
	{
		current_send_time = kgl_current_msec;
		min_limit_size = 0;
		speed_limit = 1048576;
		totalSend = 0;
		fragment = 0;
	}
	void setSpeedLimit(int min_limit_size,int speed_limit)
	{
		this->min_limit_size = min_limit_size;
		if(speed_limit<=0){
			speed_limit = 1048576;
		}
		this->speed_limit = speed_limit;
	}
	int getSpeedLimit()
	{
		return speed_limit;
	}
	/*
	inline INT64 getSendTime(int len,INT64 current_msec)
	{
		if (min_limit_size>0) {
			totalSend += len;
			if (totalSend<=min_limit_size) {
				return 0;
			}
		}
		refsLock.Lock();
		//debug("len=%d,fragment=%d\n",len,fragment);	
		len = fragment + len;
		INT64 sleep_time = len * 10 / speed_limit;
		fragment = len - (int)(sleep_time * speed_limit / 10);
		sleep_time *= 100;
		//debug("sleep_time=%d,fragment=%d\n",(int)sleep_time,fragment);
		current_send_time += sleep_time;
		if (current_send_time < current_msec) {
			current_send_time = current_msec;
			refsLock.Unlock();
			return 0;
		}
		refsLock.Unlock();
		return current_send_time;
	}
	*/
	int getSleepTime(int len)
	{
		
		refsLock.Lock();
		len = fragment + len;
		int sleep_time = len * 10 / speed_limit;
		fragment = len - (int)(sleep_time * speed_limit / 10);
		sleep_time *= 100;
		current_send_time += sleep_time;
		if (current_send_time<kgl_current_msec) {
			current_send_time = kgl_current_msec;
		}
		refsLock.Unlock();
		return sleep_time;
	}
private:
	INT64 current_send_time;
	INT64 totalSend;
	int fragment;
	int speed_limit;
	int min_limit_size;
};
class KSpeedLimitHelper
{
public:
	KSpeedLimitHelper(KSpeedLimit *sl)
	{
		next = NULL;
		this->sl = sl;
		sl->addRef();
	}
	~KSpeedLimitHelper()
	{
		if (sl) {
			sl->release();
		}
	}
	KSpeedLimitHelper *next;
	KSpeedLimit *sl;
};
#endif
