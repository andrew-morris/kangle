/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "global.h"
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <time.h>
#ifndef _WIN32
#include <dirent.h>
#endif
#include "cache.h"
#include "utils.h"
#include "log.h"
#include "server.h"

#include "utils.h"
#include "KThreadPool.h"
#include "lib.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KSelectorManager.h"
#include "malloc_debug.h"
#include "KHttpObjectHash.h"
/////////[219]
#include "KVirtualHostManage.h"
#include "KProcessManage.h"
#include "KProcess.h"
#include "KLogManage.h"
#include "KHttpServerParser.h"
#include "malloc_debug.h"
#include "KHttpDigestAuth.h"
#include "KObjectList.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KCdnContainer.h"
#include "KWriteBackManager.h"
#include "md5.h"

void list_all_malloc();
using namespace std;
volatile bool configReload = false;
volatile bool cur_config_ext = false;
bool dump_memory_object = false;
volatile int stop_service_sig = 0;
string get_service_to_name(int port);
static bool flush_mem_flag = true;
volatile bool autoupdate_thread_started = false;
/////////[220]
std::string md5sum(FILE *fp)
{
        KMD5_CTX context;
        unsigned char digest[17];
        char buf[1024];
        KMD5Init (&context);
        for(;;){
                int len=fread(buf,1,sizeof(buf),fp);
                if(len<=0)
                        break;
                KMD5Update(&context,(unsigned char *)buf, len);
                if(len!=sizeof(buf)){
                        break;
                }
        }
        KMD5Final (digest, &context);
		for(int i=0;i<16;i++){
			sprintf(buf+2*i,"%02x",digest[i]);
		}
        buf[32]=0;
        return buf;
}
void get_cache_size(INT64 &total_mem_size, INT64 &total_disk_size) {
	cache.getSize(total_mem_size,total_disk_size);
}
/////////[221]
void flush_mem_cache(void) {
	INT64 disk_cache = 0;
#ifdef ENABLE_DISK_CACHE
	disk_cache = conf.disk_cache;
#endif
	cache.flush(conf.mem_cache,disk_cache);
}
/*
void reloadVirtualHostConfig() {
#ifndef HTTP_PROXY
	vhd.clear();
	conf.gvm->updateAllVirtualHost();
	conf.gvm->load();
	conf.gvm->checkAllVirtualHost();
#endif
}
*/
FUNC_TYPE FUNC_CALL time_thread(void* arg) {
	unsigned i = rand();
	//	quit_program_flag = PROGRAM_NO_QUIT;
	int sleep_time = GC_SLEEP_TIME;
#ifdef MALLOCDEBUG
	void start_hook_alloc();
	if (conf.mallocdebug) {
		start_hook_alloc();
	}
#endif
/////////[222]
	time_t nowTime = time(NULL);
	for(;;){
		i++;
		sleep_time = GC_SLEEP_TIME - (int)(time(NULL) - nowTime);
		if (sleep_time>0) {
			sleep(sleep_time);
		}
		nowTime = time(NULL);
/*
		if (quit_program_flag == PROGRAM_QUIT_IMMEDIATE) {
			break;
		}
*/
/*
		if (quit_program_flag == PROGRAM_QUIT_CLOSE_CONNECTION) {
			debug("total_connect = %d\n",total_connect);
			sleep_time = 1;
			check_graceful_shutdown();
			continue;
		}
*/

//*/
		//if (stop_service_sig > 0) {
		//	stopService(stop_service_sig);
		//}
		/////////[223]
/////////[224]
		spProcessManage.refresh(nowTime);
		conf.gam->refreshCmd(nowTime);
/////////[225]
#ifdef MALLOCDEBUG
		if (dump_memory_object) {
			dump_memory(0,-1);
			dump_memory_object = false;
		}
#endif
		if (i % 6 == 0) {
			selectorManager.flush(nowTime);
			accessLogger.checkRotate(nowTime);
			errorLogger.checkRotate(nowTime);
			logManage.checkRotate(nowTime);
			m_thread.Flush(conf.min_free_thread);	
#ifdef ENABLE_DIGEST_AUTH
			KHttpDigestAuth::flushSession(kgl_current_sec);
#endif
		}
/////////[226]
		flush_mem_cache();
/////////[227]
		cdnContainer.flush(kgl_current_sec);
		if(vhd.isLoad() && !vhd.isSuccss()){
			klog(KLOG_NOTICE,"vh database last status is failed.try again.\n");
			std::string errMsg;
			if(!vhd.loadVirtualHost(conf.gvm,errMsg)){
				klog(KLOG_ERR,"load virtual host failed. %s\n",errMsg.c_str());
			}
		}
#ifdef ENABLE_DISK_CACHE
		scan_disk_cache();
#endif
	}
	KTHREAD_RETURN;
}

