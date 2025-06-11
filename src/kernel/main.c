#include <limits.h>

#include <string.h>
#include <stdio.h>

#include <kernel/vga.h>
#include <kernel/ps2_kbd.h>
#include <kernel/interrupts.h>
#include <kernel/serial_out.h>
#include <kernel/memory.h>
#include <kernel/kmalloc.h>


void kernel_main(void *mboot_header) 
{
    vga_clear();

    interrupts_init();
    serial_init();
    ps2_init();
    keyboard_init();
    parse_mboot_tags(mboot_header);
    MMU_init();

    // int stop = 1;
    // while(stop);

    printk("%c\n", 'a'); // should be "a"
    printk("%c\n", 'Q'); // should be "Q"
    printk("%c\n", 256 + '9'); // Should be "9"
    printk("%s\n", "test string"); // "test string"
    printk("foo%sbar\n", "blah"); // "fooblahbar"
    printk("foo%%sbar\n"); // "foo%sbar"
    printk("%d\n", INT_MIN); // "-2147483648"
    printk("%d\n", INT_MAX); // "2147483647"
    printk("%u\n", 0); // "0"
    printk("%u\n", UINT_MAX); // "4294967295"
    printk("%x\n", 0xDEADbeef); // "deadbeef"
    printk("%p\n", (void*)UINTPTR_MAX); // "0xFFFFFFFFFFFFFFFF"
    printk("%hd\n", 0x8000); // "-32768"
    printk("%hd\n", 0x7FFF); // "32767"
    printk("%hu\n", 0xFFFF); // "65535"
    printk("%ld\n", LONG_MIN); // "-9223372036854775808"
    printk("%ld\n", LONG_MAX); // "9223372036854775807"
    printk("%lu\n", ULONG_MAX); // "18446744073709551615"
    printk("%qd\n", (long long int)LONG_MIN); // "-9223372036854775808"
    printk("%qd\n", (long long int)LONG_MAX); // "9223372036854775807"
    printk("%qu\n", (long long unsigned int)ULONG_MAX); // "18446744073709551615"


    // page frames testing for demo
    // frames_sequence_test();
    // frames_stress_test();

    //MMU_test();

    /* kmalloc tests */
    size_t test_size = 64;
    void *p = kmalloc(test_size);
    if (!p) {
        printk("kmalloc failed (returned NULL)\n");
    }

    /* write then read a pattern */
    memset(p, 0x5A, test_size);
    for (size_t i = 0; i < test_size; i++) {
        if (((uint8_t*)p)[i] != 0x5A) {
            printk("kmalloc memory corrupt\n");
        }
    }

    /* free and alloc again to test reuse */
    kfree(p);
    void *q = kmalloc(test_size);
    if (!q) {
        printk("kmalloc failed on second alloc\n");
    }
    if (q != p) {
        printk("kmalloc returned a different address on reuse\n");
    }

    kfree(q);
    printk("test done\n");
    

    /* note that this is still "polling" even though the ISR is getting keyboard input */
    while(1) {
        int ascii = keyboard_getchar();
        if (ascii == -1) {
            continue;
        }

        printk("%c", (char)ascii);
    }
}