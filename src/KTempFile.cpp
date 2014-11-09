#include "KTempFile.h"
#include "KHttpRequest.h"
#include "malloc_debug.h"
#include "KSelector.h"
#include "http.h"
#include "directory.h"
#include "forwin32.h"
#ifdef ENABLE_TF_EXCHANGE

#ifdef _WIN32
#define tunlink(f)             
#else
#define tunlink(f)             unlink(f)
#endif
KTempFile::KTempFile()
{
	total_size = 0;
	writeModel = true;
}
KTempFile::~KTempFile()
{
	if (fp.opened()) {
		tunlink(file.c_str());
	}
}
void KTempFile::init(INT64 length)
{
	total_size = 0;
	this->length = length;
	if (fp.opened()) {
		fp.close();
		tunlink(file.c_str());
	}
	buffer.destroy();
	writeModel = true;
}
bool KTempFile::writeBuffer(KHttpRequest *rq,const char *buf,int len)
{
	while (len>0) {
		int size;
		char *t = writeBuffer(size);
		if (t==NULL || size==0) {
			return false;
		}
		size = MIN(len,size);
		kassert(size>0);
		memcpy(t,buf,size);
		buf += size;
		len -= size;
		if (!writeSuccess(rq,size)) {
			return false;
		}
	}
	return true;
}
char *KTempFile::writeBuffer(int &size)
{
	char *t = buffer.getWBuffer(size);
	if (t && length>0) {
		size = (int)MIN((INT64)size,(length - total_size));
	}
	return t;
}
bool KTempFile::writeSuccess(KHttpRequest *rq,int got)
{
	total_size += got;
	buffer.writeSuccess(got);
	if (length>0 && total_size>=length) {
		return false;
	}
	if (buffer.getLen() < conf.buffer) {
		return true;
	}
	if (!openFile(rq)) {
		return false;
	}
	return dumpOutBuffer();
}
bool KTempFile::dumpOutBuffer()
{
	nbuff *head = buffer.getHead();
	while (head) {
		if (head->used>0) {
			if ((int)head->used!=(int)fp.write(head->data,head->used)) {
				return false;
			}
		}
		head = head->next;
	}
	buffer.destroy();
	return true;
}
bool KTempFile::dumpInBuffer()
{
	kassert(buffer.getHead()==NULL);
	while(buffer.getLen()<conf.buffer){
		nbuff *buf = (nbuff *)malloc(NBUFF_SIZE);
		buf->used = fp.read(buf->data,NBUFF_CHUNK);
		if (buf->used<=0) {
			free(buf);
			break;
		}
		buffer.appendBuffer(buf);
	}
	buffer.startRead();
	return buffer.getLen()>0;
}
bool KTempFile::openFile(KHttpRequest *rq)
{
	if (fp.opened()) {
		return true;
	}
	std::stringstream s;
	s << conf.tmppath << "krf" << m_ppid << "_" << m_pid << "_" << (void *)rq;
	s.str().swap(file);
	return fp.open(file.c_str(),fileWriteRead,KFILE_TEMP_MODEL);
}
void KTempFile::switchRead()
{	
	writeModel = false;
	if (fp.opened()) {
		dumpOutBuffer();
		fp.seek(0,seekBegin);
		dumpInBuffer();
	} else {
		buffer.startRead();
	}
}
char *KTempFile::readBuffer(int &size)
{
	return buffer.getRBuffer(size);
}
bool KTempFile::readSuccess(int got)
{
	bool result = buffer.readSuccess(got);
	if (result) {
		return true;
	}
	if (!fp.opened()) {
		return false;
	}
	buffer.destroy();
	result = dumpInBuffer();
	if (!result) {
		return false;
	}
	return true;
}
void KTempFile::resetRead()
{
	if (fp.opened()) {
		buffer.destroy();
		fp.seek(0,seekBegin);
		dumpInBuffer();
	} else {
		buffer.startRead();
	}
}
int KTempFile::readBuffer(char *buf,int size)
{
	int len;
	char *t = buffer.getRBuffer(len);
	if (t) {
		size = MIN(size,len);
		memcpy(buf,t,size);
		buffer.readSuccess(size);
		return size;
	}
	if (fp.opened()) {
		size = fp.read(buf,size);
		return size;
	}
	return 0;
}
inline bool tempFileReadPostResult(KHttpRequest *rq,int got)
{
	kassert(rq->tf!=NULL);
	int buf_size;
	char *buf = rq->tf->writeBuffer(buf_size);
	if (got==0) {		
		got = rq->server->read(buf,buf_size);
#ifdef KSOCKET_SSL
		if (got<=0 &&  TEST(rq->workModel,WORK_MODEL_SSL)) {
			KSSLSocket *sslsocket = static_cast<KSSLSocket *>(rq->server);
			if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_READ) {
				rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_READ);
				return false;
			}
		}
#endif
	}
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageTempFileWriteEnd(rq);
		return false;
	}
	/*
	if (!TEST(rq->flags,RQ_POST_UPLOAD) && !rq->tf->countPostParams(buf,got)) {
		klog(KLOG_ERR,"POST params is too big,drop it. src %s:%d\n",rq->server->get_remote_ip().c_str(),rq->server->get_remote_port());
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		send_error(rq,NULL,STATUS_BAD_REQUEST,"POST params is too big");
		return false;
	}
	*/
#ifdef ENABLE_INPUT_FILTER
	if (rq->if_ctx && JUMP_DENY==rq->if_ctx->check(buf,got,rq->tf->checkLast(got))) {
		denyInputFilter(rq);
		return false;
	}
#endif
	if (!rq->tf->writeSuccess(rq,got)) {
		//已经读完了post数据，开始处理请求，视情况放入队列中
		rq->tf->switchRead();
		asyncLoadHttpObject(rq);
	} else {
#ifdef KSOCKET_SSL
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			return true;
		}
#endif
		rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_READ);
	}
	return false;
}
//临时文件读post的处理器
void handleTempFileReadPost(KSelectable *st,int got)
{
	KHttpRequest *rq = static_cast<KHttpRequest *>(st);
	while (tempFileReadPostResult(rq,got)) {
		got = 0;
	}
}
void stageReadTempFile(KHttpRequest *rq)
{
	rq->handler = handleTempFileReadPost;
#ifdef KSOCKET_SSL
	if (TEST(rq->workModel,WORK_MODEL_SSL)) {
		handleTempFileReadPost(rq,0);
		return;
	}
#endif
	rq->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_TF_READ);
}
int listHandleTempFile(const char *file,void *param)
{
	if (filencmp(file,"krf",3)!=0) {
		return 0;
	}
	int pid = atoi(file+3);
	if (pid==m_ppid) {
		return 0;
	}
	std::stringstream s;
	s << conf.tmppath << file;
	klog(KLOG_NOTICE,"remove uncleaned tmpfile [%s]\n",s.str().c_str());
	unlink(s.str().c_str());
	return 0;
}
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param)
{
	klog(KLOG_DEBUG,"start to clean tmp file thread...\n");
	list_dir(conf.tmppath.c_str(),listHandleTempFile,NULL);
	klog(KLOG_DEBUG,"clean tmp file done.\n");
	KTHREAD_RETURN;
}
#endif
