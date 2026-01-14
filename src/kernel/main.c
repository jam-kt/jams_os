#include <limits.h>
#include <stdint-gcc.h>

#include <string.h>
#include <stdio.h>

#include <kernel/vga.h>
#include <kernel/ps2_kbd.h>
#include <kernel/interrupts.h>
#include <kernel/serial_out.h>
#include <kernel/memory.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/multitask.h>


// static void dump_stack(const char *tag, int count)
// {
//     uint64_t *sp;
//     asm volatile ("mov %%rsp, %0" : "=r"(sp));
//     printk("%s RSP=%p\n", tag, sp);
//     for (int i = 0; i < count; i++) {
//         printk("  [%d] %p\n", i, (void *)sp[i]);
//     }
// }

static void worker_c(void *arg)
{
    int id = (int)(uintptr_t)arg;
    int count = 0;
    for (int i = 0; i < 10; i++) {
        printk("[C%d] count=%d\n", id, count++);
        //dump_stack("[C]", 3);
        yield();
    }
}

static void kbd_io_thread(void *arg)
{
    printk("made into kbd thread\n");

    (void)arg;

    while (1) {
        int c = keyboard_getchar();
        if (c == -1) {
            continue;
        }
        printk("%c\n", (char)c);
    }
}

static void kernel_thread(void *arg)
{
    printk("in the kernel thread\n");

    while (1) {
        yield();
    }
}

void kernel_main(void *mboot_header) 
{
    vga_clear();
    interrupts_init();
    serial_init();
    syscall_init();
    multitask_init();
    ps2_init();
    keyboard_init();
    parse_mboot_tags(mboot_header);
    MMU_init();

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

    // MMU_test();

    /* kmalloc tests */
    size_t test_size = 48;
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

    void *big_block = kmalloc(4096);
    if (!big_block) {
        printk("big block not allocated\n");
    }

    memset(big_block, 0xAA, 4096);
    for (size_t i = 0; i < 4096; i++) {
        if (((uint8_t*)big_block)[i] != 0xAA) {
            printk("kmalloc memory corrupt\n");
        }
    }

    kfree(big_block);


    printk("test done\n");

  

    PROC_run();
    PROC_create_kthread(worker_c, (void *)1);
    PROC_create_kthread(worker_c, (void *)2);
    PROC_create_kthread(worker_c, (void *)3);
    
    for (int i = 0; i < 10; i++) {
        PROC_run();
    }

    printk("Done with test loop\n");

    int stop = 1;
    while(stop);

    PROC_create_kthread(kernel_thread, NULL);

    PROC_create_kthread(kbd_io_thread, NULL);
    printk("made keyboard thread\n");


    while (1) {
        PROC_run();
    }
}