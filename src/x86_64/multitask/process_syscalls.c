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
static uint64_t syscall_clone(struct syscall_frame *frame);
static uint64_t syscall_getpid(struct syscall_frame *frame);
static uint64_t syscall_gettid(struct syscall_frame *frame);
static uint64_t syscall_thread_exit(struct syscall_frame *frame);
static uint64_t syscall_thread_join(struct syscall_frame *frame);
static uint64_t clone_process(struct syscall_frame *frame);
static uint64_t clone_thread(struct syscall_frame *frame);

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
    register_syscall(SYS_CLONE_NUM, syscall_clone);
    register_syscall(SYS_GETPID_NUM, syscall_getpid);
    register_syscall(SYS_GETTID_NUM, syscall_gettid);
    register_syscall(SYS_THREAD_EXIT_NUM, syscall_thread_exit);
    register_syscall(SYS_THREAD_JOIN_NUM, syscall_thread_join);
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
    return (uint64_t)PROC_exit_current_thread((int)frame->rdi, 1);
}

static uint64_t syscall_wait(struct syscall_frame *frame)
{
    process parent;

    if (!curr_proc || !curr_proc->owner) {
        return (uint64_t)-1;
    }

    parent = curr_proc->owner;
    printk("pre wait count: %lu\n", MMU_pf_free_count());

    while (1) {
        process child = PROC_find_zombie_child(parent);
        if (child) {
            uint64_t pid = child->pid;
            int *status_ptr = (int *)frame->rdi;
            proc t = child->threads;

            if (status_ptr) {
                *status_ptr = child->exit_status;
            }

            PROC_unlink_process_child(parent, child);
            while (t) {
                proc next = t->next_thread;
                PROC_free_thread(t);
                t = next;
            }
            PROC_free_process(child);
            printk("post wait count: %lu\n", MMU_pf_free_count());
            return pid;
        }

        if (!parent->first_child) {
            return (uint64_t)-1;
        }

        CLI();
        PROC_block_on(&parent->wait_child_exit, 1);
    }
}

static uint64_t syscall_exec(struct syscall_frame *frame)
{
    char filename[128];
    struct elf_image image;
    uint64_t old_cr3;
    process owner;

    printk("pre exec count: %lu\n", MMU_pf_free_count());

    if (!proc_root || !curr_proc || !curr_proc->owner) {
        return (uint64_t)-1;
    }

    owner = curr_proc->owner;
    if (owner->live_threads != 1) {
        return (uint64_t)-1;
    }

    if (PROC_copy_user_string(filename, (const char *)frame->rdi,
                              sizeof(filename)) < 0) {
        return (uint64_t)-1;
    }

    old_cr3 = owner->cr3;
    if (elf_load_program(proc_root, filename, &image) < 0) {
        return (uint64_t)-1;
    }

    CLI();
    owner->cr3 = image.cr3;
    curr_proc->ustack = (uint64_t *)image.ustack_base;
    MMU_switch_p4(image.cr3);
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
    frame->rdi = CLONE_PROCESS;
    frame->rsi = 0;
    frame->rdx = 0;
    frame->r10 = 0;
    return syscall_clone(frame);
}

static uint64_t syscall_clone(struct syscall_frame *frame)
{
    if (frame->rdi == CLONE_PROCESS) {
        return clone_process(frame);
    }

    if (frame->rdi == CLONE_THREAD) {
        return clone_thread(frame);
    }

    return (uint64_t)-1;
}

static uint64_t syscall_getpid(struct syscall_frame *frame)
{
    (void)frame;

    if (!curr_proc || !curr_proc->owner) {
        return 0;
    }

    return curr_proc->owner->pid;
}

static uint64_t syscall_gettid(struct syscall_frame *frame)
{
    (void)frame;

    if (!curr_proc) {
        return 0;
    }

    return curr_proc->tid;
}

static uint64_t syscall_thread_exit(struct syscall_frame *frame)
{
    return (uint64_t)PROC_exit_current_thread((int)frame->rdi, 0);
}

