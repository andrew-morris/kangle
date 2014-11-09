/*
 * test.cpp
 *
 *  Created on: 2010-5-31
 *      Author: keengo
 *
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
#include <assert.h>
#include <string.h>
#include "global.h"
#include "lib.h"
#include "KGzip.h"
#include "do_config.h"
#include "malloc_debug.h"
#include "KLineFile.h"
#include "KHttpFieldValue.h"
#include "KHttpField.h"
#include "KPipeStream.h"
#include "KFileName.h"
#include "KVirtualHost.h"
#include "KChunked.h"
#include "KExpressionParseTree.h"
#include "KHtAccess.h"
#include "KTimeMatch.h"
#include "KReg.h"
#include "KFile.h"
#include "KXml.h"
#include "cache.h"
#include "KConnectionSelectable.h"
/////////[87]
#ifdef ENABLE_INPUT_FILTER
#include "KMultiPartInputFilter.h"
#endif
using namespace std;
char *getString(char *str, char **substr);
void test_file()
{
	const char *test_file = "c:\\windows\\temp\\test.txt";
	KFile file;
	assert(file.open(test_file,fileWrite));
	assert(file.write("test",4));
	file.close();
	assert(file.open(test_file,fileRead));
	char buf[8];
	int len = file.read(buf,8);
	assert(len==4);
	assert(strncmp(buf,"test",4)==0);
	file.seek(1,seekBegin);
	len = file.read(buf,8);
	assert(len==3);
	file.close();
	file.open(test_file,fileAppend);
	file.write("t2",2);
	file.close();
	file.open(test_file,fileRead);
	assert(file.read(buf,8)==6);
	file.close();
}
bool test_pipe() {
	return true;
}
void test_regex() {
	KReg reg;
	reg.setModel("s",0);
	int ovector[6];
	int ret = reg.match("sjj",-1,PCRE_PARTIAL,ovector,6);
	//printf("ret=%d\n",ret);
	//KRegSubString *ss = reg.matchSubString("t", 1, 0);
	//assert(ss);
}
/*
void test_cache()
{
	const char *host = "abcdef";
	for (int i=0;i<100;i++) {
		std::stringstream s;
		int h = rand()%6;
		s << "http://" << host[h] << "/" << rand();
		KHttpObject *obj = new KHttpObject;
		create_http_object(obj,s.str().c_str(),false);
		obj->release();
	}
}
*/
void test_htaccess() {
	//static const char *file = "/home/keengo/httpd.conf";
	//KHtAccess htaccess;
	//KFileName file;
	//file.setName("/","/home/keengo/");
	//printf("result=%d\n",htaccess.load("/","/home/keengo/"));
}
void test_expr() {
	static const char *to_test[] = { "name=name", "name!=name", "name!=name2",
			"name=/na/", "name=/na/ && name!=aaa",
			"test=ddd || (test=test && a=/b/)" };
	static ExpResult result[] = { Exp_true, Exp_false, Exp_true, Exp_true,
			Exp_true, Exp_false };
	KSSIContext context;
	for (size_t i = 0; i < sizeof(result) / sizeof(ExpResult); i++) {
		KExpressionParseTree *parser = new KExpressionParseTree;
		parser->setContext(&context);
		char *buf = xstrdup(to_test[i]);
		ExpResult result1 = parser->evaluate(buf);
		if (result1 != result[i]) {
			fprintf(stderr, "test exp[%s] result=[%d] test result[%d] failed.",
					buf, result[i], result1);
			abort();
		}
		xfree(buf);
		delete parser;
	}
}
void test_file(const char *path)
{
#if 0
	KFileName file;
	std::string doc_root = "d:\\project\\kangle\\www\\";
	bool result = file.setName(doc_root.c_str(), path, FOLLOW_LINK_NO|FOLLOW_PATH_INFO);
	printf("triped_path=[%s],result=%d\n",path,result);
	if(result){
		printf("pre_dir=%d,is_dir=%d,path_info=[%d],filename=[%s]\n",file.isPrevDirectory(),file.isDirectory(),file.getPathInfoLength(),file.getName());
	}
#endif
}
void test_files()
{
	test_file("/test.php");
	test_file("/test.php/a/b");
	test_file("/");
	test_file("/a/");
	test_file("/a");
	test_file("/b");

}
void test_timematch()
{
	KTimeMatch *t = new KTimeMatch;
	t->set("* * * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("*/5  */5 * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("2-10/3,50  0-6 * * *");
	t->Show();
	delete t;
}
/////////[88]
bool test() {
	//printf("sizeof(KSelectable) = %d\n",sizeof(KSelectable));
	//printf("sizeof(KConnectionSelectable)=%d\n",sizeof(KConnectionSelectable));
	//printf("sizeof(KHttpRequest) = %d\n",sizeof(KHttpRequest));
	//printf("sizeof(pthread_mutex_t)=%d\n",sizeof(pthread_mutex_t));
	//printf("sizeof(lock)=%d\n",sizeof(KMutex));
	//printf("sizeof(HttpObjectIndex)=%d\n",sizeof(HttpObjectIndex));
	//printf("sizeof(HttpObject)=%d\n",sizeof(KHttpObject));
	//test_cache();
	//test_file();
	//test_timematch();
	//test_xml();
	//printf("sizeof(kgl_str_t)=%d\n",sizeof(kgl_str_t));
	buff b;
	b.flags = 0;
	test_regex();
	test_htaccess();
	test_expr();
	//printf("sizeof(KHttpRequest)=%d\n",sizeof(KHttpRequest));
	//	test_pipe();
	//printf("sizeof(obj)=%d,%d\n",sizeof(KHttpObject),sizeof(HttpObjectIndex));
	time_t nowTime = time(NULL);
	char timeStr[41];
	mk1123time(nowTime, timeStr, sizeof(timeStr) - 1);
	//printf("parse1123=%d\n",parse1123time(timeStr));
	assert(parse1123time(timeStr)==nowTime);
	INT64 t = 123;
	char buf[INT2STRING_LEN];
	int2string(t, buf);
	//printf("sizeof(sockaddr_i)=%d\n",sizeof(sockaddr_i));
	if (strcmp(buf, "123") != 0) {
		fprintf(stderr, "Warning int2string function is not correct\n");
		assert(false);
	} else if (string2int(buf) != 123) {
		fprintf(stderr, "Warning string2int function is not correct\n");
		assert(false);
	}
	KHttpField field;
//	test_files();
	return true;
}
