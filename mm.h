#ifndef MM_HDR_H
#define MM_HDR_H      /* Prevent accidental double inclusion */

#include <stddef.h>

extern bool mm_init(const int key, size_t heap_size);
extern bool mm_destroy(void);

extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

#endif
