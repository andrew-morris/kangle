#ifndef KLISTNODE_H
#define KLISTNODE_H
#include <stdlib.h>
class KListNode
{
public:
	KListNode(void);
	virtual ~KListNode(void);
	KListNode *next;
	KListNode *prev;
};
#endif
