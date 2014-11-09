#ifndef KCONTEXT_H
#define KCONTEXT_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
class KHttpTransfer;
class KHttpObject;
class KRequestQueue;
class KHttpRequest;
enum modified_type
{
	modified_if_modified,
	modified_if_range,
	modified_if_unmodified
};
class KContext
{
public:
	inline KContext()
	{
		memset(this,0,sizeof(KContext));
	}
	~KContext()
	{
		assert(obj==NULL && old_obj==NULL);
		assert(st == NULL);
	}
	void pushObj(KHttpObject *obj);
	void popObj();
	KHttpObject *obj;
	KHttpObject *old_obj;
	//bool last_status;
	bool haveStored;
	bool new_object;
	//lastModified类型
	modified_type mt;
	time_t lastModified;
	INT64 content_range_length;
	int keepAlive;
	//异步读文件时需要的数据
	INT64 left_read;
	KWStream *st;
	void clean();
	void store_obj(KHttpRequest *rq);
	void clean_obj(KHttpRequest *rq);
};
class KSendContext
{
public:
	KSendContext()
	{
		hot_buffer = NULL;
		header = NULL;
		body = NULL;
		send_size = 0;
		header_size = 0;
	}
	~KSendContext()
	{
		kassert(header==NULL);
	}
	void clean();
	inline char *init_hot()
	{
		if (header) {
			hot_buffer = header;
			return hot_buffer->data;
		}else{
			hot_buffer = body;
			return hot_buffer->data + body_start;
		}
	}
	inline bool getSendBuffer(LPWSABUF buffer,int &bufferCount,char *hot)
	{
		int i;
#ifdef _WIN32
		buffer[0].buf = hot;
		buffer[0].len = getSendSize(hot);
#elif HAVE_WRITEV
		buffer[0].iov_base = hot; 
       	buffer[0].iov_len = getSendSize(hot);
#else
		assert(false);
		return false;
#endif
		INT64 total_body_len = body_len;
		if(total_body_len>=0 && header==NULL){
			//此为body
#ifdef _WIN32
			total_body_len -= buffer[0].len;
#elif HAVE_WRITEV
			total_body_len -= buffer[0].iov_len;
#endif	
		}
		buff *tmp = hot_buffer;
		bool data_is_body = false;
		for(i=1;i<bufferCount;i++){
			tmp = tmp->next;
			if(tmp==NULL){
				if(data_is_body || header==NULL){
					//body结束
					break;
				}
				tmp = body;
				data_is_body = true;
				if(tmp==NULL){
					//body结束
					break;
				}
			}
			int size;
			char *data;
			if (tmp==body) {
				//第一块body
				size = tmp->used - (int)body_start;
				kassert(size>=0);
				data = tmp->data + body_start;
			} else {
				size = tmp->used;
				data = tmp->data;
			}
			//if(total_body_len>=0){
			size = (int)MIN((INT64)size,(INT64)total_body_len);
			total_body_len-=size;
			//}
			if(size<=0){
				break;
			}
#ifdef _WIN32
			buffer[i].buf = data;
			buffer[i].len = size;
#elif HAVE_WRITEV
			buffer[i].iov_base = data;
			buffer[i].iov_len = size;
#endif
			
		}
		bufferCount = i;
		return true;
	}
	inline INT64 getSendedBodyLen()
	{
		INT64 length = send_size - header_size;
		if (length<0) {
			return 0;
		}
		return length;		
	}
	inline int getSendSize(char *hot)
	{
		assert(hot_buffer && hot_buffer->data);
		unsigned used = (unsigned)(hot - hot_buffer->data);
		assert(used < hot_buffer->used);
		int size = hot_buffer->used - used;
		if(header==NULL && body_len>0){
			return MIN(size,(int)body_len);
		}
		return size;
	}
	inline char *step_next(char *hot,int got)
	{		
		send_size+=got;
		if (header==NULL) {
			//body处理
			if (body_len > 0) {
				body_len -= got;
				assert(body_len>=0);
				if (body_len==0) {
					body = NULL;
					return NULL;
				}
			}
		}
		hot += got;
		unsigned used = (unsigned)(hot - hot_buffer->data);
		if(used >= hot_buffer->used){
			hot_buffer = hot_buffer->next;
			if(hot_buffer == NULL || hot_buffer->used == 0 ){
				if(header){
					//如果现在是发送http头。则要切换
					//delete ctx.sndHeader;
					KBuffer::destroy(header);
					header = NULL;					
					if(body && body->used > 0){
						hot_buffer = body;
						hot = hot_buffer->data + body_start;						
						body_start = 0;
						return hot;
					}
				}
				body = NULL;
				body_len = 0;
				//已经发完了
				return NULL;
			}
			return hot_buffer->data;
		}
		return hot;
	}
	inline char *next(char *hot,int  got)
	{
		while(got>0){
			int size = getSendSize(hot);
			size = MIN(got,size);
			hot = step_next(hot,size);
			if (hot==NULL) {
				break;
			}
			got-=size;
		}
		return hot;
	}
	buff *header;
	buff *body;
	buff *hot_buffer;
	INT64 body_start;
	INT64 body_len;
	INT64 send_size;
	int header_size;
};
#endif
