#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>

#include "idt_tss.h"
#include "pic.h"


void ISR8_double_fault(int vector, int err, void *rsp);


/* Table of C ISR function pointers. Indexed using the interrupt vector num */
struct isr_c_entry isr_table[NUM_ISRS];


/*******************************************************************************
 * ASM HELPERS
 ******************************************************************************/
/* disable interrupts */
inline void CLI()
{
    asm ( "CLI");
}

/* enable interrupts */
inline void STI()
{
    asm ( "STI");
}

/* returns 0 for false or a positive int for true */
inline int are_interrupts_enabled()
{
    unsigned long flags;
    asm volatile ( "pushf\n\t"
                   "pop %0"
                   : "=g"(flags) );
    return flags & (1 << 9);
}


/*******************************************************************************
 * INTERRUPTS API
 ******************************************************************************/
void interrupts_init()
{
    CLI();

    idt_init();
    tss_init();
    PIC_init();

    /* disable the IRQs */
    for (int i = 0; i < 15; i++) {
        IRQ_set_mask(i);
    }

    // IRQ_clear_mask(1);  /* enable keyboard irq */
    
    /* register basic double fault handler */
    register_interrupt(8, 1, TYPE_INTRGATE, ISR8_double_fault, NULL);

    STI();
}

/*
 * - vector : interrupt vector number
 * - IST    : 0 for current stack or 1-7 for an IST stack
 * - type   : interrupt or trap gate. See macros in interrupts.h
 * - handler: callback function
 * - arg    : ISR specific context struct or if NULL defaults to the rsp
 */
void register_interrupt(int vector, uint8_t IST, uint8_t type, isr_t handler, void *arg)
{
    CLI();

    if ((vector >= NUM_ISRS) || (vector < 0)) {
        printk("Tried to register an interrupt on a bad vector: %d", vector);
        return;
    }

    add_idt_entry(vector, IST, type);
    
    isr_table[vector].handler = handler;
    isr_table[vector].arg = arg;

    STI();
}


/*******************************************************************************
 * C INTERRUPT SERVICE ROUTINES (generic handler and critical isrs)
 ******************************************************************************/
void c_isr_handler(int vector, int err, void *rsp)
{
    // int CONTINUE = 0;
    // while(!CONTINUE);

    /* dispatch to the ISR for a given vector or default to printing an error */
    if (isr_table[vector].handler != NULL) {
        /* if the registered ISR has a NULL arg field then pass rsp instead */
        if (isr_table[vector].arg != NULL) {
            isr_table[vector].handler(vector, err, isr_table[vector].arg);
        } else {
            isr_table[vector].handler(vector, err, rsp);
        }
    } else {
        printk("Unhandled Interrupt Vector: %d\n", vector);
        printk("Interrupt pushed error code: %d (%x)\n", (int)err, (int)err);
        while(1);
    }
}

/* double fault handler, use IST 1 */
void ISR8_double_fault(int vector, int err, void *rsp)
{
    /* for debug but might get rid of this */
    struct {
        uint64_t rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11, rip, cs, rflags;
    } *regs = rsp;

    printk("Double fault!\n");
    if (err != 0) {
        printk("Expected a 0 error code for #DF, got: %d\n", (int)err);
    }

    printk("RIP = %p\n", (void *)regs->rip);

    printk("halting...\n");
    __asm__("hlt");
}