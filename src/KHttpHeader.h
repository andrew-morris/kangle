#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
#include "KSocket.h"
#include "ksapi.h"
enum know_response_header
{
	response_content_range,
};
struct kgl_str_t
{
	char *data;
	size_t len;
};
struct kgl_keyval_t {
	kgl_str_t   key;
	kgl_str_t   value;
};
#define kgl_expand_string(str)     (char *)str ,sizeof(str) - 1
#define kgl_string(str)     { (char *) str,sizeof(str) - 1 }
#define kgl_null_string     {  NULL,0 }
#define kgl_str_set(str, text) \
    (str)->len = sizeof(text) - 1; (str)->data = (char *) text
#define kgl_str_null(str)   (str)->len = 0; (str)->data = NULL

inline void free_header(struct KHttpHeader *av) {
	struct KHttpHeader *next;
	while (av) {
		next = av->next;
		free(av->attr);
		free(av->val);
		free(av);
		av = next;
	}
}
#endif /*KHTTPHEADER_H_*/
