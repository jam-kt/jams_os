#ifndef __VPAGES_H__
#define __VPAGES_H__


/* -------- virtual-address layout (one PML4 entry = 0x1000'0000'0000) ----- */
#define PML4_SIZE          0x100000000000   /* 512 GiB */
#define VA_PHYS_BASE       0x000000000000   /* slot 0  identity map */
#define VA_KHEAP_BASE      0x001000000000   /* slot 1  kernel heap */
#define VA_KSTACK_BASE     0x00F000000000   /* slot 15 kernel stacks */
#define VA_USER_BASE       0x010000000000   /* slot 16 user space */

/* demand-paging flag: we steal bit 9 (“available to software”) */
#define PTE_DEMAND         (1ULL << 9)

/* masks / helpers */
#define PAGE_SIZE          4096
#define PAGE_MASK          (~(PAGE_SIZE-1))
#define PML4_INDEX(va)     (((va) >> 39) & 0x1FF)
#define PDP_INDEX(va)      (((va) >> 30) & 0x1FF)
#define PD_INDEX(va)       (((va) >> 21) & 0x1FF)
#define PT_INDEX(va)       (((va) >> 12) & 0x1FF)


struct p4_entry {
    uint16_t present:1;
    uint16_t rw:1;
    uint16_t us:1;
    uint16_t pwt:1;
    uint16_t pcd:1;
    uint16_t a:1;
    uint16_t ign:1;
    uint16_t mbz:2;
    uint16_t allocated:1;
    uint16_t avl:2;
    uint16_t p3_addr_low:4;
    uint32_t p3_addr_mid;
    uint16_t p3_addr_high:4;
    uint16_t available:11;
    uint16_t nx:1;
} __attribute__((packed));

struct p3_entry {
    uint16_t present:1;
    uint16_t rw:1;
    uint16_t us:1;
    uint16_t pwt:1;
    uint16_t pcd:1;
    uint16_t a:1;
    uint16_t ign:1;
    uint16_t big_page:1;
    uint16_t mbz:1;
    uint16_t allocated:1;
    uint16_t avl:2;
    uint16_t p2_addr_low:4;
    uint32_t p2_addr_mid;
    uint16_t p2_addr_high:4;
    uint16_t available:11;
    uint16_t nx:1;
} __attribute__((packed));

struct p2_entry {
    uint16_t present:1;
    uint16_t rw:1;
    uint16_t us:1;
    uint16_t pwt:1;
    uint16_t pcd:1;
    uint16_t a:1;
    uint16_t ign1:1;
    uint16_t big_page:1;
    uint16_t ign2:1;
    uint16_t allocated:1;
    uint16_t avl:2;
    uint16_t p1_addr_low:4;
    uint32_t p1_addr_mid;
    uint16_t p1_addr_high:4;
    uint16_t available:11;
    uint16_t nx:1;
} __attribute__((packed));

struct p1_entry {
    uint16_t present:1;
    uint16_t rw:1;
    uint16_t us:1;
    uint16_t pwt:1;
    uint16_t pcd:1;
    uint16_t a:1;
    uint16_t d:1;
    uint16_t pat:1;
    uint16_t g:1;
    uint16_t allocated:1;
    uint16_t avl:2;
    uint16_t phys_addr_low:4;
    uint32_t phys_addr_mid;
    uint16_t phys_addr_high:4;
    uint16_t available:11;
    uint16_t nx:1;
}__attribute__((packed));

#endif