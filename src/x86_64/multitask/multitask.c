#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/scheduler.h>
#include <kernel/multitask.h>

#define DEFAULT_STACK_BYTES (8 * 1024 * 1024)   /* binary 8MB */
#define x86_64_ALIGNMENT 16                     /* cpu happy  */   
#define KERNEL_CS 0x08
#define DEFAULT_RFLAGS 0x202


static scheduler sched = NULL;
proc curr_proc = NULL;
proc next_proc = NULL;
static process_st main_proc;
static uint64_t next_pid = 1;
static int multitask_started = 0;

static void syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5, uint64_t a6);
static void syscall_kexit(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5, uint64_t a6);
static void kthread_start(kproc_t entry, void *arg);
static uintptr_t align_down(uintptr_t addr, size_t align);


void multitask_init()
{
    sched = round_robin;
    register_syscall(SYS_YIELD_NUM, syscall_yield);
    register_syscall(SYS_KEXIT_NUM, syscall_kexit);
}

void syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                    uint64_t a5, uint64_t a6)
{
    printk("We made it into the yield syscall\n");

    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    PROC_reschedule();
}

/* the public facing syscalls */
void yield(void) 
{
    register long nr asm("rax") = SYS_YIELD_NUM;
    asm volatile("int 0x80" : "+a"(nr) :: "memory");
}

void kexit(void)
{
    register long nr asm("rax") = SYS_KEXIT_NUM;
    asm volatile("int 0x80" : "+a"(nr) :: "memory");
}

void PROC_run(void)
{
    if (!multitask_started) {
        memset(&main_proc, 0, sizeof(main_proc));
        main_proc.pid = 0;
        sched->admit(&main_proc);
        curr_proc = &main_proc;
        next_proc = &main_proc;
        multitask_started = 1;
    }

    yield();
}

void PROC_create_kthread(kproc_t entry_point, void *arg)
{
    if (!entry_point) {
        return;
    }

    proc new_proc = kmalloc(sizeof(*new_proc));
    if (!new_proc) {
        printk("PROC_create_kthread: failed to allocate proc\n");
        return;
    }

    memset(new_proc, 0, sizeof(*new_proc));

    /* stack var serves as the base (low addr) of stack */
    void *stack = kmalloc(DEFAULT_STACK_BYTES);
    if (!stack) {
        printk("PROC_create_kthread: failed to allocate stack\n");
        kfree(new_proc);
        return;
    }

    new_proc->pid = next_pid++;
    new_proc->stack = stack;
    new_proc->stacksize = DEFAULT_STACK_BYTES;

    /* sp var serves as the stack pointer (high addr) of stack */
    
    uintptr_t sp = (uintptr_t)stack + DEFAULT_STACK_BYTES;
    sp = align_down(sp, x86_64_ALIGNMENT);

    uint64_t *stack_top = (uint64_t *)sp;

    /* "pushing" what is normall saved on an interrupt (restored by iretq) */
    /* NOTE: on priv level changes IRETQ will pop more than RIP, CS, RFLAGS */
    *--stack_top = DEFAULT_RFLAGS;
    *--stack_top = KERNEL_CS;
    *--stack_top = (uint64_t)kthread_start; /* set RIP to a trampoline */

    /* "pushing" nothing for err and vector to mimic interrupt stub behavior */
    *--stack_top = 0; /* error code */
    *--stack_top = 0; /* vector */

    /* "push" rest of context registers */
    *--stack_top = 0;                       /* rax */
    *--stack_top = 0;                       /* rcx */
    *--stack_top = 0;                       /* rdx */
    *--stack_top = (uint64_t)arg;           /* rsi */
    *--stack_top = (uint64_t)entry_point;   /* rdi */
    *--stack_top = 0;                       /* r8  */
    *--stack_top = 0;                       /* r9  */
    *--stack_top = 0;                       /* r10 */
    *--stack_top = 0;                       /* r11 */
    *--stack_top = 0;                       /* rbx */
    *--stack_top = 0;                       /* rbp */
    *--stack_top = 0;                       /* r12 */
    *--stack_top = 0;                       /* r13 */
    *--stack_top = 0;                       /* r14 */
    *--stack_top = 0;                       /* r15 */

    new_proc->state.rsp = (unsigned long)stack_top;

    sched->admit(new_proc);
}

void PROC_reschedule(void)
{
    proc candidate = sched->next();
    if (candidate == NULL) {
        if (curr_proc) {
            next_proc = curr_proc;
        } else if (multitask_started) {
            next_proc = &main_proc;
        } else {
            next_proc = NULL;
        }

        return;
    }

    if (candidate == curr_proc && sched->qlen && sched->qlen() > 1) {
        candidate = sched->next();
    }

    next_proc = candidate;
}

static void syscall_kexit(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5, uint64_t a6)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    if (curr_proc == NULL) {
        return;
    }

    proc exiting = curr_proc;
    if (exiting == &main_proc) {
        printk("kexit called on main thread, halting\n");
        __asm__("hlt");
    }

    sched->remove(exiting);
    curr_proc = NULL;

    if (exiting->stack) {
        kfree(exiting->stack);
    }

    if (exiting != &main_proc) {
        kfree(exiting);
    }

    PROC_reschedule();
}

static void kthread_start(kproc_t entry, void *arg)
{
    printk("made into kthread_start\n");
    entry(arg);
    kexit();
}

static uintptr_t align_down(uintptr_t addr, size_t align)
{
    return addr & ~(align - 1);
}