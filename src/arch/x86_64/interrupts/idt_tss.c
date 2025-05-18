#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>

#include "idt_tss.h"


extern void isr0(void);

struct idt_ptr idt_descriptor;
struct idt_entry idt[NUM_ISRS];


/* 
 * Add entries for every interrupt vector. Default to using the current stack 
 * and the interrupt gate type.
 */
void idt_init()
{
    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base = (uintptr_t)&idt;
    asm volatile("lidt %0" : : "m"(idt_descriptor));

    for (int vector = 0; vector < NUM_ISRS; vector++) {
        add_idt_entry(vector, 0, TYPE_INTRGATE);
    }
}

/* add or modify an entry to the IDT */
void add_idt_entry(uint8_t vector, uint8_t IST, uint8_t type)
{
    /* get addr of stub using first stub addr and pointer arithmetic */
    uintptr_t addr = (uintptr_t)&isr0 + (vector * STUB_SIZE);
    idt[vector].offset1 = (uint16_t)(addr & 0xFFFF);
    idt[vector].select  = KERNEL_CS;
    idt[vector].IST     = IST;
    idt[vector].type    = type;
    idt[vector].DPL     = 0;
    idt[vector].pres    = 1;
    idt[vector].offset2 = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset3 = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
}
