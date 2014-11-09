/*
 * KHttpProxyFetchObject.h
 *
 *  Created on: 2010-4-20
 *      Author: keengo
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

#ifndef KHTTPPROXYFETCHOBJECT_H_
#define KHTTPPROXYFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KAcserver.h"
#include "KSocket.h"
#include "KAsyncFetchObject.h"
#include "KHttpObjectParserHook.h"
class KHttpProxyFetchObject: public KAsyncFetchObject {
public:
	KHttpProxyFetchObject()
	{
	}
	virtual ~KHttpProxyFetchObject()
	{
	}
	KFetchObject *clone(KHttpRequest *rq)
	{
		if (brd) {
			return KFetchObject::clone(rq);
		}
		return new KHttpProxyFetchObject();
	}
	bool needTempFile()
	{
		return false;
	}
protected:
	void buildHead(KHttpRequest *rq);
	Parse_Result parseHead(KHttpRequest *rq,char *buf,int len);
	char *nextBody(KHttpRequest *rq,int &len)
	{
		if (parser.bodyLen>0) {
			len = parser.bodyLen;
			parser.bodyLen = 0;
			return parser.body;
		}
		if (hot) {
			len = hot - header;
			hot = NULL;
			return header;
		}
		return NULL;
	}
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len)
	{
		hot = data + len;
		return Parse_Continue;
	}
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		if (TEST(rq->ctx->obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
			if(rq->ctx->left_read<=0){
				expectDone();
				return false;
			}
		}
		return true;
	}
	void expectDone()
	{
		if (hook.keep_alive_time_out>=0) {
			lifeTime = hook.keep_alive_time_out;
		} else {
			lifeTime = 0;
		}
	}
	void readBodyEnd(KHttpRequest *rq)
	{
		expectDone();
	}
	KHttpProtocolParser parser;
	KHttpObjectParserHook hook;
private:
	/////////[379]
};

#endif /* KHTTPPROXYFETCHOBJECT_H_ */
