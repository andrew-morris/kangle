#ifndef KASYNCSELECTABLE_H
#define KASYNCSELECTABLE_H
#include "KSelectable.h"
#include "KFile.h"
class KAsyncSelectable : public KSelectable
{
public:
	KAsyncSelectable(KFile *fp)
	{
		this->fp = fp;
	}
	KSocket *getSocket()
	{
		return NULL;
	}
	void read(void *arg,resultEvent result,bufferEvent buffer);
private:
	KFile *fp;
};
#endif
