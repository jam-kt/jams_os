#ifndef __MEMORY_H__
#define __MEMORY_H__


void parse_mboot_tags(void *mboot_header);
void frames_sequence_test();
void frames_stress_test();

void MMU_init(void);
void *MMU_alloc_page(void);
void *MMU_alloc_pages(int num);
void  MMU_free_page(void *vaddr);
void  MMU_free_pages(void *vaddr, int num);
void MMU_test(void);


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifndef INVALID_FRAME_ADDR
#define INVALID_FRAME_ADDR ((void *)0xDEADBEEF)
#endif

#endif