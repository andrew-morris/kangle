#include <assert.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "KDsoModule.h"
#include "utils.h"
#include "log.h"

KDsoModule::KDsoModule()
{
	handle = NULL;
}
KDsoModule::~KDsoModule()
{
	
}
bool KDsoModule::isloaded()
{
	return handle!=NULL;
}
bool KDsoModule::load(const char *file)
{
	assert(handle==NULL);
	if(handle!=NULL){
		return false;
	}
	std::string path;
	if(!isAbsolutePath(file)){
		path = conf.path + file;
	}else{
		path = file;
	}
/////////[310]
	handle = LoadLibrary(file);
	if (handle == NULL) {
		debug("cann't LoadLibrary %s %s\n", path.c_str(), getError());
		return false;
	}
/////////[311]
	return true;
}
void *KDsoModule::findFunction(const char *func)
{
	if(handle==NULL){
		return NULL;
	}
	return GetProcAddress(handle,func);
}
const char *KDsoModule::getError()
{
#ifndef _WIN32
	return dlerror();
#else
	return "";
#endif
}
bool KDsoModule::unload()
{
	if (handle == NULL) {
		return false;
	}
	FreeLibrary(handle);
	handle = NULL;
	return true;	
}
