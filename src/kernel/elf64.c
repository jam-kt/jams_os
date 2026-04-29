#include <stdint-gcc.h>
#include <stddef.h>

#include <string.h>
#include <stdio.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/block_dev.h>
#include <kernel/vfs.h>
#include <kernel/ext2.h>
#include <kernel/elf64.h>
#include <kernel/multitask.h>

extern proc curr_proc;

/* context struct for dir search callback */
struct find_ctx {
    const char *target;
    struct inode *result;
};

struct elf_load_ctx {
    struct file *file;
    struct ELF64_head header;
    uint64_t kernel_cr3;
    uint64_t user_cr3;
    uint64_t old_proc_cr3;
};

static struct file *elf_open_file(struct inode *root, const char *filename);
static int elf_read_header(struct file *f, struct ELF64_head *header);
static int elf_switch_to_image_space(struct elf_load_ctx *ctx);
static void elf_restore_kernel_space(struct elf_load_ctx *ctx);
static int elf_load_segment(struct file *f, struct ELF64_prog_head *ph);
static int elf_load_segments(struct file *f, struct ELF64_head *header);
static int elf_alloc_user_stack(struct elf_image *image);

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

static struct file *elf_open_file(struct inode *root, const char *filename)
{
    struct find_ctx ctx;
    struct inode *inode;

    /* find file */
    ctx.target = filename;
    ctx.result = NULL;
    
    if (root->readdir) {
        root->readdir(root, find_file_cb, &ctx);
    }

    if (ctx.result == NULL) {
        printk("ELF Error: file %s not found\n", filename);
        return NULL;
    }

    inode = ctx.result;

    /* given the file exists, open */
    struct file *f = inode->open(inode);
    if (f == NULL) {
        printk("ELF Error: failed to open file\n");
        return NULL;
    }

    return f;
}

static int elf_read_header(struct file *f, struct ELF64_head *header)
{
    if (f->read(f, header, sizeof(*header)) != sizeof(*header)) {
        printk("ELF Error: failed to read header\n");
        return -1;
    }

    /* check magic: 0x7F, 'E', 'L', 'F' */
    if (header->common.magic[0] != 0x7F ||
        header->common.magic[1] != 'E' ||
        header->common.magic[2] != 'L' ||
        header->common.magic[3] != 'F') {
        printk("ELF Error: invalid magic bytes\n");
        return -1;
    }

    printk("ELF: entry point: %p\n", (void *)header->prog_entry_pos);
    return 0;
}

static int elf_switch_to_image_space(struct elf_load_ctx *ctx)
{
    asm volatile("mov %0, cr3" : "=r" (ctx->kernel_cr3));
    ctx->user_cr3 = MMU_create_user_p4();
    if (ctx->user_cr3 == 0) {
        printk("ELF Error: failed to create user page table\n");
        return -1;
    }

    CLI();
    asm volatile("mov cr3, %0" : : "r" (ctx->user_cr3) : "memory");
    if (curr_proc) {
        ctx->old_proc_cr3 = curr_proc->cr3;
        curr_proc->cr3 = ctx->user_cr3;
    }
    STI();

    return 0;
}

static void elf_restore_kernel_space(struct elf_load_ctx *ctx)
{
    CLI();
    asm volatile("mov cr3, %0" : : "r" (ctx->kernel_cr3) : "memory");
    if (curr_proc) {
        curr_proc->cr3 = ctx->old_proc_cr3;
    }
    STI();
}

static int elf_load_segment(struct file *f, struct ELF64_prog_head *ph)
{
    printk("ELF: loading segment to %p (size: %lx)\n", 
           (void *)ph->load_addr, ph->mem_size);
    
    /* demand page */
    if(MMU_alloc_at(ph->load_addr, ph->mem_size) == NULL) {
        printk("ELF Error: failed to alloc pages for a segment\n");
        return -1;
    }

    /* seek segment data */
    f->lseek(f, ph->file_offset);
    
    /* read file_size bytes into memory */
    if (f->read(f, (void *)ph->load_addr, ph->file_size) != ph->file_size) {
        printk("ELF Error: failed to read segment data\n");
        return -1;
    }

    /* handle BSS (uninitialized data) */
    if (ph->mem_size > ph->file_size) {
        memset((void *)(ph->load_addr + ph->file_size), 0, 
               ph->mem_size - ph->file_size);
    }

    return 0;
}

static int elf_load_segments(struct file *f, struct ELF64_head *header)
{
    struct ELF64_prog_head ph;
    uint64_t current_ph_pos = header->prog_table_pos;

    for (int i = 0; i < header->prog_ent_num; i++) {
        /* seek current program header entry */
        f->lseek(f, current_ph_pos);
        if (f->read(f, &ph, sizeof(ph)) != sizeof(ph)) {
            printk("ELF Error: failed to read program header %d\n", i);
            return -1;
        }

        /* prog header type = 1. PT_LOAD */
        if (ph.type == 1 && elf_load_segment(f, &ph) < 0) {
            return -1;
        }

        /* go to the next program header */
        current_ph_pos += header->prog_ent_size;
    }

    return 0;
}

static int elf_alloc_user_stack(struct elf_image *image)
{
    void *ustack = MMU_alloc_at(VA_USTACK_BASE, DEFAULT_STACK_BYTES);

    if (!ustack) {
        printk("ELF Error: failed to allocate user stack\n");
        return -1;
    }

    uintptr_t usp = (uintptr_t)ustack + DEFAULT_STACK_BYTES;
    usp &= ~(16 - 1);

    image->ustack_base = (uint64_t)ustack;
    image->ustack_top = (uint64_t)usp;
    return 0;
}

int elf_load_program(struct inode *root, const char *filename, struct elf_image *image)
{
    struct elf_load_ctx ctx;
    int ret = -1;

    printk("ELF: attempting to load %s\n", filename);

    if (!root || !filename || !image) {
        return -1;
    }

    memset(&ctx, 0, sizeof(ctx));
    memset(image, 0, sizeof(*image));

    ctx.file = elf_open_file(root, filename);
    if (!ctx.file) {
        return -1;
    }

    if (elf_read_header(ctx.file, &ctx.header) < 0) {
        goto out_close;
    }

    if (elf_switch_to_image_space(&ctx) < 0) {
        goto out_close;
    }

    if (elf_load_segments(ctx.file, &ctx.header) < 0) {
        goto out_restore;
    }

    if (elf_alloc_user_stack(image) < 0) {
        goto out_restore;
    }

    image->cr3 = ctx.user_cr3;
    image->entry = ctx.header.prog_entry_pos;
    ret = 0;
    
out_restore:
    elf_restore_kernel_space(&ctx);
    if (ret < 0 && ctx.user_cr3) {
        MMU_destroy_userspace(ctx.user_cr3);
    }

out_close:
    ctx.file->close(ctx.file);
    return ret;
}

void elf_load(struct inode *root, const char *filename) 
{
    struct elf_image image;

    if (elf_load_program(root, filename, &image) < 0) {
        printk("ELF: failed to load program\n");
        return;
    }

    if (PROC_create_uthread((kproc_t)image.entry, NULL, image.cr3, (void *)image.ustack_base) < 0) {
        MMU_destroy_userspace(image.cr3);
        printk("ELF: failed to create a new userspace proces\n");
        return;
    }
}
