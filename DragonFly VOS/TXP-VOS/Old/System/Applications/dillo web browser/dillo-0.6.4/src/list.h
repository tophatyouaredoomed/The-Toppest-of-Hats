/*
 * Fast list methods
 * Feb 2000  --Jcid
 *
 */

#ifndef __LIST_H__
#define __LIST_H__

/*
 * a_List_resize()
 *
 * Make sure there's space for 'num_items' items within the list
 * (First, allocate an 'alloc_step' sized chunk, after that, double the
 *  list size --to make it faster)
 */
#define a_List_resize(list,num_items,item_size,alloc_step) \
   if ( !list ) { \
      list = g_malloc(alloc_step * item_size); \
   } \
   if ( num_items >= alloc_step ){ \
      while ( num_items >= alloc_step ) \
         alloc_step <<= 1; \
      list = g_realloc(list, alloc_step * item_size); \
   }


/*
 * a_List_add()
 *
 * Make sure there's space for one more item within the list.
 */
#define a_List_add(list,num_items,item_size,alloc_step) \
   a_List_resize(list,num_items,item_size,alloc_step)


/*
 * a_List_remove()
 *
 * Quickly remove an item from the list
 * ==> We preserve relative position, but not the element index <==
 */
#define a_List_remove(list, item, num_items) \
   if ( list && item < num_items ) { \
      list[item] = list[--num_items]; \
   }


#endif /* __LIST_H__ */
