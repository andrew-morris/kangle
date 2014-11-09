#include "KReplaceContentMark.h"
bool replaceContentMarkCallBack(void *param,KRegSubString *sub_string,int *ovector,KStringBuf *st)
{
	KReplaceContentMarkParam *rp = (KReplaceContentMarkParam *)param;
	return rp->mark->callBack(rp->rq,sub_string,ovector,st);
}
void replaceContentMarkEndCallBack(void *param)
{
	KReplaceContentMarkParam *rp = (KReplaceContentMarkParam *)param;
	rp->mark->release();
	delete rp;
}
