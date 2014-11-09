#include "KSelectable.h"
#include "KSelector.h"
KSelectable::~KSelectable()
{
#ifndef NDEBUG
	klog(KLOG_DEBUG,"delete st=%p\n",this);
#endif
	if (secondHandler) {
		delete secondHandler;
	}
}
