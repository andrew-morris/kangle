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
#ifndef KGZIP_H_
#define KGZIP_H_
#include "zlib.h"
#include "KSocket.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
class KHttpTransfer;
enum {
	GZIP_DECOMPRESS_ERROR, GZIP_DECOMPRESS_CONTINUE, GZIP_DECOMPRESS_END
};
class KGzipDecompress : public KWUpStream
{
public:
	KGzipDecompress(KWStream *st,bool autoDelete);
	~KGzipDecompress();
	StreamState write_all(const char *str,int len);
	StreamState write_end();
	void setFast(bool fast)
	{
		this->fast = fast;
	}
private:
	StreamState decompress(int flush_flag);
	bool isSuccess;
	z_stream strm;
	char *out;
	unsigned used;
	bool fast;
	int in_skip ;
};
class KGzipCompress : public KWUpStream
{
public:
	KGzipCompress(KWStream *st,bool autoDelete);
	~KGzipCompress();
	StreamState write_all(const char *str,int len);
	StreamState write_end();
	void setFast(bool fast)
	{
		this->fast = fast;
	}
	StreamState flush()
	{
		return compress(Z_SYNC_FLUSH);
	}
private:
	StreamState compress(int flush_flag);
	z_stream strm;
	char *out;
	unsigned used;
	uLong crc;
	bool fast;
	bool isSuccess;
};
/*
老的压缩类，新的请使用KGzipCompress，使用流操作.
* /
class KGzip {
public:
	KGzip();
	virtual ~KGzip();
	unsigned getLen() {
		return totalLen;
	}
	bool initCompress();
	bool initDecompress();

	int decompress(const char *str, int len, KHttpRequest *rq, KBuffer *buffer);
	bool
	compress(const char *str, int len, KWStream *st);
	//compress data end
	bool compressEnd(KWStream *st, bool fast = true);
private:
	bool compress(KWStream *st, bool flush);
	z_stream strm;
	char *out;
	unsigned used;
	uLong crc;
	int model;
	unsigned totalLen;
};
*/

#endif /* KGZIP_H_ */
