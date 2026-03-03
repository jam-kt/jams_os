#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>
#include <string.h>
#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/syscall.h>
#include <kernel/memory.h>
#include <kernel/scheduler.h>
#include <kernel/multitask.h>

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

static uint64_t syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5, uint64_t a6);
static uint64_t syscall_kexit(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                          uint64_t a5, uint64_t a6);
static void thread_start(kproc_t entry, void *arg);
static uintptr_t align_down(uintptr_t addr, size_t align);

static void enqueue(proc_queue *q, proc p);
static proc dequeue(proc_queue *q);


void multitask_init()
{
    sched = round_robin;
    register_syscall(SYS_YIELD_NUM, syscall_yield);
    register_syscall(SYS_KEXIT_NUM, syscall_kexit);
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
    /* TODO: SWITCH BACK TO VECTOR 0x80 ONCE WE HAVE A CLEANUP THREAD 
     * SEE syscalls.c FOR REST OF THE TEMP KEXIT RUGPULL FIX */
    asm volatile("int 0x81" : "+a"(nr) :: "memory");
}

uint64_t syscall_yield(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
    uint64_t a5, uint64_t a6)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    PROC_reschedule();

    return 0;
}

static uint64_t syscall_kexit(uint64_t a1, uint64_t a2, uint64_t a3, 
    uint64_t a4, uint64_t a5, uint64_t a6)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    if (curr_proc == NULL) {
        return 1;
    }

    proc exiting = curr_proc;
    if (exiting == &main_proc) {
        printk("kexit called on main thread, halting\n");
        __asm__("hlt");
    }

    sched->remove(exiting);
    curr_proc = NULL;

    if (exiting->kstack) {
        kfree(exiting->kstack);
    }
    // if (exiting->ustack) {
    //     kfree(exiting->ustack);
    // }

    if (exiting != &main_proc) {
        kfree(exiting);
    }

    PROC_reschedule();

    return 0;
}

void PROC_run(void)
{
    if (!multitask_started) {
        memset(&main_proc, 0, sizeof(main_proc));
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
    new_proc->kstack = stack;
    new_proc->stacksize = DEFAULT_STACK_BYTES;

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

    sched->admit(new_proc);
}

/* a new user page table must be created and passed through arg 3.
 * The calling context for this function must temporarily use that page table
 */
void PROC_create_uthread(kproc_t entry_point, void *arg, uint64_t cr3)
{
    if (!entry_point) {
        return;
    }

    proc new_proc = kmalloc(sizeof(*new_proc));
    if (!new_proc) {
        printk("PROC_create_uthread: failed to allocate proc\n");
        return;
    }

    memset(new_proc, 0, sizeof(*new_proc));

    /* stack var serves as the base (low addr) of stack */
    void *kstack = kmalloc(DEFAULT_STACK_BYTES);
    void *ustack = MMU_alloc_at(VA_USTACK_BASE, DEFAULT_STACK_BYTES);
    if (!kstack || !ustack) {
        printk("PROC_create_uthread: failed to allocate stack\n");
        kfree(new_proc);
        return;
    }

    new_proc->pid = next_pid++;
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


/* process management code */

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
