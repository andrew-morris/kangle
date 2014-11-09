/*
 * sapi.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef SAPI_H_
#define SAPI_H_
#ifdef __cplusplus
extern "C" {
#endif
#if defined _WIN32 || defined __CYGWIN__
      #define DLL_PUBLIC __declspec(dllexport)
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #define DLL_PUBLIC
  #endif
#endif
#ifdef _WIN32
#include <windows.h>
#else
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NO_DATA             232 
#define ERROR_INVALID_INDEX       1413
#define WINAPI  
#define APIENTRY
#define MAX_PATH	260
#define IN
#define MAKELONG(x,y) ((x<<16)|y)
#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif
#define DLL_PROCESS_ATTACH	1
#define DLL_THREAD_ATTACH	2
#define DLL_THREAD_DETACH	3
#define DLL_PROCESS_DETACH	0
typedef unsigned int        DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef void                VOID;
typedef VOID *              LPVOID;
typedef VOID *              PVOID;
typedef char                CHAR;
typedef CHAR *              LPSTR;
typedef BYTE *              LPBYTE;
typedef DWORD *             LPDWORD;
typedef wchar_t             WCHAR;

typedef void *              HANDLE;
typedef const char *        LPCSTR;
typedef unsigned short      USHORT;
typedef wchar_t *           LPWSTR;
typedef unsigned long long  ULONGLONG;
typedef int                 HRESULT;
typedef void *              HMODULE;
typedef void *              HINSTANCE;
void SetLastError(DWORD errorCode);
#endif
#ifdef _WIN32
#define PIPE_T	HANDLE
#define ClosePipe	CloseHandle
#define INVALIDE_PIPE	INVALID_HANDLE_VALUE
#define KTHREAD_FUNCTION  void
#define KTHREAD_RETURN    return
#else
#define PIPE_T	int
#define ClosePipe	::close
#define INVALIDE_PIPE	-1
typedef void * KTHREAD_FUNCTION;
#define KTHREAD_RETURN do { return NULL; }while(0)
#endif
#define   KGL_REQ_RESERV_COMMAND                    100000
#define   KGL_REQ_COMMAND                          (KGL_REQ_RESERV_COMMAND+1)
#define   KGL_REQ_THREAD                           (KGL_REQ_RESERV_COMMAND+2)
#define   KGL_REQ_CREATE_WORKER                    (KGL_REQ_RESERV_COMMAND+3)
#define   KGL_REQ_RELEASE_WORKER                   (KGL_REQ_RESERV_COMMAND+4)
#define   KGL_REQ_SERVER_VAR                       (KGL_REQ_RESERV_COMMAND+5)
struct kgl_command_env_t
{
	char *name;
	char *val;
	struct kgl_command_env_t *next;
};
struct kgl_process_std_t
{	
	PIPE_T hstdin;
	PIPE_T hstdout;
	PIPE_T hstderr;
	const char *stdin_file;
	const char *stdout_file;
	const char *stderr_file;
};
struct kgl_command_t
{
	const char *vh;
	const char *cmd;
	const char *dir;
	struct kgl_command_env_t *env;
	kgl_process_std_t std;
};
struct kgl_thread_t
{
	KTHREAD_FUNCTION (* thread_function)(void *param);
	void *param;
	void *worker;
};
#ifdef __cplusplus
 }
#endif
#endif /* SAPI_H_ */
