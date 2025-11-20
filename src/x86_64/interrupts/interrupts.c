#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>

#include "idt_tss.h"
#include "pic.h"


void ISR8_double_fault(int vector, int err, void *rsp);
void ISR13_general_protection(int vector, int err, void *rsp);


/* Table of C ISR function pointers. Indexed using the interrupt vector num */
static struct isr_c_entry isr_table[NUM_ISRS];


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

    STI();

    /* TODO: move registering handlers to a new file for each specific ISR ? */
    /* register basic double fault handler */
    register_interrupt(8, 1, TYPE_INTRGATE, ISR8_double_fault, NULL);
    /* basic general protection fault handler */
    register_interrupt(13, 2, TYPE_INTRGATE, ISR13_general_protection, NULL);
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

    if ((vector >= NUM_ISRS) || (vector < 0) || isr_table[vector].handler) {
        printk("Tried to register an interrupt on a bad vector: %d", vector);
        return;
    }

    add_idt_entry(vector, IST, type);
    
    isr_table[vector].handler = handler;
    isr_table[vector].arg = arg;

    /* enable IRQ associated with the interrupt vector if any */
    if ((vector >= PIC1_VECTOR) && (vector < PIC2_VECTOR + 8)) {
        IRQ_clear_mask(vector - PIC1_VECTOR);
    }

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
        printk("halting...\n");
        __asm__("hlt");
    }

    IRQ_end_of_interrupt(vector);   /* send EOI for IRQ vectors */
}

/* double fault handler, use IST 1 */
void ISR8_double_fault(int vector, int err, void *rsp)
{

    printk("!! Double Fault !!\n");
    if (err != 0) {
        printk("Expected a 0 error code for #DF, got: %d\n", (int)err);
    }

    printk("halting...\n");
    __asm__("hlt");
}

/* general protection fault handler, use IST 2 */
void ISR13_general_protection(int vector, int err, void *rsp)
{
    printk("!! General Protection Fault !!\n");
    printk("Got error code: %d (%x)\n", (int)err, (int)err);
}