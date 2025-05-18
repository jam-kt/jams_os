#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <kernel/interrupts.h>

#include "idt_tss.h"
#include "pic.h"


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
    PIC_init();

    /* disable the IRQs */
    for (int i = 0; i < 15; i++) {
        IRQ_set_mask(i);
    }

    // IRQ_clear_mask(1);  /* enable keyboard irq */
    
    STI();
}

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
 * GENERIC C INTERRUPT SERVICE ROUTINE
 ******************************************************************************/
void c_isr_handler(int vector, unsigned err)
{
    // int CONTINUE = 0;
    // while(!CONTINUE);

    if (isr_table[vector].handler != NULL) {
        isr_table[vector].handler(vector, err, isr_table[vector].arg);
    } else {
        printk("Unhandled Interrupt Vector: %d\n", vector);
        printk("Interrupt pushed error code: %d\n", (int)err);
        while(1);
    }
}