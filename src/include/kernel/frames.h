#ifndef __FRAMES_H__
#define __FRAMES_H__

void parse_mboot_tags(void *mboot_header);
void *MMU_pf_alloc();
void MMU_pf_free(void *pf);
void frames_sequence_test();
void frames_stress_test();

/* regions describe entire ranges of addresses, should not be many of them */
#define MAX_REGIONS 64
#define PAGE_SIZE 4096
#define INVALID_FRAME_ADDR ((void *)0xDEADBEEF)

/* keeps tracks of mmap regions */
struct mem_region {
    uint64_t addr;
    uint64_t size;
    int valid;
};

struct pf_header {
    struct pf_header *next;
};


#endif
