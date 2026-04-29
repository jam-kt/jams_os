#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/memory.h>
#include <kernel/scheduler.h>
#include <kernel/multitask.h>

#include "multitask_internal.h"

#define x86_64_ALIGNMENT 16                     /* cpu happy  */   
#define KERNEL_CS 0x08                          /* see boot.asm GDT layout */
#define DEFAULT_RFLAGS 0x202
#define USER_CS 0x18                            /* see boot.asm GDT layout */
#define USER_DS 0x20
#define USER_RPL 3

static scheduler sched = NULL;
proc curr_proc = NULL;
proc next_proc = NULL;
static process_st main_proc;
static uint64_t next_pid = 1;
static int multitask_started = 0;

static void thread_start(kproc_t entry, void *arg);
static uintptr_t align_down(uintptr_t addr, size_t align);

static void enqueue(proc_queue *q, proc p);
static proc dequeue(proc_queue *q);

/*******************************************************************************
 * MULTITASKING INIT
 ******************************************************************************/

void multitask_init()
{
    sched = round_robin;
    PROC_register_syscalls();
}

void PROC_run(void)
{
    if (!multitask_started) {
        memset(&main_proc, 0, sizeof(main_proc));
        PROC_init_queue(&main_proc.wait_child_exit);
        main_proc.run_state = PROC_RUNNING;
        main_proc.cr3 = MMU_get_kernel_p4();
        curr_proc = &main_proc;
        next_proc = &main_proc;
        multitask_started = 1;
    }

    yield();
}

/*******************************************************************************
 * PROCESS CREATION
 ******************************************************************************/
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
    PROC_init_fields(new_proc, curr_proc);

    /* stack var serves as the base (low addr) of stack */
    void *stack = kmalloc(DEFAULT_STACK_BYTES);
    if (!stack) {
        printk("PROC_create_kthread: failed to allocate stack\n");
        kfree(new_proc);
        return;
    }

    new_proc->kstack = stack;
    new_proc->stacksize = DEFAULT_STACK_BYTES;
    new_proc->cr3 = MMU_get_kernel_p4();

    /* sp var serves as the stack pointer (high addr) of stack */
    
    uintptr_t sp = (uintptr_t)stack + DEFAULT_STACK_BYTES;
    sp = align_down(sp, x86_64_ALIGNMENT);

    uint64_t *stack_top = (uint64_t *)sp;

    /* "pushing" what is normall saved on an interrupt (restored by iretq)  */
    /* NOTE: on priv level changes IRETQ will pop more than RIP, CS, RFLAGS */
    *--stack_top = 0;               /* SS (0 is fine for kernel mode)       */
    *--stack_top = (uint64_t)sp;    /* RSP (Points to the top of the stack) */
    *--stack_top = DEFAULT_RFLAGS;
    *--stack_top = KERNEL_CS;
    *--stack_top = (uint64_t)thread_start; /* set RIP to a trampoline */

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

    PROC_link_child(curr_proc, new_proc);
    sched->admit(new_proc);
}

/* a new user page table must be created and passed through arg 3.
 * The calling context for this function must temporarily use that page table
 */
