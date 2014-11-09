#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
struct	KHttpHeader {
	char		*attr;
	char		*val;
	struct	KHttpHeader	*next;
};
void free_header(struct KHttpHeader *);
#endif /*KHTTPHEADER_H_*/
