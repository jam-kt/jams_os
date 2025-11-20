#include <stdint-gcc.h>
#include <stddef.h>

#include <stdio.h>
#include <string.h>
#include <kernel/memory.h>
#include <kernel/interrupts.h>

#include "frames.h"
#include "vpages.h"


/* top level page table and one p3 table for the identity map */
static struct p4_entry ptable_p4[512] __attribute__((aligned(4096)));
static struct p3_entry ptable_p3_identity[512] __attribute__((aligned(4096)));

/* increasing vitrtual address "counter" */
static uint64_t kernel_va = VA_KHEAP_BASE;


static struct p1_entry *travel_pagetable(uint64_t va, int create);
static struct p1_entry *get_or_alloc_PT_entry(struct p2_entry *e2);
static struct p2_entry *get_or_alloc_PD_entry(struct p3_entry *e3);
static struct p3_entry *get_or_alloc_PDPT_entry(struct p4_entry *e4);
static void ISR14_PAGE_FAULT_HANDLER(int vector, int error_code, void *arg);


/* helpers to convert between 40 bit page frame number and 52 bit frame addr */
static inline uint64_t phys_to_pfn(uint64_t phys_addr) {
    return (phys_addr >> 12);
}

static inline uint64_t pfn_to_phys(uint64_t pfn) {
    return (pfn << 12);
}

/* 
 * given a p4_entry return the base addr of its p3 table. If it has no p3 table
 * allocate a new frame for it and return that base address
 */ 
static struct p3_entry *get_or_alloc_PDPT_entry(struct p4_entry *e4) {
    if (!e4->present) {
        /* alloc new frame if table not present (error check in MMU_pf_alloc)*/
        void *frame = MMU_pf_alloc();
        if (frame == INVALID_FRAME_ADDR) {
            return INVALID_FRAME_ADDR;
        }

        memset(frame, 0, PAGE_SIZE);

        e4->p3_addr     = phys_to_pfn((uint64_t)frame);
        e4->present     = 1;
        e4->rw          = 1;
        e4->us          = 0;
        e4->allocated   = 1;
    }

    uint64_t pdpt_phys = pfn_to_phys((uint64_t)e4->p3_addr);

    return (struct p3_entry *)(VA_PHYS_BASE + pdpt_phys);
}

/* 
 * given a p3_entry return the base addr of its p2 table. If it has no p2 table
 * allocate a new frame for it and return that base address
 */ 
static struct p2_entry *get_or_alloc_PD_entry(struct p3_entry *e3) {
    if (!e3->present) {
        void *frame = MMU_pf_alloc();
        if (frame == INVALID_FRAME_ADDR) {
            return INVALID_FRAME_ADDR;
        }

        memset(frame, 0, PAGE_SIZE);

        e3->p2_addr     = phys_to_pfn((uint64_t)frame);
        e3->present     = 1;
        e3->rw          = 1;
        e3->us          = 0;
        e3->big_page    = 0;
        e3->allocated   = 1;
    }

    uint64_t pd_phys = pfn_to_phys((uint64_t)e3->p2_addr);

    return (struct p2_entry *)(VA_PHYS_BASE + pd_phys);
}

/* 
 * given a p2_entry return the base addr of its p1 table. If it has no p1 table
 * allocate a new frame for it and return that base address
 */ 
static struct p1_entry *get_or_alloc_PT_entry(struct p2_entry *e2) {
    if (!e2->present) {
        void *frame = MMU_pf_alloc();
        if (frame == INVALID_FRAME_ADDR) {
            return INVALID_FRAME_ADDR;
        }
        memset(frame, 0, PAGE_SIZE);

        e2->p1_addr     = phys_to_pfn((uint64_t)frame);
        e2->present     = 1;
        e2->rw          = 1;
        e2->us          = 0;
        e2->big_page    = 0;
        e2->allocated   = 1;
    }

    uint64_t pt_phys = pfn_to_phys((uint64_t)e2->p1_addr);

    return (struct p1_entry *)(VA_PHYS_BASE + pt_phys);
}

/*
 * Will walk the virtual 4-level page table to return the p1_entry associated
 * with a given virtual address. if create = CREATE_MODE then the function
 * will allocate page table entries missing in the path to the virtual addr
 */
static struct p1_entry *travel_pagetable(uint64_t va, int create) {
    uint16_t idx4 = PML4_INDEX(va);
    uint16_t idx3 = PDP_INDEX(va);
    uint16_t idx2 = PD_INDEX(va);
    uint16_t idx1 = PT_INDEX(va);
    
    struct p4_entry *e4 = &ptable_p4[idx4];
    struct p3_entry *pdpt;
    struct p2_entry *pd;
    struct p1_entry *pt;

