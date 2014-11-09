/*
 * KDeChunked.cpp
 *
 *  Created on: 2010-5-4
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

#include <vector>
#include <stdlib.h>
#include "http.h"
#include "KDeChunked.h"
#include "KHttpTransfer.h"
#include "malloc_debug.h"
#define		CHUNKED_DF_ERROR		-2
#define		CHUNKED_ERROR			-1
#define		CHUNKED_CONTINUE		0
#define		CHUNKED_END				1
KDeChunked::KDeChunked(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	chunk_size = 0;
	work_len = 0;
	work = NULL;

}

KDeChunked::~KDeChunked() {
	if (work) {
		xfree(work);
	}
}
/*
 dechunk函数:
 work_len	=		-1			表示已读完body,要读body的结束\n
 work_len	=		-2			表示读body状态
 work_len	>=	    0			表示读chunk_size
 work_len	=		-3			表示读最后 body end
 */
StreamState KDeChunked::write_all(const char *buf, int buf_len) {
	char *next_line;
	bool head_buffer = false;
	char *data;
	int data_len;
	restart:
	//读body结束部分\n
	if (work_len == -1 || work_len == -3) {
		next_line = (char *) memchr(buf, '\n', buf_len);
		if (next_line == NULL) {
			return STREAM_WRITE_SUCCESS;//continue;
		}
		int skip_length = next_line - buf + 1;
		buf += skip_length;
		buf_len -= skip_length;
		//	chunk_size = 0;
		//正确结束
		if (work_len == -3) {
			//assert(buf_len==0);
			//skip double 0\r\n0\r\n this will cause buf_len>0
			assert(work==NULL);
			//	printf("success dechunk buf\n");
			return STREAM_WRITE_END;
		}
		work_len = 0;
		if (buf_len == 0) {
			//	printf("刚好够数据,继续\n");
			return STREAM_WRITE_SUCCESS;
		}
	}
	//表示读chunk_size
	if (work_len >= 0) {
		if (work) {
			int read_len = MIN(20-work_len,buf_len);
			//assert(read_len>0);
			if (read_len <= 0) {
				//	printf("chunk size 太长了,20位都不够,出错!");
				xfree(work);
				work = NULL;
				return STREAM_WRITE_FAILED;
			}
			memcpy(work + work_len, buf, read_len);
			work_len += read_len;
			data = work;
			data_len = work_len;
		} else {
			data = (char *) buf;
			data_len = buf_len;
		}
		next_line = (char *) memchr(data, '\n', data_len);
		if (next_line == NULL) {//不够哦
			head_buffer = false;
			if (work) {
				//continue;
				return STREAM_WRITE_SUCCESS;
			}
			work = (char *) xmalloc(buf_len+20);
			work_len = buf_len;
			memcpy(work, buf, buf_len);
			//continue;
			return STREAM_WRITE_SUCCESS;
		}
		work_len = -2;
		chunk_size = strtol(data, NULL, 16);

		//	printf("chunk_size=%d\n",chunk_size);
		if (chunk_size == 0 && data[0] != '0') {
			//	printf("read chunk size failed %d,data[0]=%c\n", chunk_size,
			//			data[0]);
			//assert(false);
			if (work) {
				free(work);
				work = NULL;
			}
			return STREAM_WRITE_FAILED;
		}
		if (work) {
			free(work);
			work = NULL;
		}
		if (chunk_size < 0 || chunk_size > 100000000) {
			//assert(false);
			//printf("chunk size 不正确,%d\n",chunk_size);
			return STREAM_WRITE_FAILED;
		}
		//结束
		next_line = (char *) memchr(buf, '\n', buf_len);
		assert(next_line);
		int skip_length = next_line - buf + 1;
		buf += skip_length;
		buf_len -= skip_length;
		if (chunk_size == 0) {
			work_len = -3;
			goto restart;
		}
	}
	//读body
	if (work_len == -2) {
		int read_len = MIN(chunk_size,buf_len);
		if(read_len>0){
			/*if (to && !to->write_all(buf, read_len)) {
				return CHUNKED_DF_ERROR;
			}
			if (df && !df->trySendBody(buf, read_len)) {
				return CHUNKED_DF_ERROR;
			}
			*/
			if(!KHttpStream::write_all(buf,read_len)){
				return STREAM_WRITE_FAILED;
			}
			buf_len -= read_len;
			buf += read_len;
			chunk_size -= read_len;
		}
		if (buf_len > 0) {
			assert(chunk_size==0);
			work_len = -1;
			//	chunk_size = -1;
			//	printf("牛!还有chunk可读.\n");
			goto restart;
		}
		if (chunk_size == 0) {
			//	chunk_size = -1;
			work_len = -1;
		}
		//printf("chunk 数据不够,等下一个buf\n");
		return STREAM_WRITE_SUCCESS;
	}
	//printf("不可能到这里来哦!有bug\n");
	assert(false);
	return STREAM_WRITE_FAILED;
}
dechunk_status  KDeChunked::dechunk(const char **buf,int &buf_len,const char **piece,int &piece_length)
{
	char *next_line;
	bool head_buffer = false;
	char *data;
	int data_len;
	*piece = NULL;
restart:
	//读body结束部分\n
	if (work_len == -1 || work_len == -3) {
		next_line = (char *) memchr(*buf, '\n', buf_len);
		if (next_line == NULL) {
			return dechunk_continue;//continue;
		}
		int skip_length = next_line - (*buf) + 1;
		(*buf) += skip_length;
		buf_len -= skip_length;
		//	chunk_size = 0;
		//正确结束
		if (work_len == -3) {
			//assert(buf_len==0);
			//skip double 0\r\n0\r\n this will cause buf_len>0
			assert(work==NULL);
			//	printf("success dechunk buf\n");
			return dechunk_end;
		}
		work_len = 0;
		if (buf_len == 0) {
			//	printf("刚好够数据,继续\n");
			return dechunk_continue;
		}
	}
	//表示读chunk_size
	if (work_len >= 0) {
		if (work) {
			int read_len = MIN(20-work_len,buf_len);
			//assert(read_len>0);
			if (read_len <= 0) {
				//	printf("chunk size 太长了,20位都不够,出错!");
				xfree(work);
				work = NULL;
				return dechunk_failed;
			}
			memcpy(work + work_len, buf, read_len);
			work_len += read_len;
			data = work;
			data_len = work_len;
		} else {
			data = (char *)(*buf);
			data_len = buf_len;
		}
		next_line = (char *) memchr(data, '\n', data_len);
		if (next_line == NULL) {//不够哦
			head_buffer = false;
			if (work) {
				//continue;
				return dechunk_continue;
			}
			work = (char *) xmalloc(buf_len+20);
			work_len = buf_len;
			memcpy(work, *buf, buf_len);
			//continue;
			return dechunk_continue;
		}
		work_len = -2;
		chunk_size = strtol(data, NULL, 16);

		//	printf("chunk_size=%d\n",chunk_size);
		if (chunk_size == 0 && data[0] != '0') {
			//	printf("read chunk size failed %d,data[0]=%c\n", chunk_size,
			//			data[0]);
			//assert(false);
			if (work) {
				free(work);
				work = NULL;
			}
			return dechunk_failed;
		}
		if (work) {
			free(work);
			work = NULL;
		}
		if (chunk_size < 0 || chunk_size > 100000000) {
			//assert(false);
			//printf("chunk size 不正确,%d\n",chunk_size);
			return dechunk_failed;
		}
		//结束
		next_line = (char *) memchr(*buf, '\n', buf_len);
		assert(next_line);
		int skip_length = next_line - (*buf) + 1;
		(*buf) += skip_length;
		buf_len -= skip_length;
		if (chunk_size == 0) {
			work_len = -3;
			goto restart;
		}
	}
	//读body
	if (work_len == -2) {
		int read_len = MIN(chunk_size,buf_len);
		if (read_len>0) {
			*piece = *buf;
			piece_length = read_len;
			buf_len -= read_len;
			(*buf) += read_len;
			chunk_size -= read_len;
		}
		if (buf_len > 0) {
			assert(chunk_size==0);
			work_len = -1;
			//	chunk_size = -1;
			//	printf("牛!还有chunk可读.\n");
			//goto restart;
			return dechunk_success;
		}
		if (chunk_size == 0) {
			//	chunk_size = -1;
			work_len = -1;
		}
		//printf("chunk 数据不够,等下一个buf\n");
		return dechunk_continue;
	}
	//printf("不可能到这里来哦!有bug\n");
	assert(false);
	return dechunk_failed;
}
