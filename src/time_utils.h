#ifndef TIMEUTILS_H
#define TIMEUTILS_H
#include "KMutex.h"
#include "extern.h"
#include "server.h"
#include "KConfig.h"
#ifndef _WIN32
#include <sys/time.h>
#endif
const char * mk1123time(time_t time, char *buf, int size);
const char *log_request_time(char *tstr,size_t buf_size);

//#include "lib.h"
extern volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
extern volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
extern volatile INT64 kgl_current_msec;
extern volatile time_t kgl_current_sec; 

extern KMutex timeLock;
/////////[60]
inline void setActive()
{
		/////////[61]
}
inline bool updateTime()
{
	struct timeval   tv;
	gettimeofday(&tv,NULL);
	kgl_current_msec = (INT64) tv.tv_sec * 1000 + (tv.tv_usec/1000);
	if(kgl_current_sec!=tv.tv_sec){
		timeLock.Lock();
		kgl_current_sec = tv.tv_sec;
		mk1123time(kgl_current_sec, (char *)cachedDateTime, sizeof(cachedDateTime));
		log_request_time((char *)cachedLogTime,sizeof(cachedLogTime));
		timeLock.Unlock();
		setActive();
		if (configReload) {
			do_config(false);
			configReload = false;
		}
		if (quit_program_flag>0) {
			shutdown();
		}
		return true;
	}
	return false;
}
#endif

