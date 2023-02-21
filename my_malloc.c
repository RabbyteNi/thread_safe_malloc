#define NDEBUG
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include "my_malloc.h"

void * normal_bds = NULL; /* base data segment */
pthread_mutex_t lock; /* mutex lock */
pthread_mutex_t sbrk_lock; /* exclusive lock for sbrk */
__thread void * tls_bds = NULL; /* thread-local storage */

/* To judge if the prev block and next block is adjacent
 * Return 1 if adjacent
 * Return 0 if not
 */
int
is_adjacent(
    const block * prev,
    const block * next
    ) {
  void * p = (void*) prev;
  void * n = (void*) next;
  size_t size = prev->size;
  assert(prev != NULL && next != NULL);
  if (p + BLOCK_INFO_SIZE + size == n) return 1;
  else return 0; 
}

/* To merge the current block and the next block */
void
merge_next(
    block * curr,
    block * prev,
    block * next) {
  if (next != NULL &&
      is_adjacent(curr, next)) {
    curr->size += BLOCK_INFO_SIZE + next->size;
    curr->is_free = 1;
    curr->prev = prev;
    curr->next = next->next;
    prev->next = curr;
    if (next->next) next->next->prev = curr;
  }
}

/* To merge the previous block and the current block */
void
merge_prev(block * curr,
    block * prev,
    block * next,
    void * bds) {
  if (prev != (block*)bds &&
      is_adjacent(prev, curr)) {
    prev->size += BLOCK_INFO_SIZE + curr->size;
    prev->is_free = 1;
    prev->next = next;
    if (next) {
      next->prev = prev;
    }
  }
}



/* To merge the three adjacent free blocks */
void
merge(block * curr,
    block * prev,
    block * next,
    void * bds) {
  curr->is_free = 1;
  curr->prev = prev;
  curr->next = next;
  prev->next = curr;
  if (next) next->prev = curr;
  merge_next(curr, prev, next);
  next = curr->next;
  merge_prev(curr, prev, next, bds);
}

/* To create the initial head block */
void
initialize_heap(void ** ptr2bds) {
  pthread_mutex_lock(&sbrk_lock);
  (*ptr2bds) = sbrk(BLOCK_INFO_SIZE);
  pthread_mutex_unlock(&sbrk_lock);
  block * head = (block*)(*ptr2bds);
  head->size = 0;
  head->is_free = 1;
  head->prev = NULL;
  head->next = NULL;
}

/* To split the block and set the second part as free */
void
split_block(
    block * curr,
    size_t size
    ) {
  assert(curr != NULL);
  assert(curr->is_free);
  assert(curr->size >= size);

  /* no need to split, just change flag */
  if (curr->size >= size && 
      curr->size <= size + BLOCK_INFO_SIZE) {
    curr->is_free = 0;
    curr->prev->next = curr->next;
    if (curr->next) {
      curr->next->prev = curr->prev;
    }
  }
  else {
    /* split the block */
    void * start_ptr = (void*) curr;
    block * new_block = (block*)(start_ptr + BLOCK_INFO_SIZE + size);
    block * prev = curr->prev;
    block * next = curr->next;

    new_block->size = curr->size - size - BLOCK_INFO_SIZE;
    new_block->is_free = 1;
    new_block->prev = prev;
    new_block->next = next;

    curr->size = size;
    curr->is_free = 0;

    prev->next = new_block;
    if (next) {
      next->prev = new_block;
    }
  }

}

/* extend the top of the heap 
 * Return the address of the new allocated memory is success
 * Return NULL if fail
 */
void *
extend_heap(size_t size) {
  pthread_mutex_lock(&sbrk_lock);
  void * extend_res = sbrk(BLOCK_INFO_SIZE + size);
  pthread_mutex_unlock(&sbrk_lock);
  if (extend_res == (void*)(-1)) {
    return NULL;
  }
  block * new_block = extend_res;
  new_block->size = size;
  new_block->is_free = 0;
  new_block->prev = NULL;
  new_block->next = NULL;
  return (extend_res + BLOCK_INFO_SIZE);
}

/* To iterate through the free block linked list to find the block and change the flag
*/
void
my_free(void * ptr, void * bds, int if_lock) {
  if (if_lock) pthread_mutex_lock(&lock);
  if (ptr == NULL) {
    return;
  }
  block * curr = (block*)(ptr - BLOCK_INFO_SIZE);
  if (curr->is_free) {
    return;
  }
  block * iter = bds;
  while (iter) {
    if (iter->next == NULL) break;
    else if (iter->next > curr) break;
    else if (iter == iter->next) break;
    else iter = iter->next;
  }
  if (iter == NULL) {
    return;
  }
  merge(curr, iter, iter->next, bds);
  if (if_lock) pthread_mutex_unlock(&lock);
}

/* helper function to malloc with or without lock */
void *
my_malloc(size_t size, void ** ptr2bds, int if_lock) {
  if (if_lock) pthread_mutex_lock(&lock);
  if ((*ptr2bds) == NULL) initialize_heap(ptr2bds);
  block * iter = (block*)(*ptr2bds);
  int redundancy = INT_MAX;
  block * best = NULL;
  while (iter) {
    if (iter->size == size) {
      best = iter;
      break;
    }
    if (iter->size > size && iter->size - size < redundancy) {
      redundancy = iter->size - size;
      best = iter;
    }
    iter = iter->next;
  }
  void * res = NULL;
  if (best == NULL) {
    res = extend_heap(size);
  }
  else {
    split_block(best, size);
    res = best + 1;
  }
  if (if_lock) pthread_mutex_unlock(&lock);
  return res;
}


void *
ts_malloc_lock(size_t size) {
  /* call my_malloc with lock */
  return my_malloc(size, &normal_bds, 1);
}

void *
ts_malloc_nolock(size_t size) {
  /* call my_malloc with no lock and tls_bds */
  return my_malloc(size, &tls_bds, 0);
}


void
ts_free_lock(void * ptr) {
  my_free(ptr, normal_bds, 1);
}

void
ts_free_nolock(void * ptr) {
  my_free(ptr, tls_bds, 0);
}









