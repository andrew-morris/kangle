/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kanglesoft.com/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef KCONFIG_H
#define KCONFIG_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef _WIN32
#include <unistd.h>
#endif
#include <vector>
#include <list>
#include <string>
#include <map>
#include "KSocket.h"
#include "global.h"
#include "KMutex.h"
#include "KTimeMatch.h"
#include "malloc_debug.h"
#include "KUserManageInterface.h"
#include "server.h"
#include "KAsyncWorker.h"
#ifdef KSOCKET_SSL
#include "KSSLSocket.h"
#endif
#define AUTOUPDATE_OFF    0
#define AUTOUPDATE_ON   1
#define AUTOUPDATE_DOWN   2

#define REFRESH_AUTO	0
#define	REFRESH_ANY		1	
#define REFRESH_NEVER	2

#define USE_PROXY	1
#define USE_LOG		(1<<1)
#define IGNORE_CASE	(1<<2)
#define NO_FILTER	(1<<2)
#define SAFE_STRCPY(s,s1)   do {memset(s,0,sizeof(s));strncpy(s,s1,sizeof(s)-1);}while(0)
class KAcserverManager;
class KVirtualHostManage;
class KVirtualHost;
struct KPerIpConnect;
class KListenHost {
public:
	KListenHost()
	{
		ext = cur_config_ext;
		event_driven = false;
	}
	std::string name;
	std::string ip;
	int port;
	int model;
	bool ext;
	bool event_driven;
#ifdef KSOCKET_SSL
	bool sni;
	std::string certificate;
	std::string certificate_key;
#endif
};
struct WorkerProcess
{
#ifdef _WIN32
#ifdef ENABLE_DETECT_WORKER_LOCK
	HANDLE active_event;
#endif
	HANDLE shutdown_event;
	HANDLE notice_event;
	HANDLE hProcess;
	int pid;
	int pending_count;
#else
	pid_t pid;
#endif
	bool closed;
	int worker_index;
};
class KConfigBase 
{
public:
	KConfigBase()
	{
		memset(this,0,sizeof(KConfigBase));
	}
	unsigned int worker;
	unsigned int max; //最大线程
	unsigned int max_per_ip; //每个IP最大连接数
	unsigned int per_ip_deny;
	unsigned min_free_thread;
	unsigned time_out;
	unsigned keep_alive;
	char hostname[32];
	int log_level;
	INT64 log_rotate_size;
	INT64 error_rotate_size;
	//默认是否缓存,1=是,其它=不
	int default_cache;
	unsigned max_cache_size;
	INT64 mem_cache;
#ifdef ENABLE_DISK_CACHE
	INT64 disk_cache;
#ifdef ENABLE_BIG_OBJECT
	INT64 max_bigobj_size;
#endif
#endif
	int select_count;
	int refresh;
	int refresh_time;
	
