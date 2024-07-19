
#ifndef clist_H_INCLUDED
#define clist_H_INCLUDED


#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C"{
#endif

struct clist_node_t;
typedef struct  clist_node_t  clist_node;

/*   alloc: alloc new node
 *   data: item data pointer
 *   return:  return node pointer
 */
clist_node* clist_node_alloc(uint64_t typesize, const void *data);

/*   insert: insert new node
 *   head: node pointer , node insert behind head
 *   node: node pointer
 *   return:  return node pointer
 */
clist_node* clist_node_insert(clist_node* head, clist_node* node);

/*   erase: erase node
 *   node: node pointer
 *   return:  return node pointer
 */
clist_node* clist_node_erase(clist_node* node);

/*   free: free node
 *   node: node pointer
 */
void        clist_node_free(clist_node *node);

/*   get data: get node data pointer
 *   node: node pointer
 *   return: return data pointer
 */
void*       clist_node_data(clist_node* node);

/*   next: get next node pointer
 *   node: node pointer
 *   return: return next node pointer
 */
clist_node* clist_node_next(clist_node* node);

/*   prev: get prev node pointer
 *   node: node pointer
 *   return: return prev node pointer
 */
clist_node* clist_node_prev(clist_node* node);




struct clist_t;
typedef struct clist_t clist;


struct clist_t {
/*   clear: clear data, but not free
 *   thiz: clist pointer
 */
    void      (*clear)(clist *thiz);

/*   free: free thiz
 *   thiz: clist pointer
 */
    void      (*free)(clist *thiz);

/*   typesize: get item size
 *   thiz: clist pointer
 *   return  item size > 0
 */
    uint64_t  (*typesize)(clist *thiz);

/*   size: get item count
 *   thiz: clist pointer
 *   return  item count > 0
 */
    uint64_t  (*size)(clist *thiz);

/*   empty: item count == 0
 *   thiz: clist pointer
 *   return  item count == 0
 */
    uint8_t   (*empty)(clist *thiz);

/*   back: last item pointer
 *   thiz: clist pointer
 *   return last item pointer
 */
    void*     (*back)(clist *thiz);

/*   front: first item pointer
 *   thiz: clist pointer
 *   return first item pointer
 */
    void*     (*front)(clist *thiz);

/*   begin: first node pointer
 *   thiz: clist pointer
 *   return first node pointer
 */
    clist_node*     (*begin)(clist *thiz);

/*   end: last node pointer
 *   thiz: clist pointer
 *   return last node pointer
 */
    clist_node*     (*end)(clist *thiz);

/*   rbegin: last node pointer
 *   thiz: clist pointer
 *   return last node pointer
 */
    clist_node*     (*rbegin)(clist *thiz);

/*   back: last node pointer
 *   thiz: clist pointer
 *   return last node pointer
 */
    clist_node*     (*rend)(clist *thiz);


    clist_node*     (*at)(clist *thiz, uint64_t index);


    clist_node*     (*find)(clist *thiz, const void* val);

/*   push_back: add last item behind 
 *   thiz: clist pointer
 *   val:  item pointer
 */
    void      (*push_back)(clist *thiz, const void* val);

/*   push_front: add first item before 
 *   thiz: clist pointer
 *   val:  item pointer
 */
    void      (*push_front)(clist *thiz, const void* val);

/*   pop_back: delete last item 
 *   thiz: clist pointer
 */
    void      (*pop_back)(clist *thiz);

/*   pop_front: delete first item 
 *   thiz: clist pointer
 */
    void      (*pop_front)(clist *thiz);

/*   remove: delete position item 
 *   thiz: clist pointer
 *   position: item pointer
 */
    void      (*remove)(clist *thiz, void* val);

/*   assign: copy value from first to last items 
 *   thiz: clist pointer
 *   first: begin item pointer
 *   last: last item pointer
 */
    void      (*assign)(clist *thiz, void* first, void* last);

/*   reverse: first and last items change 
 *   thiz: clist pointer
 */
    void      (*reverse)(clist *thiz);

/*   copy: copy value from thiz to that
 *   thiz: clist pointer
 *   that: clist pointer
 */
    void      (*copy)(clist *thiz, clist *that);

/*   equal: compare thiz with that
 *   thiz: clist pointer
 *   that: clist pointer
 *   return: thiz == that
 */
    uint8_t   (*equal)(clist *thiz, clist *that);
};

/*   clist_alloc: malloc clist pointer
 *   typesize: clist item size
 *   return: clist pointer
 */
clist* clist_alloc(uint64_t typesize);


#ifdef __cplusplus
}
#endif

#endif 