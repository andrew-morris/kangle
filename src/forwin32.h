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
#ifndef for_win32_include_skdjfskdfkjsdfj
#define for_win32_include_skdjfskdfkjsdfj
/////////[217]
#define PID_LIKE(x)  (x>0)
#define PTHREAD_CREATE_SUCCESSED(x) (x==0)
#define FUNC_CALL
typedef void * FUNC_TYPE;
#define filecmp		strcmp
#define filencmp 	strncmp
#define LoadLibrary(x) dlopen(x,RTLD_NOW|RTLD_GLOBAL|RTLD_LOCAL)
#define GetProcAddress dlsym
#define FreeLibrary	dlclose
#define SetDllDirectory(x)
#define GetLastError()	errno
#define _stati64 stat
#define _stat64 stat
typedef struct iovec   WSABUF;
typedef struct iovec * LPWSABUF;
#define PATH_SPLIT_CHAR		'/'
/////////[218]
#ifndef WIN32
#include <sys/types.h>
#endif
	typedef char				int8;
	typedef unsigned char		uint8;
	typedef short				int16;
	typedef unsigned short		uint16;
	typedef int					int32;
	typedef unsigned int		uint32;
#ifdef WIN32
	typedef __int64				int64;
	typedef unsigned __int64	uint64;
#else
	typedef int64_t				int64;
	typedef u_int64_t			uint64;
#endif
#endif
