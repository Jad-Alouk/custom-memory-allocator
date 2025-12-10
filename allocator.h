#ifndef MEM_ALLOC_H
#define MEM_ALLOC_H

#include <stddef.h>

void *alloc_mem(size_t size);
void free_mem(void *ptr);

#endif