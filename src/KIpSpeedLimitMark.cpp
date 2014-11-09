#include "KIpSpeedLimitMark.h"
void ip_speed_limit_clean(KHttpRequest *rq,void *data)
{
	KIpSpeedLimitContext *ctx = (KIpSpeedLimitContext *)data;
	ctx->mark->requestClean(rq,ctx->ip);
	free(ctx->ip);
	ctx->mark->release();
	delete ctx;
}