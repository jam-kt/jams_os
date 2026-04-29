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
#include <kernel/elf64.h>
#include <kernel/pit.h>

void ls_recursive(struct inode *dir, int indent_level);
void test_fs_checksum(struct inode *root);


static void ata_test_thread(void *arg)
{
    printk("entering ATA driver test thread\n");

    block_dev *hdd = ata_probe(ATA_PRIMARY_IO, ATA_MASTER, 0, 
        "ATA PRIMARY MASTER", ATA_PRIMARY_ISR);

    if (!hdd) {
        printk("failed to detect a primary master\n");
        kexit();
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
        kexit();
    }

    ls_recursive(sb->root_inode, 0);
    PROC_set_root(sb->root_inode);

    /* ELF loading */
    elf_load(sb->root_inode, "init.elf");

    /* Block devices are globally registered and expected to remain valid. */
    kexit();
}


static void kernel_init(void *arg)
{
    printk("started kernel initialization thread\n");

    /* post multithreading initialization here */
    ps2_init();
    keyboard_init();

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
    parse_mboot_tags(mboot_header);
    MMU_init();
    multitask_init();
    PROC_run();
    pit_init();

    printk("pre-multitasking kernel initialization finished\n");

    PROC_create_kthread(kernel_init, NULL);

    /* idle loop */
    while (1) {
        CLI();
        if (num_proc_runnable() == 0) {
            asm volatile("sti");    /* re-enables interrupts AFTER halting */
            asm volatile("hlt");    /* halts until an ISR unblocks a thread  */
        } else {
            STI();
            /* give up rest of quantum */
            yield();
        }
    }
}

/* helpers for EXT32 read demonstration */
int recursive_print_cb(const char *name, struct inode *inode, void *p) 
{
    // static const char hex[] = "0123456789abcdef";   // yes this is dumb
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
    } else {
        printk("\n");
    }

    /* Check if it is a regular file */
    // else if (inode->mode & S_IFREG) {
    //     /* Open the file to compute checksum */
    //     struct file *f = inode->open(inode);
    //     uint8_t *buf = kmalloc(4096);
        
    //     if (f) {
    //         uint8_t digest[16];
            
    //         /* Compute hash of content only */
    //         md5File(f, digest);
            
    //         printk(" MD5: ");
    //         for(int i = 0; i < 16; ++i){
    //             char hi = hex[(digest[i] >> 4) & 0xF];
    //             char lo = hex[digest[i] & 0xF];
    //             printk("%c%c", hi, lo);
    //         }

    //         while (f->read(f, buf, 4096) > 0);
            
    //         f->close(f);
    //         kfree(buf);
    //     } else {
    //         printk(" [Error opening file]");
    //     }
        
    //     printk("\n");
    // }

    return 0;
}


void ls_recursive(struct inode *dir, int indent_level) 
{
    /* pass the indent level as the 'void *p' user data */
    dir->readdir(dir, recursive_print_cb, &indent_level);
}
