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
#include <kernel/block_dev.h>
#include <kernel/ata.h>
#include <kernel/mbr.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>
#include <kernel/md5.h>

void ls_recursive(struct inode *dir, int indent_level);
void test_fs_checksum(struct inode *root);


static void ata_test_thread(void *arg)
{
    printk("entering ATA driver test thread\n");

    block_dev *hdd = ata_probe(ATA_PRIMARY_IO, ATA_MASTER, 0, 
        "ATA PRIMARY MASTER", ATA_PRIMARY_ISR);

    if (!hdd) {
        printk("failed to detect a primary master\n");
    }

    /* begin test sequence */
    uint8_t *buf = kmalloc(512);
    if (!buf) {
        return;
    }

    /* fill with junk */
    memset(buf, 0xCC, 512);

    /* test read block 0 first 16 bytes */
    int ret = hdd->read_block(hdd, 0, buf);
    if (ret != 0) {
        printk("ata test failed to read block 0\n");
    } else {
        printk("\n--- BLOCK 0 DUMP ---\n");
        for (int i = 0; i < 16; i++) {
            printk("%x ", buf[i]);
        }

    }

    /* test block 32 */
    ret = hdd->read_block(hdd, 32, buf);
    if (ret != 0) {
        printk("ata test failed to read block 32\n");
    } else {
        printk("\n--- BLOCK 32 DUMP ---\n");
        for (int i = 0; i < 16; i++) {
            printk("%x ", buf[i]);
        }

    }

    /* test block 64 */
    ret = hdd->read_block(hdd, 64, buf);
    if (ret != 0) {
        printk("ata test failed to read block 64\n");
    } else {
        printk("\n--- BLOCK 64 DUMP ---\n");
        for (int i = 0; i < 16; i++) {
            printk("%x ", buf[i]);
        }
    }

    printk("\n\nATA driver test complete\n\n");

    /* begin testing for VFS stuff */
    MBR_init(hdd);
    ext2_init();

    /* MBR_init should register the ext2 FS as a block device, use that now */
    block_dev *part_dev = blk_find_partiton_fstype(EXT2_TYPE);
    if (!part_dev) {
        printk("error: no partitions found\n");
        kexit();
    }
    
    struct superblock *sb = fs_probe(part_dev);
    if (!sb) {
        kfree(hdd);
        kexit();
    }

    ls_recursive(sb->root_inode, 0);

    kfree(hdd);
    kfree(part_dev);
    kexit();
}


static void kbd_io_thread(void *arg)
{
    printk("entering kbd io thread\n");

    while (1) {
        int c = keyboard_getchar();
        if (c == -1) {
            continue;
        }
        printk("%c", (char)c);
    }
}

static void kernel_init(void *arg)
{
    printk("started kernel initialization thread\n");

    /* post multithreading initialization here */
    ps2_init();
    keyboard_init();

    /* keyboard */
    PROC_create_kthread(kbd_io_thread, NULL);

    /* block device / ATA */
    PROC_create_kthread(ata_test_thread, NULL);

    printk("ending kernel initialization thread\n");
    kexit();
}


