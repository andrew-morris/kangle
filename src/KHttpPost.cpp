#include <string.h>
#include "KHttpPost.h"
#include "malloc_debug.h"

static int my_htoi(char *s) {
	int value;
	int c;

	c = ((unsigned char *) s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char *) s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}

int get_tmp(char m_char) {
	if (m_char <= '9' && m_char >= '0')
		return 0x30;
	if (m_char <= 'f' && m_char >= 'a')
		return 0x57;
	if (m_char <= 'F' && m_char >= 'A')
		return 0x37;
	return 0;

}
int url_decode(char *str, int len, int *flag,bool space2plus) {
	char *dest = str;
	char *data = str;
	if (len == 0) {
		len = strlen(str);
	}
	while (len--) {
		if (space2plus && *data == '+') {
			*dest = ' ';
			if (flag) {
				SET(*flag,RQ_URL_ENCODE);
			}
		} else if (*data == '%' && len >= 2 && isxdigit((unsigned char) *(data + 1))
				&& isxdigit((unsigned char) *(data + 2))) {
			*dest = (char) my_htoi(data + 1);
			data += 2;
			len -= 2;
			if (flag) {
				SET(*flag,RQ_URL_ENCODE);
			}
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	*dest = '\0';
	return dest - str;
}
char *my_strtok(char *msg, char split, char **ptrptr) {
	char *str;
	if (msg)
		str = msg;
	else
		str = *ptrptr;
	if (str == NULL)
		return NULL;
	int len = strlen(str);
	if (len <= 0) {
		return NULL;
	}
	for (int i = 0; i < len; i++) {
		if (str[i] == split) {
			str[i] = 0;
			*ptrptr = str + i + 1;
			return str;
		}
	}
	*ptrptr = NULL;
	return str;

}
bool KHttpPost::parseUrlParam(char *param,KUrlValue *uv) {
	char *name;
	char *value;
	char *tmp;
	char *msg;
	char *ptr;
	for (size_t i = 0; i < strlen(param); i++) {
		if (param[i] == '\r' || param[i] == '\n') {
			param[i] = 0;
			break;
		}
	}
	//	url_unencode(param);
	//printf("param=%s.\n",param);
	tmp = param;
	char split = '&';
	//	strcpy(split,"=");
	while ((msg = my_strtok(tmp, split, &ptr)) != NULL) {
		tmp = NULL;
		char *p = strchr(msg,'=');
		if(p==NULL){
			value=(char *)"";
		}else{
			*p = '\0';
			value = p+1;
			if(urldecode){
				url_decode(value,0,NULL,true);
			}			
		}
		name = msg;
		if(urldecode){
			url_decode(name,0,NULL,true);
		}
		uv->add(name,value);
	}
	return true;
}
KHttpPost::KHttpPost()
{
	buffer = NULL;
	urldecode = true;
}

KHttpPost::~KHttpPost(void)
{
	if(buffer){
		xfree(buffer);
	}
}
bool KHttpPost::init(int totalLen)
{
	if(totalLen>MAX_POST_SIZE){
		return false;
	} 
	this->totalLen = totalLen;
	buffer = (char *)xmalloc(totalLen+1);
	hot = buffer;
	return buffer!=NULL;

}
bool KHttpPost::addData(const char *data,int len)
{
	int used = hot - buffer;
	if(totalLen-used < len){
		return false;
	}
	memcpy(hot,data,len);
	hot+=len;
	return true;
}
bool KHttpPost::readData(KRStream *st)
{	
	int used = hot - buffer;
	int leftRead = totalLen - used;
	for(;;){
		if(leftRead<=0){
			return true;
		}
		int read_len = st->read(hot,leftRead);
		if(read_len<=0){
			return false;
		}
		leftRead-=read_len;
		hot+=read_len;
	}
}
bool KHttpPost::parse(KUrlValue *uv)
{
	*hot = '\0';
	//printf("post data=[%s]\n",buffer);
	return parseUrlParam(buffer,uv);
}
