#include <string.h>
#include <stdlib.h>
#include "clist.h"


struct clist_node_t {
    clist_node   *next;
    clist_node   *prev;
    void         *data;
};

/*   alloc: alloc new node
 *   data: item data pointer
 *   return:  return node pointer
 */
clist_node* clist_node_alloc(uint64_t typesize, const void *data) {
	clist_node* node = (clist_node*)malloc(sizeof(clist_node));
    node->data = malloc(typesize);
    if (data != NULL) {
    	memcpy(node->data, data, typesize);
    }
	node->next = node->prev = node;
	return node;
}

/*   insert: insert new node
 *   head: node pointer , node insert behind head
 *   node: node pointer
 *   return:  return node pointer
 */
clist_node* clist_node_insert(clist_node* head, clist_node* node) {
	clist_node *next = head->next;
	head->next       = node;
	node->next       = next;
	next->prev       = node;
	node->prev       = head;
	return head;
}

/*   erase: erase node
 *   node: node pointer
 *   return:  return node pointer
 */
clist_node* clist_node_erase(clist_node* node) {
	clist_node *prev = node->prev, *next = node->next;
	prev->next = next;
	next->prev = prev;
	return node;
}

/*   free: free node
 *   node: node pointer
 */
void        clist_node_free(clist_node *node) {
	clist_node *prev = node->prev, *next = node->next;
	prev->next = next;
	next->prev = prev;
	if (node->data)
		free(node->data);
	free(node);
}

/*   get data: get node data pointer
 *   node: node pointer
 *   return: return data pointer
 */
void*       clist_node_data(clist_node* node) {
	return node->data;
}

/*   next: get next node pointer
 *   node: node pointer
 *   return: return next node pointer
 */
clist_node* clist_node_next(clist_node* node) {
	return node->next;
}

/*   prev: get prev node pointer
 *   node: node pointer
 *   return: return prev node pointer
 */
clist_node* clist_node_prev(clist_node* node) {
	return node->prev;
}


struct clist_data_t {
	clist                      list;
	clist_node                 head;
    uint64_t                   count;
    uint64_t                   typesize;
};

typedef struct clist_data_t  clist_data;

/*   clear: clear data, but not free
 *   thiz: clist pointer
 */
static    void    clist_static_clear(clist *_thiz) {
	clist_node *node = NULL, *next = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	if (thiz->count <= 0)
		return;
	node = thiz->head.next;
	while(node != &(thiz->head)) {
		next = node->next;
        clist_node_free(node);
        node = next;
	}
	thiz->head.next = thiz->head.prev = &(thiz->head);
    thiz->count = 0;
}

/*   free: free thiz
 *   thiz: clist pointer
 */
static    void    clist_static_free(clist *_thiz) {
	clist_node *node = NULL, *next = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	if (thiz->count <= 0) {
		free(thiz);
		return;
	}
	node = thiz->head.next;
	while(node != &(thiz->head)) {
		next = node->next;
        clist_node_free(node);
        node = next;
	}
	thiz->head.next = thiz->head.prev = &(thiz->head);
    thiz->count = 0;
	free(thiz);
}

/*   typesize: get item size
 *   thiz: clist pointer
 *   return  item size > 0
 */
static uint64_t    clist_static_typesize(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return 0;
	thiz = (clist_data*) _thiz;
	return thiz->typesize;
}

/*   size: get item count
 *   thiz: clist pointer
 *   return  item count > 0
 */
static uint64_t    clist_static_size(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return 0;
	thiz = (clist_data*) _thiz;
	return thiz->count;
}

/*   empty: item count == 0
 *   thiz: clist pointer
 *   return  item count == 0
 */
static uint8_t    clist_static_empty(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return 0;
	thiz = (clist_data*) _thiz;
	if (thiz->count == 0)
		return 1;
	return 0;
}

/*   back: last item pointer
 *   thiz: clist pointer
 *   return last item pointer
 */
