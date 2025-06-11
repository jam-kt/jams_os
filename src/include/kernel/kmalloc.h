#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <stddef.h>


void *kmalloc(size_t size);
void kfree(void *addr);

#endif