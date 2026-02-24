#include <stddef.h>

#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <stdio.h>


/* used to create a linked list of free blocks within a pool */
struct free_list {
    struct free_list *next;
};

/* data structure to keep info about one pool */
struct kmalloc_pool {
    int max_size;
    int avail;
    struct free_list *head;
};

/* header placed before the addr returned by kmalloc. Used by kfree */
struct kmalloc_extra {
    struct kmalloc_pool *pool;
    size_t size;
};


#define NUM_POOLS 6

/*
 * allocation pools with block sizes: 32, 64, 128, 512, 1024, and 2048.
 * Any requested size greater than 2048 would default to using entire 4096 byte
 * pages. We don't need a pool larger than 2048
 */
static struct kmalloc_pool pools[NUM_POOLS] = {
    {32,   0, NULL},    /* max_size = 32; avail = 0; free_list = NULL */
    {64,   0, NULL},
    {128,  0, NULL},
    {512,  0, NULL},
    {1024, 0, NULL},
    {2048, 0, NULL}
};


/* increase the size of a pool by allocating a new virtual page (like brk) */
static void expand_pool(struct kmalloc_pool *pool)
{
    char *page = MMU_alloc_page();
    if (page == NULL) {
        return;
    }

    /* split the new page into allocation blocks then add blocks to free list */
    int num_blocks = PAGE_SIZE / pool->max_size;
    for (int i = 0; i < num_blocks; i++) {
        struct free_list *block = (struct free_list *)(page + i * pool->max_size);
        block->next = pool->head;
        pool->head = block;
    }

    pool->avail += num_blocks;
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t total_size = size + sizeof(struct kmalloc_extra);
    struct kmalloc_pool *best_pool = NULL;
    void *block = NULL;

    /* find the pool with the smallest allocation size suitable for request */
    for (int i = 0; i < NUM_POOLS; i++) {
        if (pools[i].max_size >= total_size) {
            best_pool = &pools[i];
            break;
        }
    }

    /* allocate from the best fitting pool */
    if (best_pool) {
        if (best_pool->avail == 0) {
            expand_pool(best_pool);
            if (best_pool->avail == 0) {
                return NULL;
            }
        }
        
        block = best_pool->head;
        best_pool->head = best_pool->head->next;
        best_pool->avail--;

    /* the requested size was bigger than 2048, just alloc pages directly */
    } else {
        size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
        block = MMU_alloc_pages(num_pages);
        if (block == NULL) {
            return NULL;
        }
    }

    /* insert the kmalloc_extra header then return allocation base addr */
    struct kmalloc_extra *header = (struct kmalloc_extra *)block;
    header->pool = best_pool;
    header->size = size;

    return (void *)(header + 1);
}

/* 
 * note that the freed blocks within a pool are marked as available but never
 * freed back to the OS. A pool can expand but it cannot shrink.
 */
void kfree(void *addr)
{
    if (addr == NULL) {
        return;
    }

    /* get header using pointer arithmetic from the allocation base addr */
    struct kmalloc_extra *header = (struct kmalloc_extra *)addr - 1;
    if (header->pool) {
        struct kmalloc_pool *pool = header->pool;
        struct free_list *node = (struct free_list *)header;

        node->next = pool->head;
        pool->head = node;
        pool->avail++;
    } else {
        /* just free the virtual pages associated with large allocations */
        size_t total_size = header->size + sizeof(struct kmalloc_extra);
        size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
        MMU_free_pages(header, num_pages);
    }
}