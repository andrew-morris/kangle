#ifndef KLISsadfasdf1112T_H
#define KLISsadfasdf1112T_H

#define klist_init(list) do {\
        (list)->next = (list);\
        (list)->prev = (list);\
} while(0)
#define klist_empty(list) ((list)->next == list)
#define klist_insert(link,  new_link)   do {\
        (new_link)->prev          = (link)->prev;\
        (new_link)->next          = (link);\
        (new_link)->prev->next    = (new_link);\
        (new_link)->next->prev    = (new_link);\
} while (0)

#define klist_append(list,  new_link) klist_insert(list,new_link)
#define klist_prepend(list, new_link) klist_insert((list)->next, new_link)
#define klist_remove(link) do {\
        (link)->prev->next = (link)->next;\
        (link)->next->prev = (link)->prev;\
} while(0)
#define klist_head(list) (list)->next;
#define klist_end(list)  (list)->prev;
#define klist_foreach(pos, list)                  \
        for (pos = (list)->next;                  \
		pos != list;                      \
		pos = pos->next)
#define klist_rforeach(pos, list)                 \
        for (pos = (list)->prev;                  \
		pos != list;                      \
		pos = pos->prev)
#include "KListNode.h"
class KList
{
public:
	KList(void);
	virtual ~KList(void);
	virtual void push_back(KListNode *node)
	{
		node->next = NULL;
		node->prev = end;
		if(end==NULL){
			head = end = node;
		}else{
			end->next = node;
			end = node;
		}
	}
	virtual void push_front(KListNode *node)
	{
		node->prev = NULL;
		node->next = head;
		if(head==NULL){
			head = end = node;
		}else{
			head->prev = node;
			head = node;
		}
	}
	virtual void remove(KListNode *node)
	{
		if(node == head){
			head = head->next;
		}
		if(node == end){
			end = end->prev;
		}
		if(node->prev){
			node->prev->next = node->next;
		}
		if(node->next){
			node->next->prev = node->prev;
		}
		node->next = NULL;
		node->prev = NULL;
		//return node->next;
	}
	KListNode *getHead()
	{
		return head;
	}
	KListNode *getEnd()
	{
		return end;
	}
protected:
	KListNode *head;
	KListNode *end;
};
#endif
