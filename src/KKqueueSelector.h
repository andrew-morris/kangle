#ifndef KKQUEUESELECTOR_H_
#define KKQUEUESELECTOR_H_

#include "global.h"

#ifdef BSD_OS
#include <sys/event.h>
#include "KSelector.h"
#include "malloc_debug.h"

class KKqueueSelector : public KSelector
{
public:
	const char *getName()
	{
		return "kqueue";
	}
	KKqueueSelector();
	virtual ~KKqueueSelector();
	void select();
	bool addListenSocket(KSelectable *st);
protected:
	bool addSocket(KSelectable *rq,int op);
	void removeSocket(KSelectable *rq);
private:
	int kdpfd;

};
#endif
#endif /*KEPOLLSELECTOR_H_*/
