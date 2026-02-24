#include <stdint-gcc.h>
#include <stddef.h>

#include <string.h>
#include <stdio.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/block_dev.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>
#include <kernel/elf64.h>
#include <kernel/multitask.h> 

/* context struct for dir search callback */
struct find_ctx {
    const char *target;
    struct inode *result;
};


/* cb function to find a file by name in a directory */
static int find_file_cb(const char *name, struct inode *inode, void *p) 
{
    struct find_ctx *ctx = (struct find_ctx *)p;
    
    if (strcmp(name, ctx->target) == 0) {
        ctx->result = inode;
        return 1;                       /* stop searching */
    }

    return 0;
}


void elf_load(struct inode *root, const char *filename) 
{
    struct find_ctx ctx;
    struct inode *inode;
    struct file *f;
    struct ELF64_head header;
    struct ELF64_prog_head ph;
    int i;
    uint64_t current_ph_pos;

    printk("ELF: attempting to load %s\n", filename);

    /* find file */
    ctx.target = filename;
    ctx.result = NULL;
    
    if (root->readdir) {
        root->readdir(root, find_file_cb, &ctx);
    }

    if (ctx.result == NULL) {
        printk("ELF Error: file %s not found\n", filename);
        return;
    }

    inode = ctx.result;

    /* given the file exists, open */
    f = inode->open(inode);
    if (f == NULL) {
        printk("ELF Error: failed to open file\n");
        return;
    }

    /* read and verify the ELF Header */
    if (f->read(f, &header, sizeof(header)) != sizeof(header)) {
        printk("ELF Error: failed to read header\n");
        f->close(f);
        return;
    }

    /* check magic: 0x7F, 'E', 'L', 'F' */
    if (header.common.magic[0] != 0x7F ||
        header.common.magic[1] != 'E' ||
        header.common.magic[2] != 'L' ||
        header.common.magic[3] != 'F') {
        printk("ELF Error: invalid magic bytes\n");
        f->close(f);
        return;
    }

    printk("ELF: entry point: %p\n", (void *)header.prog_entry_pos);

    /* load segments */
    current_ph_pos = header.prog_table_pos;

    for (i = 0; i < header.prog_ent_num; i++) {
        /* seek current program header entry */
        f->lseek(f, current_ph_pos);
        if (f->read(f, &ph, sizeof(ph)) != sizeof(ph)) {
            printk("ELF Error: failed to read program header %d\n", i);
            break;
        }

        /* prog header type = 1. PT_LOAD */
        if (ph.type == 1) {
            printk("ELF: loading segment %d to %p (size: 0x%lx)\n", 
                   i, (void *)ph.load_addr, ph.mem_size);
            
            /* demand page */
            if(MMU_alloc_at(ph.load_addr, ph.mem_size) == NULL) {
                printk("ELF Error: failed to alloc pages for a segment\n");
                f->close(f);
                return;
            }

            /* seek segment data */
            f->lseek(f, ph.file_offset);
            
            /* read file_size bytes into memory */
            f->read(f, (void *)ph.load_addr, ph.file_size);

            /* handle BSS (uninitialized data) */
            if (ph.mem_size > ph.file_size) {
                memset((void *)(ph.load_addr + ph.file_size), 0, 
                       ph.mem_size - ph.file_size);
            }
        }

        /* go to the next program header */
        current_ph_pos += header.prog_ent_size;
    }

    f->close(f);

    /* start the new kernel thread */
    PROC_create_kthread((kproc_t)header.prog_entry_pos, NULL);
    
    printk("ELF: process started successfully\n");
}