static uint64_t syscall_thread_join(struct syscall_frame *frame)
{
    uint64_t tid = frame->rdi;
    int *status_ptr = (int *)frame->rsi;
    process owner;
    proc target;

    if (!curr_proc || !curr_proc->owner || tid == curr_proc->tid) {
        return (uint64_t)-1;
    }

    owner = curr_proc->owner;
    target = PROC_find_thread(owner, tid);
    if (!target) {
        return (uint64_t)-1;
    }

    while (target->run_state != PROC_ZOMBIE) {
        CLI();
        PROC_block_on(&owner->wait_thread_exit, 1);

        target = PROC_find_thread(owner, tid);
        if (!target) {
            return (uint64_t)-1;
        }
    }

    if (status_ptr) {
        *status_ptr = target->exit_status;
    }

    PROC_unlink_thread(owner, target);
    PROC_free_thread(target);

    return tid;
}

static uint64_t clone_process(struct syscall_frame *frame)
{
    proc parent_thread = curr_proc;
    process parent;
    process child_process;
    proc child;
    void *kstack;
    uint64_t child_cr3;
    uintptr_t frame_offset;
    struct syscall_frame *child_frame;

    printk("pre fork count: %lu\n", MMU_pf_free_count());

    if (!parent_thread || !parent_thread->owner) {
        return (uint64_t)-1;
    }

    parent = parent_thread->owner;
    if (!parent->cr3 || parent->cr3 == MMU_get_kernel_p4() ||
        parent->live_threads != 1) {
        return (uint64_t)-1;
    }

    child_cr3 = MMU_clone_userspace(parent->cr3);
    if (!child_cr3) {
        return (uint64_t)-1;
    }

    child_process = kmalloc(sizeof(*child_process));
    if (!child_process) {
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }
    memset(child_process, 0, sizeof(*child_process));
    PROC_init_process(child_process, parent, child_cr3);

    child = kmalloc(sizeof(*child));
    if (!child) {
        kfree(child_process);
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }
    memset(child, 0, sizeof(*child));
    PROC_init_thread(child, child_process);

    kstack = kmalloc(DEFAULT_STACK_BYTES);
    if (!kstack) {
        kfree(child);
        kfree(child_process);
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }

    frame_offset = (uintptr_t)frame - (uintptr_t)parent_thread->kstack;
    if (frame_offset >= DEFAULT_STACK_BYTES) {
        kfree(kstack);
        kfree(child);
        kfree(child_process);
        MMU_destroy_userspace(child_cr3);
        return (uint64_t)-1;
    }

    memcpy((void *)((uintptr_t)kstack + frame_offset),
           (void *)((uintptr_t)parent_thread->kstack + frame_offset),
           DEFAULT_STACK_BYTES - frame_offset);
    child->kstack = kstack;
    child->ustack = parent_thread->ustack;
    child->stacksize = parent_thread->stacksize;

    /* copy the kernel stack contents and then edit the return value to 0.
     * This means fork will return the child's PID for the parent, and 0 for the
     * child. Copying the parent's kernel stack means when the child first gets
     * scheduled, it appears as though it just finished calling fork() 
    */
    child_frame = (struct syscall_frame *)((uintptr_t)kstack + frame_offset);
    child_frame->rax = 0;
    child->state.rsp = (uint64_t)child_frame;

    PROC_link_thread(child_process, child);
    PROC_link_process_child(parent, child_process);
    PROC_admit(child);

    printk("post fork count: %lu\n", MMU_pf_free_count());

    return child_process->pid;
}

static uint64_t clone_thread(struct syscall_frame *frame)
{
    process owner;
    proc child;
    uint64_t child_stack_top = frame->rsi;
    kproc_t entry = (kproc_t)frame->rdx;
    void *arg = (void *)frame->r10;

    if (!curr_proc || !curr_proc->owner || !child_stack_top || !entry) {
        return (uint64_t)-1;
    }

    owner = curr_proc->owner;
    if (!owner->cr3 || owner->cr3 == MMU_get_kernel_p4()) {
        return (uint64_t)-1;
    }

    child = kmalloc(sizeof(*child));
    if (!child) {
        return (uint64_t)-1;
    }

    memset(child, 0, sizeof(*child));
    PROC_init_thread(child, owner);
    PROC_link_thread(owner, child);

    if (PROC_setup_uthread(child, entry, arg, child_stack_top) < 0) {
        PROC_unlink_thread(owner, child);
        if (owner->live_threads > 0) {
            owner->live_threads--;
        }
        PROC_free_thread(child);
        return (uint64_t)-1;
    }

    PROC_admit(child);

    return child->tid;
}
