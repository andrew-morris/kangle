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
#ifndef cache_h_saldkfjsaldkfjalsfdjasdf987a9sd7f9adsf7
#define cache_h_saldkfjsaldkfjalsfdjasdf987a9sd7f9adsf7

#include <string>
#include <assert.h>
#include "global.h"
#include "KMutex.h"
#include "malloc_debug.h"
#include "KHttpObject.h"
#include "KVirtualHost.h"
#include "KObjectList.h"
#include "KHttpObjectHash.h"
#include "do_config.h"
#include "KCache.h"
//16  32   64   128  256  512   1024  2048  4096
//0xF 0x1F 0x3F 0x7F 0xFF 0x1FF 0x3FF 0x7FF 0xFFF
#define CACHE_DIR_MASK1    0x7F
#define CACHE_DIR_MASK2    0x7F
//@deprecated use KCache

bool saveCacheIndex(bool fast);
bool loadCacheIndex();
FUNC_TYPE FUNC_CALL load_cache_index(void *param);

void init_cache();
int get_count_in_hash();
void dead_obj(KHttpObject *obj);
void release_obj(KHttpObject *);
void dead_all_obj();
std::string get_disk_cache_file(KHttpObject *obj);
std::string get_cache_index_file();
void clean_disk(int m_size);
void get_cache_size(INT64 &total_mem_size,INT64 &total_disk_size);
void caculateCacheSize(INT64 &csize,INT64 &cdsize,INT64 &hsize,INT64 &hdsize);

//内容过滤链更新时
//void change_content_filter(int flag = GLOBAL_KEY_CHECKED, KVirtualHost *vh =
//		NULL);
inline bool objCanCache(KHttpRequest *rq,KHttpObject *obj)
{
	if (TEST(rq->flags,RQ_HAS_AUTHORIZATION)) {
		//如果是有认证用户的我们不缓存。
		SET(obj->index.flags,ANSW_NO_CACHE);
	}
	if (TEST(obj->index.flags,FLAG_DEAD|ANSW_NO_CACHE)) {
		//死物件和标记为不缓存的
		return false;
	}
	if (conf.default_cache == 0 && !TEST(obj->index.flags,FLAG_NEED_CACHE)) {
		//默认不缓存并且也没有说明要缓存的
		return false;
	}
	return true;
}
inline KHttpObject * findHttpObject(KHttpRequest *rq, int flags, bool *new_object) {
	*new_object = false;
	u_short url_hash = cache.hash_url(rq->url);
	KHttpObject *obj = cache.find(rq,url_hash);
	if (obj == NULL) {
		if (TEST(flags,AND_PUT)) {
			obj = new KHttpObject(rq);
			//cache the url_hash
			obj->h = url_hash;
			*new_object = true;
		}
	}
	return obj;
}
inline void release_obj(KHttpObject *obj) {
	obj->release();
}
int clean_cache(KReg *reg,int flag);
inline int clean_cache(const char *str,bool wide)
{
	KUrl url;
	if (!parse_url(str,&url)) {
		url.destroy();
		return 0;
	}
	int count = cache.clean_cache(&url,wide);
	url.destroy();
	return count;
}
inline int get_cache_info(const char *str,bool wide,KCacheInfo *ci)
{
	KUrl url;
	if (!parse_url(str,&url)) {
		url.destroy();
		return 0;
	}
	int count = cache.get_cache_info(&url,wide,ci);
	url.destroy();
	return count;
}
#endif
