#ifndef KTEMPFILE_H
#define KTEMPFILE_H
#include "global.h"
#include <stdio.h>
#include <string>
#include "forwin32.h"
#include "KSocketBuffer.h"
#include "KFile.h"
#ifdef ENABLE_TF_EXCHANGE
class KHttpRequest;
class KTempFile
{
public:
	KTempFile();
	~KTempFile();
	//初始化,length是已经长度，如果为-1,则长度未知
	void init(INT64 length);
	bool writeBuffer(KHttpRequest *rq,const char *buf,int len);
	char *writeBuffer(int &size);
	bool writeSuccess(KHttpRequest *rq,int got);
	void switchRead();
	//重新读
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//返回true,继续读，否则读完了
	bool readSuccess(int got);
	bool isWrite()
	{
		return writeModel;
	}
	bool checkLast(int got)
	{
		return total_size + got >= length;
	}
	INT64 getSize()
	{
		return total_size;
	}
private:
	bool openFile(KHttpRequest *rq);
	bool dumpOutBuffer();
	bool dumpInBuffer();
	KFile fp;
	KSocketBuffer buffer;
	INT64 length;
	//总长度
	INT64 total_size;
	std::string file;
	bool writeModel;
};
//读post数据到临时文件
void stageReadTempFile(KHttpRequest *rq);
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param);
#endif
#endif