    /* walk from p4 to p3 */
    if (!e4->present) {
        if (create) {
            pdpt = get_or_alloc_PDPT_entry(e4);
            if (pdpt == INVALID_FRAME_ADDR) {
                printk("Failed to alloc P3 table\n");
                return INVALID_FRAME_ADDR;
            }
        } else {
            return INVALID_FRAME_ADDR;
        }
    } else {
        pdpt = get_or_alloc_PDPT_entry(e4);
    }

    /* p3 to p2 */
    struct p3_entry *e3 = &pdpt[idx3];
    if (e3->present && e3->big_page) {
        printk("PANIC: Attempted to map a 4k page inside a 1G huge page!\n");
        return INVALID_FRAME_ADDR;
    }
    
    if (!e3->present) {
        if (create) {
            pd = get_or_alloc_PD_entry(e3);
            if (pd == INVALID_FRAME_ADDR) {
                printk("Failed to alloc P2 table\n");
                return INVALID_FRAME_ADDR;
            }
        } else {
            return INVALID_FRAME_ADDR;
        }
    } else {
        pd = get_or_alloc_PD_entry(e3);
    }

    /* p2 to p1 */
    struct p2_entry *e2 = &pd[idx2];
    if (e2->present && e2->big_page) {
        printk("PANIC: Attempted to map a 4k page inside a 2M huge page!\n");
        return INVALID_FRAME_ADDR;
    }

    if (!e2->present) {
        if (create) {
            pt = get_or_alloc_PT_entry(e2);
            if (pt == INVALID_FRAME_ADDR) {
                printk("Failed to alloc P1 table\n");
                return INVALID_FRAME_ADDR;
            }
        } else {
            return INVALID_FRAME_ADDR;
        }
    } else {
        pt = get_or_alloc_PT_entry(e2);
    }
    
    /* the p1 entry for the given VA */
    return &pt[idx1];
}

/* creates an identity mapping for the first 512 GB */
static void make_identity_map(void) {
    memset(ptable_p4, 0, PAGE_SIZE);
    memset(ptable_p3_identity, 0, PAGE_SIZE);

    /* make each p3 entry map a 1 GB huge page */
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t phys1gb = (i << 30);
        ptable_p3_identity[i].present   = 1;
        ptable_p3_identity[i].rw        = 1;
        ptable_p3_identity[i].us        = 0;
        ptable_p3_identity[i].big_page  = 1;
        ptable_p3_identity[i].p2_addr   = phys_to_pfn(phys1gb);
        ptable_p3_identity[i].allocated = 1;
    }

    /* index 0 of the p4 table will hold the base addr to the identity map */
    ptable_p4[0].p3_addr    = phys_to_pfn((uint64_t)ptable_p3_identity);
    ptable_p4[0].present    = 1;
    ptable_p4[0].rw         = 1;
    ptable_p4[0].us         = 0;
    ptable_p4[0].allocated  = 1;

    /* load the new page table into cr3 reg */
    asm volatile("mov cr3, %0" : : "r" (&ptable_p4) : "memory");
}

/* reserve a virtual page using demand paging */
static int demand_page(uint64_t va) {
    struct p1_entry *p1 = travel_pagetable(va, CREATE_MODE);
    if (p1 == INVALID_FRAME_ADDR) {
        printk("demand_page: bad p1 entry given\n");
        return -1;
    }
    if (p1->demand || p1->present) {
        printk("demand_page: frame for given VA already allocated or reserved\n");
        return -1;
    }
    
    p1->demand = 1;
    p1->present = 0;
    p1->phys_addr = 0;
    
    return 0;
}

void *MMU_alloc_page(void) {
    uint64_t va = kernel_va;
    if (demand_page(va) < 0) {
        return NULL;
    }

    kernel_va += PAGE_SIZE;

    return (void *)va;
}

void *MMU_alloc_pages(int num) {
    uint64_t base = kernel_va;
    for (int i = 0; i < num; i++) {
        if (demand_page(base + i * PAGE_SIZE) < 0) {

            return NULL;
        }
    }

    kernel_va += (uint64_t)num * PAGE_SIZE;

    return (void *)base;
}

/* 
 * currently the frames allocated to store the page tables are not freed. 
 * the frames that are mapped to a VA and given out are freed.
 */
