/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kanglesoft.com/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KHTTPAUTH_H
#define KHTTPAUTH_H
#include <stdlib.h>
#include "global.h"
/*
 * 定义认证类型
 */
#define AUTH_BASIC  0
#define AUTH_DIGEST 1
#define AUTH_NTLM   2
#ifdef ENABLE_DIGEST_AUTH
#define TOTAL_AUTH_TYPE 2
#else
#define TOTAL_AUTH_TYPE 1
#endif
#include "KStream.h"
class KHttpRequest;
/*
 * http认证基类
 */
class KHttpAuth {
public:
	KHttpAuth(int type) {
		this->type = type;
		user = NULL;
		realm = NULL;
	}
	virtual ~KHttpAuth();
	static int parseType(const char *type);
	static const char *buildType(int type);
	int getType() {
		return type;
	}
	const char *getUser() {
		return user;
	}
	const char *getRealm() {
		return realm;
	}
	virtual void insertHeader(KWStream &s) = 0;
	virtual void insertHeader(KHttpRequest *rq) = 0;
	virtual bool parse(KHttpRequest *rq, const char *str) = 0;
	virtual bool verify(KHttpRequest *rq, const char *password,
			int passwordType) = 0;
	virtual bool verifySession(KHttpRequest *rq)
	{
		return true;
	}
	friend class KAuthMark;
protected:
	char *user;
	char *realm;

private:
	int type;
};
#endif