static    void*    clist_static_back(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
	if (thiz->head.prev == &(thiz->head))
		return NULL;
    return clist_node_data(thiz->head.prev);
}

/*   front: first item pointer
 *   thiz: clist pointer
 *   return first item pointer
 */
static    void*    clist_static_front(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
	if (thiz->head.next == &(thiz->head))
		return NULL;
    return clist_node_data(thiz->head.next);
}

/*   begin: first node pointer
 *   thiz: clist pointer
 *   return first node pointer
 */
static    clist_node*    clist_static_begin(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    return thiz->head.next;
}

/*   end: last item pointer + typesize
 *   thiz: clist pointer
 *   return last item pointer + typesize
 */
static    clist_node*    clist_static_end(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    return &(thiz->head);
}

/*   rbegin: last item pointer
 *   thiz: clist pointer
 *   return last item pointer
 */
static    clist_node*    clist_static_rbegin(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    return thiz->head.prev;
}

/*   back: last item pointer
 *   thiz: clist pointer
 *   return last item pointer
 */
static    clist_node*    clist_static_rend(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    return &(thiz->head);
}

/*   at: index item pointer
 *   thiz: clist pointer
 *   index: item index
 *   return index item pointer
 */
static    clist_node*    clist_static_at(clist *_thiz, uint64_t index) {
	clist_node *node = NULL;
	clist_data *thiz = NULL;
	uint64_t i = 0;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    if (index >= thiz->count)
        return NULL;
    node = thiz->head.next;
    while (i++ < index) {
    	node = node->next;
    }
    return node;
}

static    clist_node*    clist_static_find(clist *_thiz, const void* val) {
	clist_node *node = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return NULL;
	thiz = (clist_data*) _thiz;
    node = thiz->head.next;
    while (node != &(thiz->head)) {
    	if (memcmp(node->data, val, thiz->typesize) == 0) {
    		return node;
    	}
    	node = node->next;
    }
    return NULL;
}

/*   push_back: add last item behind 
 *   thiz: clist pointer
 *   val:  item pointer
 */
static    void    clist_static_push_back(clist *_thiz, const void* val) {
	clist_node *node = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	node = clist_node_alloc(thiz->typesize, val);
	clist_node_insert(thiz->head.prev, node);
	++thiz->count;
}

/*   push_front: add first item before 
 *   thiz: clist pointer
 *   val:  item pointer
 */
static    void    clist_static_push_front(clist *_thiz, const void* val) {
	clist_node *node = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	node = clist_node_alloc(thiz->typesize, val);
	clist_node_insert(&(thiz->head), node);
	++thiz->count;
}

/*   pop_back: delete last item 
 *   thiz: clist pointer
 */
static    void    clist_static_pop_back(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	if (thiz->count <= 0)
		return;
    clist_node_free(thiz->head.prev);
    --thiz->count;
}

/*   pop_front: delete first item 
 *   thiz: clist pointer
 */
static    void    clist_static_pop_front(clist *_thiz) {
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	if (thiz->count <= 0)
		return;
    clist_node_free(thiz->head.next);
    --thiz->count;	
}

/*   remove: delete position item 
 *   thiz: clist pointer
 *   position: item pointer
 */
static    void    clist_static_remove(clist *_thiz, void* val) {
	clist_node *node = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
    node = thiz->head.next;
    while (node != &(thiz->head)) {
    	if (memcmp(node->data, val, thiz->typesize) == 0) {
		    clist_node_free(node);
		    --thiz->count;
		    return;
    	}
    	node = node->next;
    }
}

/*   assign: copy value from first to last items 
 *   thiz: clist pointer
 *   first: begin item pointer
 *   last: last item pointer
 */
static    void    clist_static_assign(clist *_thiz, void* first, void* last) {
	void *val = NULL;
	clist_data *thiz = NULL;
	if ((_thiz == NULL) || (first == NULL) || (last == NULL))
		return;
	thiz = (clist_data*) _thiz;
    _thiz->clear(_thiz);
    for (val = first; val != last; val += thiz->typesize) {
	     _thiz->push_back(_thiz, val);
    }
}

