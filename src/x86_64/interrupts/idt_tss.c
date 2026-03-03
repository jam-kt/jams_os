#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/multitask.h>

#include "idt_tss.h"


extern void isr0(void);

static struct idt_descriptor idt_desc;
static struct idt_entry idt[NUM_ISRS];

extern uint8_t gdt64[];   /* the GDT as a byte array from boot.asm */
static struct tss_descriptor *tss_desc;
static struct tss_table tss;

/* TSS stacks aligned to 16B */
static uint8_t ist1_stack[TSS_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ist2_stack[TSS_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t ist3_stack[TSS_STACK_SIZE] __attribute__((aligned(16)));


/* IDT initialization. Load and have all IDT use IST = 0 and interrupt gate*/
void idt_init()
{
    idt_desc.limit = sizeof(idt) - 1;
    idt_desc.base = (uintptr_t)&idt;
    asm volatile("lidt %0" : : "m"(idt_desc));

    for (int vector = 0; vector < NUM_ISRS; vector++) {
        add_idt_entry(vector, 0, TYPE_INTRGATE, 0);
    }
}

/* add or modify an entry to the IDT */
void add_idt_entry(uint8_t vector, uint8_t IST, uint8_t type, uint8_t DPL)
{
    /* get addr of stub using first stub addr and pointer arithmetic */
    uintptr_t addr = (uintptr_t)&isr0 + (vector * STUB_SIZE);
    idt[vector].offset1 = (uint16_t)(addr & 0xFFFF);
    idt[vector].select  = KERNEL_CS & ~0x3;     /* ensure selector RPL = 0 */
    idt[vector].IST     = IST;
    idt[vector].type    = type;
    idt[vector].DPL     = DPL & 0x3;
    idt[vector].pres    = 1;
    idt[vector].offset2 = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset3 = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
}

/* TSS initialization (call after idt_init) */
void tss_init()
{
    /* set offset to the io bitmap (unused) */
    tss.io_map_base = sizeof(tss);

    /* set ist1 - ist3 pointers to top of their respective stacks */
    tss.ist1 = (uint64_t)(ist1_stack + TSS_STACK_SIZE);
    tss.ist2 = (uint64_t)(ist2_stack + TSS_STACK_SIZE);
    tss.ist3 = (uint64_t)(ist3_stack + TSS_STACK_SIZE);

    /* get the address of the TSS using the gdt64 byte array */
    tss_desc = (struct tss_descriptor *)(gdt64 + TSS_DESC_CS);

    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    tss_desc->limit1 = (uint16_t)(limit & 0xFFFF);
    tss_desc->base1  = (uint16_t)(base & 0xFFFF);
    tss_desc->base2  = (uint8_t)((base >> 16) & 0xFF);

    tss_desc->type   = TSS_TYPE;
    tss_desc->zero   = 0;
    tss_desc->DPL    = 0;
    tss_desc->pres   = 1;

    tss_desc->limit2 = (uint8_t)((limit >> 16) & 0x0F);
    tss_desc->AVL    = 0;
    tss_desc->IGN    = 0;
    tss_desc->gran   = 0;
    
    tss_desc->base3  = (uint8_t)((base >> 24) & 0xFF);
    tss_desc->base4  = (uint32_t)(base >> 32);
    tss_desc->reserved = 0;

    asm volatile ("ltr %0" : : "r"((uint16_t)TSS_DESC_CS));
}

/* 
 * used in interrupt_asm.asm to handle user space context swapping tricks.
 * We update the TSS to ensure all user procs use their unique kernel thread
 * while we update cr3 to ensure each user proc has a unique address space
 */
void switch_mmu_and_tss(proc next)
{
    if (!next) {
        return;
    }

    /* update TSS RSP0 to use the proc's unique kernel stack, RSP0 is only used
     * when the CPU changes permission levels and IST = 0
     */
    if (next->kstack) {
        uintptr_t kstack_top = (uintptr_t)next->kstack + DEFAULT_STACK_BYTES;
        tss.rsp0 = (uint64_t)kstack_top;
    }

    /* if the next proc is a user proc, update CR3 reg to point to the proc's 
     * unique page table. Note that kernel threads will have their cr3 field = 0 
     * so the check will fail and we will keep the same ptr in cr3
     */
    if (next->cr3) {
        asm volatile("mov cr3, %0" : : "r" (next->cr3) : "memory");
    } 

}