	//只压缩可以cache的物件
	int only_gzip_cache;
	//最小压缩长度
	unsigned min_gzip_length;
	//压缩级别(1-9)
	int gzip_level;
	//bool x_forwarded_for;
	//bool x_real_ip;
	//bool insert_via;
	//bool x_cache;
	bool error_ip;
	bool path_info;
	//是否异步io
	bool async_io;
	int worker_io;
	int worker_dns;
	int auth_type;
	int passwd_crypt;
	//int configVersion;
	int anti_fatboy;
	//白名单时间
	int wl_time;
#ifdef ENABLE_ADPP
	int process_cpu_usage;
#endif
#ifdef MALLOCDEBUG
	bool mallocdebug;
#endif
	unsigned maxLogHandle;
	unsigned logs_day;
	bool log_sub_request;
	bool log_handle;
	INT64 logs_size;
	int autoupdate;
	int autoupdate_time;
	unsigned buffer;
#ifdef ENABLE_TF_EXCHANGE
	int tmpfile;
	INT64 max_post_size;
#endif
#ifdef KSOCKET_UNIX	
	bool unix_socket;
#endif
	int errorTryCount;
	bool removeAcceptEncoding;
#ifdef ENABLE_VH_FLOW
	//自动刷新流量时间(秒)
	int flush_flow_time;
#endif
	char lang[8];
	char disk_cache_dir2[512];
	/////////[314]
	char log_rotate[32];
	char access_log[128];
	char logHandle[512];
	char cookie_stick_name[16];
	char server_software[32];
	char disk_work_time[32];
	/////////[315]
};
class KConfig : public KConfigBase
{
public:
	KConfig()
	{
		memset(disk_cache_dir,0,sizeof(disk_cache_dir));
		per_ip_head = NULL;
		per_ip_last = NULL;
	}
	~KConfig();
	KAcserverManager *am;
	KVirtualHostManage *vm;
	std::string admin_passwd;
	std::string admin_user;
	std::vector<std::string> admin_ips;
	std::vector<KListenHost *> service;	
	//run_user,run_group为一次性使用，可以安全用string
	std::string run_user;
	std::string run_group;
	//disk_cache_dir2是可以修改的。保存设置用的。
	//实际用的时候用disk_cahce_dir.
	char disk_cache_dir[512];
	KPerIpConnect *per_ip_head;
	KPerIpConnect *per_ip_last;
	/////////[316]
	void copy(KConfig *c);
	void set_time_out(unsigned val)
	{
		time_out = val;
		if(time_out>0 && time_out < 5){
			//最小超时为5秒
			time_out = 5;
		}
	}
	void setErrorTryCount(int errorTryCount)
	{
		if(errorTryCount<0) {
			errorTryCount = 0;
		} else if(errorTryCount>5) {
			errorTryCount = 5;
		}
		this->errorTryCount = errorTryCount;
	}
	void setHostname(const char *hostname) {
		SAFE_STRCPY(this->hostname,hostname);
	}
};
//配置参数和程序运行信息
class KGlobalConfig : public KConfig {
public:
	KGlobalConfig();
	//////////////////////////////////////////////////////
	KMutex admin_lock;
	/////////////////////////////////////////////////////////
	//以下是配置的编译结果，每次配置文件更改，都要重新生成
	KTimeMatch diskWorkTime;
	KTimeMatch autoupdate_install_time;
	void set_autoupdate_time(int autoupdate_time);
	/////////////////////////////////////////////////////////
	//以下是程序运行信息,只在程序启动前初始化一次...
	std::list<std::string> mergeFiles;
	std::string path;
	std::string tmppath;
	std::string program;
	std::string extworker;
	KAsyncWorker *ioWorker;
	KAsyncWorker *dnsWorker;
	int serverNameLength;
	char serverName[32];
#ifdef _WIN32
		//kangle程序所在的盘符
	std::string diskName;
#endif
	/////////[317]
	KAcserverManager *gam;
	KVirtualHostManage *gvm;
	//3311的内置虚拟主机
	KVirtualHost *sysHost;
};
extern KGlobalConfig conf;
extern int m_debug;
extern bool need_reboot_flag;
extern KConfig *cconf;
int merge_apache_config(const char *file);
void LoadDefaultConfig();
void do_config(bool firstTime=true);
//读取配置文件，可重入
void load_config(KConfig *cconf,bool firstTime);
//解析配置文件,调用完load_config之后，对一些项目，解析处理，实施
void parse_config(bool firstTime);
//清除配置文件，用于内存泄漏检测时，才调用
void clean_config();

bool saveConfig();
INT64 get_size(const char *size);
std::string get_size(INT64 size);
#define CONFIG_FILE_SIGN  "<!--configfileisok-->"
#ifndef CONFIG_FILE
#ifndef _WIN32
#define CONFIG_FILE "/etc/config.xml"
#else
#define CONFIG_FILE "\\etc\\config.xml"
#endif
#endif
#endif