void MMU_free_page(void *vaddr) {
    /* get rid of inter page offset */
    uint64_t va = ((uint64_t)vaddr) & PAGE_MASK;
    struct p1_entry *p1 = travel_pagetable(va, WALK_MODE);

    if (p1 == INVALID_FRAME_ADDR) {
        printk("MMU_free_page: VA has no p1 entry? vaddr: %p\n", vaddr);
        return;
    }
    
    if (p1->present) {
        void *phys_frame = (void *)pfn_to_phys(p1->phys_addr);
        MMU_pf_free(phys_frame);
    }

    /* clear the p1 entry present bit */
    p1->present = 0;

    /* tell CPU that the translation for vaddr is no longer valid */
    asm volatile("invlpg [%0]" : : "r"(vaddr) : "memory");
}

void MMU_free_pages(void *vaddr, int num) {
    uint64_t base = ((uint64_t)vaddr) & PAGE_MASK;
    for (int i = 0; i < num; i++) {
        MMU_free_page((void *)(base + i * PAGE_SIZE));
    }
}

/* TLB should automatically flush after a page fault */
static void ISR14_PAGE_FAULT_HANDLER(int vector, int error_code, void *arg) {
    uint64_t fault_va, cr3;
    
    /* find the VA that caused the page fault */
    asm volatile("mov %0, cr2" : "=r" (fault_va));
    asm volatile("mov %0, cr3" : "=r" (cr3));

    struct p1_entry *p1 = travel_pagetable(fault_va, WALK_MODE);

    /* allocate a frame for a demanded virtual page that is not yet present */
    if ((p1 != INVALID_FRAME_ADDR) && p1->demand && !p1->present) {
        void *phys_frame = MMU_pf_alloc();
        if (phys_frame == INVALID_FRAME_ADDR) {
            printk("Page fault handler: no more physical frames\n");
            asm volatile("cli");
            asm volatile("hlt");
        }

        memset(phys_frame, 0, PAGE_SIZE);

        /* update the p1 entry to point to the frame */
        p1->phys_addr = phys_to_pfn((uint64_t)phys_frame);
        p1->present = 1;        /* frame is now present */
        p1->rw      = 1;
        p1->us      = 0;
        p1->demand  = 0;        
    } else {
        /* non-recoverable page fault */
        printk("Page Fault Handler: Unrecoverable page fault\n");
        printk("  - Faulting VA: %p\n", (void *)fault_va);
        printk("  - Error Code:  %d\n", error_code);
        printk("  - P1 Entry:    %p\n", p1);
        if(p1 != INVALID_FRAME_ADDR) {
             printk("  - P1 Present: %d, Demand: %d\n", p1->present, p1->demand);
        }

        asm volatile("cli");
        asm volatile("hlt");
    }
}

void MMU_init(void) {
    make_identity_map();
    kernel_va = VA_KHEAP_BASE;
    register_interrupt(14, 3, TYPE_TRAPGATE, ISR14_PAGE_FAULT_HANDLER, NULL);
}

void MMU_test(void) {
    printk("--- Running MMU tests ---\n");

    printk("Allocating single page");
    char *page1 = (char *)MMU_alloc_page();
    if (page1 == NULL) {
        printk("\tFAILED: MMU_alloc_page returned NULL\n");
        return;
    }
    printk("\tOK, VA=%p\n", page1);

    printk("Writing to page (good page fault)\n");
    page1[0] = 'H';
    page1[1] = 'i';
    page1[1000] = '!';
    printk("\tOK\n");

    printk("Reading back from page\n");
    if (page1[0] == 'H' && page1[1] == 'i' && page1[1000] == '!') {
        printk("\tOK\n");
    } else {
        printk("\tFAILED: read data differs from written data\n");
        return;
    }

    printk("Allocating 4 pages");
    int *pages = (int *)MMU_alloc_pages(4);
    if (pages == NULL) {
        printk("\tFAILED: MMU_alloc_pages returned NULL\n");
        return;
    }
    printk("\tOK, VA=%p\n", pages);
    
    printk("Writing to first and last page of allocation\n");
    pages[0] = 0xDEADBEEF;
    pages[(4 * PAGE_SIZE / sizeof(int)) - 1] = 0xCAFEBABE;
    printk("\tOK\n");

    printk("Reading data back from first and last allocation\n");
    if (pages[0] == 0xDEADBEEF && pages[(4 * PAGE_SIZE / sizeof(int)) - 1] == 0xCAFEBABE) {
        printk("\tOK\n");
    } else {
        printk("\tFAILED: read data differs from written data\n");
        return;
    }

    printk("Freeing pages\n");
    MMU_free_page(page1);
    MMU_free_pages(pages, 4);
    printk("\tOK\n");

    /* since we freed the page this should be unrecoverable */
    printk("\nTests passed\n");
    printk("\nTrying to purposely cause a fatal page fault...\n");
    page1[0] = '!'; 
    
    
    printk("\tFAILED: No kernel panic. whyyyyyyyyyyy\n");
}