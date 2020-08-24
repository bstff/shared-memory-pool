#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_  
#include "mm.h"


// static int mem_pool_mutex  = SemUtil::get(MEM_POOL_COND_KEY, 1);

static inline void mem_pool_init(const int key, size_t heap_size) {
	if(mm_init(key, heap_size)) {
		 
	}
}

static inline void mem_pool_destroy(void) {
	if(mm_destroy()) {

	}
	
}

static inline void *mem_pool_malloc (size_t size) {
	void *ptr;
	while( (ptr = mm_malloc(size)) == NULL ) {
		NULL;
	}
	
	return ptr;
}


static inline void mem_pool_free (void *ptr) {
	mm_free(ptr);

}


template <typename T>
static inline  T* mem_pool_attach() {
	void *ptr = NULL;
	// T* tptr;
// printf("mem_pool_malloc_by_key  malloc before %d, %p\n", key, ptr);
  if(ptr == NULL || ptr == (void *)1 ) {
    ptr = mm_malloc(sizeof(T));
    new(ptr) T;
// printf("mem_pool_malloc_by_key  use new %d, %p\n", key, ptr);
  }
  return (T*)ptr; 
}


static inline void *mem_pool_realloc (void *ptr, size_t size) {
	return mm_realloc(ptr, size);
}


// extern int mm_checkheap(int verbose);


#endif