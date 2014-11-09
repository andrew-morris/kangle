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
#ifndef KSERVER_H_
#define KSERVER_H_
#include "KSelectable.h"
#include "KSSLSocket.h"
#include "KVirtualHostContainer.h"
#include "KIpVirtualHost.h"
#include "KCountable.h"
class KHttpRequest;
//class KNsVirtualHost;
class KVirtualHost;
class KServer : public KSelectable,public KAtomCountable {
public:
	KServer();
	inline SOCKET getSockfd()
	{
		return server.get_socket();
	}
	bool isOpened()
	{
		return getSockfd() != INVALID_SOCKET;
	}
	void close();
	KServerSocket server;
	char ip[MAXIPLEN];
	u_short model;
	u_short port;
	std::string name;
	//是否是动态的，即可删除的
	bool dynamic;
	//是否已经开始
	bool started;
	bool event_driven;
#ifdef KSOCKET_SSL
	bool sslParsed;
	bool sni;
	std::string certificate;
	std::string certificate_key;
	SSL_CTX *ssl_ctx;
	bool load_ssl();
#endif
/////////[257]
public:
	void clear();
	inline bool isClosed()
	{
		return closed;
	}
	void setClosed()
	{
		closed = true;
	}
	bool isEmpty();
	KVirtualHostContainer *vhc;
	void addVirtualHost(KVirtualHost *vh);
	void removeVirtualHost(KVirtualHost *vh);
	void bindVirtualHost(KVirtualHost *vh);
	void unbindAllVirtualHost();
	static void addDefaultVirtualHost(KVirtualHost *vh);
	static void removeDefaultVirtualHost(KVirtualHost *vh);
	static query_vh_result parseVirtualHost(KHttpRequest *rq,const char *site);
	static KVirtualHostContainer defaultVhc;
private:
	virtual ~KServer();
	volatile bool closed;
/////////[258]
};
#endif /* KSERVER_H_ */
