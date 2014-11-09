/*
 * KLineFile.cpp
 *
 *  Created on: 2010-5-30
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

#include <sys/stat.h>
#include <stdio.h>
#include "KLineFile.h"
#include "forwin32.h"
#include "KFile.h"
#include "malloc_debug.h"
#define MAX_OPEN_FILE_SIZE		4*1024*1024
KLineFile::KLineFile() {
	hot = buf = NULL;
}

KLineFile::~KLineFile() {
	if (buf) {
		xfree(buf);
	}
}
void KLineFile::give(char *str)
{
	if(buf){
		xfree(buf);
	}
	buf = str;
	hot = buf;
}

void KLineFile::init(const char *str)
{
	if(buf){
		xfree(buf);
	}
	buf = xstrdup(str);
	hot = buf;
}
OpenState KLineFile::open(const char *file, time_t &lastModified) {
	struct _stati64 sbuf;
	if (lstat(file, &sbuf) != 0 || !S_ISREG(sbuf.st_mode)) {
		return OPEN_FAILED;
	}
	if (sbuf.st_mtime == lastModified) {
		return OPEN_NOT_MODIFIED;
	}
	int len = (int)MIN(sbuf.st_size,MAX_OPEN_FILE_SIZE);
	if (buf) {
		free(buf);
	}
	buf = (char *) malloc(len + 1);
	KFile fp;
	if (fp.open(file,fileRead)) {
		len = fp.read(buf,len);
		if (len >= 0) {
			buf[len] = '\0';
			hot = buf;
			lastModified = sbuf.st_mtime;
			return OPEN_SUCCESS;
		}
	}
	return OPEN_FAILED;
}
char *KLineFile::readLine() {
	if (hot == NULL) {
		return NULL;
	}
	while (*hot && IS_SPACE(*hot))
		hot++;
	if (*hot == '\0') {
		return NULL;
	}
	char *p = hot;
	char *end = strchr(hot, '\n');
	if (end == NULL) {
		if (strlen(hot) < 1) {
			return NULL;
		}
		end = hot + strlen(hot) - 1;
		hot = NULL;
	}
	hot = end + 1;
	while (end != p && IS_SPACE(*end)) {
		*end = '\0';
		end--;
	}
	return p;
}
