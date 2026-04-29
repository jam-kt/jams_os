#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/multitask.h>
#include <kernel/elf64.h>

#include "multitask_internal.h"

static struct inode *proc_root = NULL;

static uint64_t syscall_yield(struct syscall_frame *frame);
static uint64_t syscall_exit(struct syscall_frame *frame);
static uint64_t syscall_wait(struct syscall_frame *frame);
static uint64_t syscall_exec(struct syscall_frame *frame);
static uint64_t syscall_fork(struct syscall_frame *frame);

/*******************************************************************************
 * PROCESS SYSCALLS
 ******************************************************************************/
void PROC_register_syscalls(void)
{
    register_syscall(SYS_YIELD_NUM, syscall_yield);
    register_syscall(SYS_EXIT_NUM, syscall_exit);
    register_syscall(SYS_WAIT_NUM, syscall_wait);
    register_syscall(SYS_EXEC_NUM, syscall_exec);
    register_syscall(SYS_FORK_NUM, syscall_fork);
}

void PROC_set_root(struct inode *root)
{
    proc_root = root;
}

void yield(void) 
{
    register long nr asm("rax") = SYS_YIELD_NUM;
    asm volatile("int 0x80" : "+a"(nr) :: "memory");
}

void kexit(void)
{
    register long nr asm("rax") = SYS_KEXIT_NUM;
    register long status asm("rdi") = 0;
    asm volatile("int 0x80" : "+a"(nr) : "D"(status) : "memory");
}

static uint64_t syscall_yield(struct syscall_frame *frame)
{
    (void)frame;
    PROC_reschedule();

    return 0;
}

static uint64_t syscall_exit(struct syscall_frame *frame)
{
    if (curr_proc == NULL) {
        return 1;
    }

    proc exiting = curr_proc;
    if (exiting == PROC_main_proc()) {
        printk("exit called on main thread, halting\n");
        __asm__("hlt");
    }

    exiting->run_state = PROC_ZOMBIE;
    exiting->exit_status = (int)frame->rdi;
    PROC_reparent_children(exiting);
    PROC_remove(exiting);

    if (exiting->cr3 && exiting->cr3 != MMU_get_kernel_p4()) {
        uint64_t old_cr3 = exiting->cr3;
        uint64_t kernel_cr3 = MMU_get_kernel_p4();
        asm volatile("mov cr3, %0" : : "r" (kernel_cr3) : "memory");
        exiting->cr3 = 0;
        MMU_destroy_userspace(old_cr3);
    }

    if (exiting->parent) {
        PROC_unblock_all(&exiting->parent->wait_child_exit);
    }

    curr_proc = NULL;
    PROC_reschedule();

    return 0;
}

static uint64_t syscall_wait(struct syscall_frame *frame)
{
    if (!curr_proc) {
        return (uint64_t)-1;
    }

    printk("pre wait count: %lu\n", MMU_pf_free_count());

    while (1) {
        proc child = PROC_find_zombie_child(curr_proc);
        if (child) {
            uint64_t pid = child->pid;
            int *status_ptr = (int *)frame->rdi;

            if (status_ptr) {
                *status_ptr = child->exit_status;
            }

            PROC_unlink_child(curr_proc, child);
            if (child->kstack) {
                kfree(child->kstack);
            }
            kfree(child);
            printk("post wait count: %lu\n", MMU_pf_free_count());
            return pid;
        }

        if (!curr_proc->first_child) {
            return (uint64_t)-1;
        }

        CLI();
        curr_proc->run_state = PROC_BLOCKED;
        PROC_block_on(&curr_proc->wait_child_exit, 1);
        CLI();
        if (curr_proc) {
            curr_proc->run_state = PROC_RUNNING;
        }
        STI();
    }
}

static uint64_t syscall_exec(struct syscall_frame *frame)
{
    char filename[128];
    struct elf_image image;
    uint64_t old_cr3;

    printk("pre exec count: %lu\n", MMU_pf_free_count());

    if (!proc_root || !curr_proc) {
        return (uint64_t)-1;
    }

    if (PROC_copy_user_string(filename, (const char *)frame->rdi,
                              sizeof(filename)) < 0) {
        return (uint64_t)-1;
    }

    old_cr3 = curr_proc->cr3;
    if (elf_load_program(proc_root, filename, &image) < 0) {
        return (uint64_t)-1;
    }

    CLI();
    curr_proc->cr3 = image.cr3;
    curr_proc->ustack = (uint64_t *)image.ustack_base;
    asm volatile("mov cr3, %0" : : "r" (image.cr3) : "memory");
    if (old_cr3 && old_cr3 != MMU_get_kernel_p4() && old_cr3 != image.cr3) {
        MMU_destroy_userspace(old_cr3);
    }
    STI();

    frame->rip = image.entry;
    frame->rsp = image.ustack_top;
    frame->rdi = 0;
    frame->rsi = 0;
    frame->rdx = 0;

    printk("post exec count: %lu\n", MMU_pf_free_count());

    return 0;
}

static uint64_t syscall_fork(struct syscall_frame *frame)
{
    proc parent = curr_proc;
    proc child;
    void *kstack;
    uint64_t child_cr3;
    uintptr_t frame_offset;
    struct syscall_frame *child_frame;

    printk("pre fork count: %lu\n", MMU_pf_free_count());

    if (!parent || !parent->cr3 || parent->cr3 == MMU_get_kernel_p4()) {
        return (uint64_t)-1;
    }

    child_cr3 = MMU_clone_userspace(parent->cr3);
    if (!child_cr3) {
        return (uint64_t)-1;
    }

    child = kmalloc(sizeof(*child));
    if (!child) {
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }
    memset(child, 0, sizeof(*child));

    kstack = kmalloc(DEFAULT_STACK_BYTES);
    if (!kstack) {
        kfree(child);
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }

    frame_offset = (uintptr_t)frame - (uintptr_t)parent->kstack;
    if (frame_offset >= DEFAULT_STACK_BYTES) {
        kfree(kstack);
        kfree(child);
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }

    memcpy((void *)((uintptr_t)kstack + frame_offset),
           (void *)((uintptr_t)parent->kstack + frame_offset),
           DEFAULT_STACK_BYTES - frame_offset);
    PROC_init_fields(child, parent);
    child->kstack = kstack;
    child->ustack = parent->ustack;
    child->stacksize = parent->stacksize;
    child->cr3 = child_cr3;
    child->run_state = PROC_READY;

    /* copy the kernel stack contents and then edit the return value to 0.
     * This means fork will return the child's PID for the parent, and 0 for the
     * child. Copying the parent's kernel stack means when the child first gets
     * scheduled, it appears as though it just finished calling fork() 
    */
    child_frame = (struct syscall_frame *)((uintptr_t)kstack + frame_offset);
    child_frame->rax = 0;
    child->state.rsp = (uint64_t)child_frame;

    PROC_link_child(parent, child);
    PROC_admit(child);

    printk("post fork count: %lu\n", MMU_pf_free_count());

    return child->pid;
}