/*   reverse: first and last items change 
 *   thiz: clist pointer
 */
static    void    clist_static_reverse(clist *_thiz) {
	clist_node *node = NULL, *next = NULL;
	clist_data *thiz = NULL;
	if (_thiz == NULL) 
		return;
	thiz = (clist_data*) _thiz;
	if (thiz->count <= 0)
		return;
    node = thiz->head.next;
    while(node != &(thiz->head)) {
    	next = node->next;
        clist_node_erase(node);
        clist_node_insert(&(thiz->head), node);
        node = next;
    }
}

/*   copy: copy value from thiz to that
 *   thiz: clist pointer
 *   that: clist pointer
 */
static    void    clist_static_copy(clist *_thiz, clist *_that) {
	clist_node *node = NULL;
	clist_data *thiz = NULL, *that = NULL;
	if ((_thiz == NULL) || (_that == NULL)) {
		return;
	}
	thiz = (clist_data*) _thiz;
	that = (clist_data*) _that;
    node = thiz->head.next;
    _that->clear(_that);
    that->typesize = thiz->typesize;
    while (node != &(thiz->head)) {
        _that->push_back(_that, clist_node_data(node)); 
    	node = node->next;
    }
}

/*   equal: compare thiz with that
 *   thiz: clist pointer
 *   that: clist pointer
 *   return: thiz == that
 */
static    uint8_t    clist_static_equal(clist *_thiz, clist *_that) {
	clist_node *node = NULL, *next = NULL;
	clist_data *thiz = NULL, *that = NULL;
	if ((_thiz == NULL) || (_that == NULL)) {
		return 0;
	}
	thiz = (clist_data*) _thiz;
	that = (clist_data*) _that;
	if ((thiz->typesize != that->typesize) || (thiz->count != that->count)) {
		return 0;
	}

    node = thiz->head.next;
    next = that->head.next;
    while (node != &(thiz->head)) {
    	if (memcmp(node->data, next->data, thiz->typesize) != 0)
    		return 0;
    	node = node->next;
    	next = next->next;
    }
    return 1;	
}

/*   clist_alloc: malloc clist pointer
 *   typesize: clist item size
 *   return: clist pointer
 */
clist* clist_alloc(uint64_t typesize) {
	clist *thiz = NULL;
	clist_data *thiz_data = NULL;
	if (typesize <= 0) {
		return NULL;
	}

	thiz_data = (clist_data *)malloc(sizeof(clist_data));
	if (thiz_data == NULL) {
		return NULL;
	}
    
    thiz_data->count = 0;
    thiz_data->typesize  = typesize;
    thiz_data->head.data = NULL;
    thiz_data->head.next = thiz_data->head.prev = &(thiz_data->head);
    thiz = (clist*) &(thiz_data->list);

	thiz->clear = clist_static_clear;
	thiz->free  = clist_static_free;
	thiz->typesize  = clist_static_typesize;
	thiz->size  = clist_static_size;
	thiz->empty  = clist_static_empty;

	thiz->back  = clist_static_back;
	thiz->front  = clist_static_front;
	thiz->begin  = clist_static_begin;
	thiz->end  = clist_static_end;
	thiz->rbegin  = clist_static_rbegin;
	thiz->rend  = clist_static_rend;

	thiz->at  = clist_static_at;
	thiz->find  = clist_static_find;

	thiz->push_back  = clist_static_push_back;
	thiz->push_front  = clist_static_push_front;
	thiz->pop_back  = clist_static_pop_back;
	thiz->pop_front  = clist_static_pop_front;

	thiz->remove  = clist_static_remove;
	thiz->assign  = clist_static_assign;
	thiz->reverse  = clist_static_reverse;
	thiz->copy  = clist_static_copy;
	thiz->equal  = clist_static_equal;

    return thiz;
}
