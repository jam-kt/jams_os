#include <limits.h>

#include <kernel/vga.h>
#include <string.h>
#include <stdio.h>
#include <kernel/ps2_kbd.h>
#include <kernel/interrupts.h>


void kernel_main() 
{
    vga_clear();
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
    
    ps2_init();
    keyboard_init();
    interrupts_init();

    while(1) {
        int ascii = keyboard_poll();
        if (ascii == -1) {
            continue;
        }

        printk("%c", (char)ascii);
    }
}