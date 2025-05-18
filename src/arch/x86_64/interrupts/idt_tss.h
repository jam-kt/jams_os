#ifndef __IDT_TSS_H__
#define __IDT_TSS_H__

#include <stddef.h>
#include <stdint-gcc.h>


void idt_init();
void add_idt_entry(uint8_t vector, uint8_t IST, uint8_t type);


#define STUB_SIZE 9       /* each ASM ISR stub is 9 bytes */

/* 
 * segment selector for dest code segment. Our gdt64.code section is at offset 1
 * which is equal to 8 bytes 
 */
#define KERNEL_CS 0x08

/* 
 * IDT entry (Interrupt Gate Descriptor)
 * A 16-byte entry in the Interrupt Descriptor Table
 */
struct idt_entry {
  uint16_t offset1;   /* forms 64-bit function ptr to the ISR               */
  uint16_t select;    /* GDT selector (should be kernel's code segment)     */

  uint16_t IST:3;     /* interrupt stack table index (0 to stay, else 1-7 ) */
  uint16_t IGN1:5;    /* reserved         */
  uint16_t type:4;    /* type             */
  uint16_t zero:1;    /* always zero      */
  uint16_t DPL:2;     /* protection level */
  uint16_t pres:1;    /* present          */

  uint16_t offset2;
  uint32_t offset3;
  uint32_t IGN2;      /* reserved         */
} __attribute__((packed));

/* load this struct using lidt instruction */
struct idt_ptr {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));

#endif