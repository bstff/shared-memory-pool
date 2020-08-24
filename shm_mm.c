#include "shm_mm.h"
#include "mem_pool.h"

void shm_init(const int key, int size) {
	mem_pool_init(key, size);
}

void shm_destroy() {
	mem_pool_destroy();
}
