#ifndef krwlock_h_lsdkjfs0d9fsdfsdafasdfasdf8sdf9
#define krwlock_h_lsdkjfs0d9fsdfsdafasdfasdf8sdf9
#include "KMutex.h"
#ifdef DEAD_LOCK
#define RLock          Lock
#define WLock          Lock
#define KRWLock       KMutex
#else
#ifndef _WIN32
class KRWLock
{
public:
KRWLock()
{
        pthread_rwlock_init(&m_rw_lock,NULL);
}
~KRWLock()
{
                pthread_rwlock_destroy(&m_rw_lock);

}
int RLock2()
{
        return pthread_rwlock_rdlock(&m_rw_lock);
}
int WLock2()
{
         return pthread_rwlock_wrlock(&m_rw_lock);
}
int Unlock()
{
        return pthread_rwlock_unlock(&m_rw_lock);
}
private:
	 pthread_rwlock_t m_rw_lock;

};
#define RLock		RLock2
#define WLock		WLock2
#else
#define RLock		Lock
#define WLock		Lock
#define KRWLock         	KMutex
#endif//_WIN32∂®“ÂΩ· ¯
#endif
#endif
