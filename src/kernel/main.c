#include <limits.h>

#include <string.h>
#include <stdio.h>

#include <kernel/vga.h>
#include <kernel/ps2_kbd.h>
#include <kernel/interrupts.h>
#include <kernel/serial_out.h>
#include <kernel/frames.h>


void kernel_main(void *mboot_header) 
{
    vga_clear();

    interrupts_init();
    serial_init();
    ps2_init();
    keyboard_init();
    parse_mboot_tags(mboot_header);

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


    frames_sequence_test();
    frames_stress_test();


    /* note that this is still "polling" even though the ISR is getting keyboard input */
    while(1) {
        int ascii = keyboard_getchar();
        if (ascii == -1) {
            continue;
        }

        printk("%c", (char)ascii);
    }
}