int PROC_create_uthread(kproc_t entry_point, void *arg, uint64_t cr3,
                        void *ustack)
{
    if (!entry_point || !ustack) {
        return -1;
    }

    proc new_proc = kmalloc(sizeof(*new_proc));
    if (!new_proc) {
        printk("PROC_create_uthread: failed to allocate proc\n");
        return -1;
    }

    memset(new_proc, 0, sizeof(*new_proc));
    PROC_init_fields(new_proc, curr_proc);

    /* stack var serves as the base (low addr) of stack */
    void *kstack = kmalloc(DEFAULT_STACK_BYTES);
    if (!kstack) {
        printk("PROC_create_uthread: failed to allocate stack\n");
        kfree(new_proc);
        return -1;
    }

    new_proc->kstack = kstack;
    new_proc->ustack = ustack;
    new_proc->stacksize = DEFAULT_STACK_BYTES;
    new_proc->cr3 = cr3;

    /* sp vars serve as the stack pointers (high addr) of stacks */
    uintptr_t ksp = (uintptr_t)kstack + DEFAULT_STACK_BYTES;
    ksp = align_down(ksp, x86_64_ALIGNMENT);
    uint64_t *kstack_top = (uint64_t *)ksp;

    uintptr_t usp = (uintptr_t)ustack + DEFAULT_STACK_BYTES;
    usp = align_down(usp, x86_64_ALIGNMENT);

    /* "pushing" what is normall saved on an interrupt (restored by iretq)  */
    *--kstack_top = USER_DS | USER_RPL;         /* SS */
    *--kstack_top = (uint64_t)usp;              /* RSP (to the user stack) */
    *--kstack_top = DEFAULT_RFLAGS;
    *--kstack_top = USER_CS | USER_RPL;
    *--kstack_top = (uint64_t)entry_point;      /* no trampoline */

    /* "pushing" nothing for err and vector to mimic interrupt stub behavior */
    *--kstack_top = 0; /* error code */
    *--kstack_top = 0; /* vector */

    /* "push" rest of context registers */
    *--kstack_top = 0;                       /* rax */
    *--kstack_top = 0;                       /* rcx */
    *--kstack_top = 0;                       /* rdx */
    *--kstack_top = 0;                       /* rsi */
    *--kstack_top = (uint64_t)arg;           /* rdi */
    *--kstack_top = 0;                       /* r8  */
    *--kstack_top = 0;                       /* r9  */
    *--kstack_top = 0;                       /* r10 */
    *--kstack_top = 0;                       /* r11 */
    *--kstack_top = 0;                       /* rbx */
    *--kstack_top = 0;                       /* rbp */
    *--kstack_top = 0;                       /* r12 */
    *--kstack_top = 0;                       /* r13 */
    *--kstack_top = 0;                       /* r14 */
    *--kstack_top = 0;                       /* r15 */

    new_proc->state.rsp = (unsigned long)kstack_top;

    PROC_link_child(curr_proc, new_proc);
    sched->admit(new_proc);
    
    return 0;
}

void PROC_reschedule(void)
{
    proc candidate = sched->next();
    if (candidate == NULL) {
        /* go to idle thread */
        if (multitask_started) {
            next_proc = &main_proc;
        } else {
            next_proc = NULL;
        }

        return;
    }

    if (candidate == curr_proc && sched->qlen && sched->qlen() > 1) {
        candidate = sched->next();
    }

    if (curr_proc && curr_proc->run_state == PROC_RUNNING) {
        curr_proc->run_state = PROC_READY;
    }
    candidate->run_state = PROC_RUNNING;
    next_proc = candidate;
}

/*******************************************************************************
 * THREAD TRAMPOLINE
 ******************************************************************************/
static void thread_start(kproc_t entry, void *arg)
{
    entry(arg);
    printk("A kernel thread fell off, calling kexit on thread\n");
    kexit();
}

static uintptr_t align_down(uintptr_t addr, size_t align)
{
    return addr & ~(align - 1);
}

/*******************************************************************************
 * INTERNAL PROCESS HELPERS
 ******************************************************************************/
proc PROC_main_proc(void)
{
    return &main_proc;
}

void PROC_admit(proc p)
{
    sched->admit(p);
}

void PROC_remove(proc p)
{
    sched->remove(p);
}

void PROC_init_fields(proc p, proc parent)
{
    p->pid = next_pid++;
    p->stacksize = DEFAULT_STACK_BYTES;
    p->run_state = PROC_READY;
    p->exit_status = 0;
    p->parent = parent;
    p->first_child = NULL;
    p->next_sibling = NULL;
    PROC_init_queue(&p->wait_child_exit);
}

