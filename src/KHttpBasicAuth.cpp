/*
 * KHttpBasicAuth.cpp
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#include <string.h>
#include "utils.h"
#include "KHttpBasicAuth.h"
#include "malloc_debug.h"
KHttpBasicAuth::KHttpBasicAuth() :
	KHttpAuth(AUTH_BASIC) {

}
KHttpBasicAuth::KHttpBasicAuth(const char *realm) :
	KHttpAuth(AUTH_BASIC) {
	this->realm = xstrdup(realm);
}

KHttpBasicAuth::~KHttpBasicAuth() {
	if (user) {
		xfree(user);
	}
	if (realm) {
		xfree(realm);
	}
}
void KHttpBasicAuth::insertHeader(KHttpRequest *rq)
{
	KStringBuf s;
	s << "Basic realm=\"" << realm << "\"";
	rq->responseHeader(kgl_expand_string(AUTH_RESPONSE_HEADER),s.getBuf(),s.getSize());
}
void KHttpBasicAuth::insertHeader(KWStream &s)
{
	s << AUTH_RESPONSE_HEADER << ": Basic realm=\"" << realm << "\"\r\n";
}
bool KHttpBasicAuth::parse(KHttpRequest *rq, const char *str) {
	int str_len = strlen(str);
	if (str == NULL || str_len < 2) {
		return false;
	}
	user = b64decode((const unsigned char *) str, &str_len);
	if (user == NULL) {
		return false;
	}
	password = strchr(user, ':');
	if (password == NULL) {
		//Ã»ÓÐÃÜÂë
		return false;
	}
	*password = '\0';
	password++;
	return true;
}
bool KHttpBasicAuth::verify(KHttpRequest *rq, const char *password,
		int passwordType) {
	if (password == NULL || this->password == NULL) {
		return false;
	}
	return checkPassword(this->password, password, passwordType);
}
