#ifndef __VPAGES_H__
#define __VPAGES_H__

#include <stdint-gcc.h>

#define CREATE_MODE 1
#define WALK_MODE   0

/* 12 bits to index the actual page, then 9 bits to index entries per level */
/* masks / helpers */
#define PAGE_MASK          (~(PAGE_SIZE - 1ULL))
#define PML4_INDEX(va)     (((uint64_t)(va) >> 39) & 0x1FFULL)
#define PDP_INDEX(va)      (((uint64_t)(va) >> 30) & 0x1FFULL)
#define PD_INDEX(va)       (((uint64_t)(va) >> 21) & 0x1FFULL)
#define PT_INDEX(va)       (((uint64_t)(va) >> 12) & 0x1FFULL)

/* 
 * entries to the different page table levels (4 lvl page ta)
 * Ex. a p3_entry is an entry in the "page directory pointer table"
 */
struct p4_entry {
    uint64_t present:1;
    uint64_t rw:1;
    uint64_t us:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t a:1;
    uint64_t ign:1;
    uint64_t mbz:2;
    /* flag to indicate if entry is managed by the allocator */
    uint64_t allocated:1;
    uint64_t avl:2;
    uint64_t p3_addr:40;
    uint64_t available:11;
    uint64_t nx:1;
} __attribute__((packed));

struct p3_entry {
    uint64_t present:1;
    uint64_t rw:1;
    uint64_t us:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t a:1;
    uint64_t ign:1;
    uint64_t big_page:1;
    uint64_t mbz:1;
    /* flag to indicate if entry is managed by the allocator */
    uint64_t allocated:1;
    uint64_t avl:2;
    uint64_t p2_addr:40;
    uint64_t available:11;
    uint64_t nx:1;
} __attribute__((packed));

struct p2_entry {
    uint64_t present:1;
    uint64_t rw:1;
    uint64_t us:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t a:1;
    uint64_t ign1:1;
    uint64_t big_page:1;
    uint64_t ign2:1;
    /* flag to indicate if entry is managed by the allocator */
    uint64_t allocated:1;
    uint64_t avl:2;
    uint64_t p1_addr:40;
    uint64_t available:11;
    uint64_t nx:1;
} __attribute__((packed));

struct p1_entry {
    uint64_t present:1;
    uint64_t rw:1;
    uint64_t us:1;
    uint64_t pwt:1;
    uint64_t pcd:1;
    uint64_t a:1;
    uint64_t d:1;
    uint64_t pat:1;
    uint64_t g:1;
    uint64_t demand:1;          /* flag for demand-paged entry */
    uint64_t avl:2;
    uint64_t phys_addr:40;
    uint64_t available:11;
    uint64_t nx:1;
} __attribute__((packed));

#endif