#ifndef __VPAGES_H__
#define __VPAGES_H__

#include <stdint-gcc.h>

#define CREATE_MODE 1
#define WALK_MODE   0

/* describes the virtual addr layout  */
#define VA_PHYS_BASE    0x000000000000   /* slot 0  identity map */
#define VA_KHEAP_BASE   0x010000000000   /* slot 1  kernel heap */
#define VA_RESERVED     0x020000000000    
#define VA_KSTACK_BASE  0x0F0000000000   /* slot 15 kernel stacks */
#define VA_USER_BASE    0x100000000000   /* slot 16 user space */

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