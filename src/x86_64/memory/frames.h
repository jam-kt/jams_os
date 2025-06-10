#ifndef __FRAMES_H__
#define __FRAMES_H__

#include <stdint-gcc.h>


void *MMU_pf_alloc();
void MMU_pf_free(void *pf);


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/* regions describe entire ranges of addresses, should not be many of them */
#define MAX_REGIONS 64
#define INVALID_FRAME_ADDR ((void *)0xDEADBEEF)
#define ESTIMATE_FRAME_NUMBERS 34000    /* expected max number of frames */

/* keeps tracks of mmap regions */
struct mem_region {
    uint64_t addr;
    uint64_t size;
    int valid;
};

struct pf_header {
    struct pf_header *next; /* used for the freelist linked list             */
    uint64_t addr;          /* this field is only needed for the stress test */
};


#define TAG_TYPE_BASIC   4
#define TAG_TYPE_BIOS    5
#define TAG_TYPE_CMDLINE 1
#define TAG_TYPE_BOOTNAM 2
#define TAG_TYPE_MMAP    6
#define TAG_TYPE_ELF     9

struct mboot_fixed {        /* fixed header who's pointer is passed to kmain */
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed));

struct mboot_tag {          /* generic variable size tag header              */
    uint32_t type;
    uint32_t tag_size;
} __attribute__((packed));


#define REGION_OS_USE 1

struct mmap_entry {         /* a memory info entry                          */
    uint64_t addr;          /* starting address of region                   */
    uint64_t size;          /* length of region in bytes                    */
    uint32_t type;          /* type 1 regions are free for OS use           */
    uint32_t resv;
} __attribute__((packed));

struct mmap_tag {           /* contains info about regions of physical mem  */
    uint32_t type;
    uint32_t tag_size;
    uint32_t entry_size;    /* size of each memory info entry */
    uint32_t entry_ver;     /* version of each memory info entry, must be 0 */
} __attribute__((packed));



struct sect_head {
    uint32_t name;
    uint32_t type;
    uint64_t flag;
    uint64_t addr;          /* addr of the segment in memory */
    uint64_t disk;          /* offset of segment on disk     */
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;         /* addr alignment, powers of 2   */
    uint64_t fixed_size;    /* fixed entry size IFF section has fixed entries */
} __attribute__((packed));

struct elf_tag {            /* describes location and size of kernel in mem */
    uint32_t type;
    uint32_t tag_size;
    uint32_t num_sect;      /* number of section header entries             */
    uint32_t size_sect;     /* size of one section header entry in bytes    */
    uint32_t str_tbl;       /* index of section containing the string table */
} __attribute__((packed));


#endif