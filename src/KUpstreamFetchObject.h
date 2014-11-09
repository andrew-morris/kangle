#ifndef KUPSTREAMFETCHOBJECT_H
#define KUPSTREAMFETCHOBJECT_H
#include "KFetchObject.h"
class KUpstreamFetchObject : public KFetchObject
{
public:
	virtual KClientSocket *getSocket() = 0;
#ifdef _WIN32
	virtual KSelectable *getBindData()
	{
		return NULL;
	}
#endif
};
#endif