void PROC_link_child(proc parent, proc child)
{
    if (!parent || !child) {
        return;
    }

    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

void PROC_unlink_child(proc parent, proc child)
{
    proc prev = NULL;
    proc curr;

    if (!parent || !child) {
        return;
    }

    curr = parent->first_child;
    while (curr) {
        if (curr == child) {
            if (prev) {
                prev->next_sibling = curr->next_sibling;
            } else {
                parent->first_child = curr->next_sibling;
            }
            curr->parent = NULL;
            curr->next_sibling = NULL;
            return;
        }

        prev = curr;
        curr = curr->next_sibling;
    }
}

proc PROC_find_zombie_child(proc parent)
{
    for (proc child = parent ? parent->first_child : NULL;
         child;
         child = child->next_sibling) {
        if (child->run_state == PROC_ZOMBIE) {
            return child;
        }
    }

    return NULL;
}

void PROC_reparent_children(proc exiting)
{
    proc child;

    if (!exiting || !exiting->first_child) {
        return;
    }

    child = exiting->first_child;
    while (child->next_sibling) {
        child->parent = &main_proc;
        child = child->next_sibling;
    }
    child->parent = &main_proc;
    child->next_sibling = main_proc.first_child;
    main_proc.first_child = exiting->first_child;
    exiting->first_child = NULL;
}

int PROC_copy_user_string(char *dst, const char *src, size_t dst_size)
{
    size_t i;

    if (!dst || !src || dst_size == 0) {
        return -1;
    }

    for (i = 0; i < dst_size - 1; i++) {
        dst[i] = src[i];
        if (dst[i] == '\0') {
            return 0;
        }
    }

    dst[dst_size - 1] = '\0';
    return -1;
}

/*******************************************************************************
 * BLOCKED PROCESS QUEUES
 ******************************************************************************/

void PROC_init_queue(proc_queue *q)
{
    q->head = NULL;
    q->tail = NULL;
}

/* must be run while interrupts are disabled */
void PROC_block_on(proc_queue *q, int enable_ints)
{
    if (!q) {
        printk("PROC_block_on: no queue to block on\n");
        return;
    }

    /* move from scheduler into driver's blocking queue */
    curr_proc->run_state = PROC_BLOCKED;
    sched->remove(curr_proc);
    enqueue(q, curr_proc);

    if (enable_ints) {
        STI();
    }

    yield();
}

void PROC_unblock_head(proc_queue *q)
{
    if (!q || !q->head) {
        // printk("PROC_unblock_head: queue or queue head is null");
        // printk("This warning may be harmless if no thread called the blocking
        //     wait function but the ISR related to it fired. Ex. keyboard ISR but
        //     no thread is calling kbd_read\n");
        return;
    }

    proc p = dequeue(q);
    if (p) {
        p->run_state = PROC_READY;
        sched->admit(p);
    }
}

void PROC_unblock_all(proc_queue *q)
{
    if (!q) {
        printk("PROC_unblock_all: queue is null\n");
        return;
    }

    while (q->head) {
        PROC_unblock_head(q);
    }
}

/* adds a proc to the tail of the queue */
static void enqueue(proc_queue *q, proc p)
{
    p->lib_one = NULL;

    /* init the queue if it empty */
    if (!q->head) {
        q->head = p;
        q->tail = p;
    } else {
        q->tail->lib_one = p;
        q->tail = p; 
    }
}

/* removes a proc from the head of the queue */
static proc dequeue(proc_queue *q)
{
    if (!q->head) {
        printk("dequeue (from multitasking): no head in the queue\n");
        return NULL;
    }

    proc temp = q->head;

    if (q->head == q->tail) {
        q->head = NULL;
        q->tail = NULL;
    } else {
        q->head = temp->lib_one;
    }

    temp->lib_one = NULL;

    return temp;
}

int num_proc_runnable()
{
    return sched->qlen();
}