/* kernel entry and idle thread, never block from this context */
void kernel_main(void *mboot_header) 
{
    /* pre-multitasking initialization */
    vga_clear();
    interrupts_init();
    serial_init();
    syscall_init();
    multitask_init();
    parse_mboot_tags(mboot_header);
    MMU_init();

    printk("pre-multitasking kernel initialization finished\n");

    PROC_create_kthread(kernel_init, NULL);

    /* idle loop */
    while (1) {
        PROC_run();             /* check for other runnable threads in sched */

        if (num_proc_runnable() <= 1) {
            asm volatile("hlt");    /* halts until an ISR unblocks a thread  */
        }
    }

    // print formats
    // printk("%c\n", 'a'); // should be "a"
    // printk("%c\n", 'Q'); // should be "Q"
    // printk("%c\n", 256 + '9'); // Should be "9"
    // printk("%s\n", "test string"); // "test string"
    // printk("foo%sbar\n", "blah"); // "fooblahbar"
    // printk("foo%%sbar\n"); // "foo%sbar"
    // printk("%d\n", INT_MIN); // "-2147483648"
    // printk("%d\n", INT_MAX); // "2147483647"
    // printk("%u\n", 0); // "0"
    // printk("%u\n", UINT_MAX); // "4294967295"
    // printk("%x\n", 0xDEADbeef); // "deadbeef"
    // printk("%p\n", (void*)UINTPTR_MAX); // "0xFFFFFFFFFFFFFFFF"
    // printk("%hd\n", 0x8000); // "-32768"
    // printk("%hd\n", 0x7FFF); // "32767"
    // printk("%hu\n", 0xFFFF); // "65535"
    // printk("%ld\n", LONG_MIN); // "-9223372036854775808"
    // printk("%ld\n", LONG_MAX); // "9223372036854775807"
    // printk("%lu\n", ULONG_MAX); // "18446744073709551615"
    // printk("%qd\n", (long long int)LONG_MIN); // "-9223372036854775808"
    // printk("%qd\n", (long long int)LONG_MAX); // "9223372036854775807"
    // printk("%qu\n", (long long unsigned int)ULONG_MAX); // "18446744073709551615"


    // page frames testing for demo
    // frames_sequence_test();
    // frames_stress_test();

    // MMU_test();

    /* kmalloc tests */
    // size_t test_size = 48;
    // void *p = kmalloc(test_size);
    // if (!p) {
    //     printk("kmalloc failed (returned NULL)\n");
    // }

    // /* write then read a pattern */
    // memset(p, 0x5A, test_size);
    // for (size_t i = 0; i < test_size; i++) {
    //     if (((uint8_t*)p)[i] != 0x5A) {
    //         printk("kmalloc memory corrupt\n");
    //     }
    // }

    // /* free and alloc again to test reuse */
    // kfree(p);
    // void *q = kmalloc(test_size);
    // if (!q) {
    //     printk("kmalloc failed on second alloc\n");
    // }
    // if (q != p) {
    //     printk("kmalloc returned a different address on reuse\n");
    // }

    // kfree(q);

    // void *big_block = kmalloc(4096);
    // if (!big_block) {
    //     printk("big block not allocated\n");
    // }

    // memset(big_block, 0xAA, 4096);
    // for (size_t i = 0; i < 4096; i++) {
    //     if (((uint8_t*)big_block)[i] != 0xAA) {
    //         printk("kmalloc memory corrupt\n");
    //     }
    // }

    // kfree(big_block);


    // printk("test done\n");

  

    // PROC_run();
    // PROC_create_kthread(worker_c, (void *)1);
    // PROC_create_kthread(worker_c, (void *)2);
    // PROC_create_kthread(worker_c, (void *)3);
    
    // for (int i = 0; i < 10; i++) {
    //     PROC_run();
    // }
}

// static void test_worker_c(void *arg)
// {
//     int id = (int)(uintptr_t)arg;
//     int count = 0;
//     for (int i = 0; i < 10; i++) {
//         printk("[C%d] count=%d\n", id, count++);
//         //dump_stack("[C]", 3);
//         yield();
//     }
// }


/* helpers for EXT32 read demonstration */
int recursive_print_cb(const char *name, struct inode *inode, void *p) {
    static const char hex[] = "0123456789abcdef";   // yes this is dumb
    int indent = *(int *)p;
    
    /* print indentation */
    for (int i = 0; i < indent; i++) printk("  ");
    
    /* print file name */
    printk("- %s", name);

    /* Check if it is a directory */
    if (inode->mode & S_IFDIR) {
        printk("/");
        printk("\n");

        /* Recurse: ignore "." and ".." to avoid infinite loops */
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            ls_recursive(inode, indent + 1);
        }
    } 
    /* Check if it is a regular file */
    else if (inode->mode & S_IFREG) {
        /* Open the file to compute checksum */
        struct file *f = inode->open(inode);
        
        if (f) {
            uint8_t digest[16];
            
            /* Compute hash of content only */
            md5File(f, digest);
            
            printk(" MD5: ");
            for(int i = 0; i < 16; ++i){
                char hi = hex[(digest[i] >> 4) & 0xF];
                char lo = hex[digest[i] & 0xF];
                printk("%c%c", hi, lo);
            }
            
            f->close(f);
        } else {
            printk(" [Error opening file]");
        }
        
        printk("\n");
    }

    return 0;
}


void ls_recursive(struct inode *dir, int indent_level) {
    /* pass the indent level as the 'void *p' user data */
    dir->readdir(dir, recursive_print_cb, &indent_level);
}