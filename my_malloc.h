#ifndef __MY_ALLOC_H__
#define __MY_ALLOC_H__


typedef struct _block {
  size_t size;
  int is_free;
  struct _block * prev;
  struct _block * next;
} block ;

#define BLOCK_INFO_SIZE sizeof(block)

/* Thread Safe malloc/free: locking version */
void * ts_malloc_lock(size_t size);
void ts_free_lock(void* ptr);

/* Thread Safe malloc/free: non-locking version */
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);


#endif
