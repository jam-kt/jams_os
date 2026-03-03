#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint-gcc.h>

void parse_mboot_tags(void *mboot_header);
void frames_sequence_test();
void frames_stress_test();

void MMU_init(void);
void *MMU_alloc_page(void);
void *MMU_alloc_pages(int num);
void *MMU_alloc_at(uint64_t vaddr, uint64_t size);
uint64_t MMU_create_user_p4(void);
void MMU_free_page(void *vaddr);
void MMU_free_pages(void *vaddr, int num);
void MMU_test(void);


#ifndef PAGE_SIZE
#define PAGE_SIZE 4096ULL
#endif

#ifndef INVALID_FRAME_ADDR
#define INVALID_FRAME_ADDR ((void *)0xDEADBEEF)
#endif

/* describes the virtual addr layout  */
#define VA_PHYS_BASE    0x000000000000  /* slot 0  identity map     */
#define VA_KHEAP_BASE   0x010000000000  /* slot 1  kernel heap      */
#define VA_RESERVED     0x020000000000    
#define VA_KSTACK_BASE  0x0F0000000000  /* slot 15 kernel stacks    */
#define VA_USER_BASE    0x100000000000  /* slot 32 user space       */
#define VA_USTACK_BASE  0x110000000000  /* slot 33 user space stack */

#endif