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
static process_st kernel_process;
static thread_st main_thread;
static uint64_t next_pid = 1;
static uint64_t next_tid = 1;
static int multitask_started = 0;

static void thread_start(kproc_t entry, void *arg);
static uintptr_t align_down(uintptr_t addr, size_t align);
static process alloc_process(process parent, uint64_t cr3);
static proc alloc_thread(process owner);
static void destroy_process_address_space(process p);
static int setup_kthread(proc new_proc, kproc_t entry_point, void *arg);
int PROC_setup_uthread(proc new_proc, kproc_t entry_point, void *arg,
                       uint64_t user_rsp);
static void process_teardown(process exiting, int status);

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
        memset(&kernel_process, 0, sizeof(kernel_process));
        memset(&main_thread, 0, sizeof(main_thread));
        PROC_init_process(&kernel_process, NULL, MMU_get_kernel_p4());
        kernel_process.pid = 0;
        next_pid = 1;
        PROC_init_thread(&main_thread, &kernel_process);
        main_thread.tid = NO_THREAD;
        main_thread.run_state = PROC_RUNNING;
        PROC_link_thread(&kernel_process, &main_thread);
        curr_proc = &main_thread;
        next_proc = &main_thread;
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

    proc new_proc = alloc_thread(&kernel_process);
    if (!new_proc) {
        printk("PROC_create_kthread: failed to allocate proc\n");
        return;
    }

    if (setup_kthread(new_proc, entry_point, arg) < 0) {
        PROC_unlink_thread(&kernel_process, new_proc);
        if (kernel_process.live_threads > 0) {
            kernel_process.live_threads--;
        }
        PROC_free_thread(new_proc);
        return;
    }

    sched->admit(new_proc);
}

/* a new user page table must be created and passed through arg 3. */
int PROC_create_uthread(kproc_t entry_point, void *arg, uint64_t cr3,
                        void *ustack)
{
    if (!entry_point || !ustack) {
        return -1;
    }

    process parent = curr_proc ? curr_proc->owner : &kernel_process;
    process new_process = alloc_process(parent, cr3);
    proc new_proc;

    if (!new_process) {
        printk("PROC_create_uthread: failed to allocate process\n");
        return -1;
    }

    new_proc = alloc_thread(new_process);
    if (!new_proc) {
        printk("PROC_create_uthread: failed to allocate proc\n");
        PROC_free_process(new_process);
        return -1;
    }

    if (PROC_setup_uthread(new_proc, entry_point, arg,
                           (uint64_t)ustack + DEFAULT_STACK_BYTES) < 0) {
        PROC_unlink_thread(new_process, new_proc);
        if (new_process->live_threads > 0) {
            new_process->live_threads--;
        }
        PROC_free_thread(new_proc);
        PROC_free_process(new_process);
        return -1;
    }

    new_proc->ustack = ustack;
    PROC_link_process_child(parent, new_process);
    sched->admit(new_proc);
    
    return 0;
}

void PROC_reschedule(void)
{
    proc candidate = sched->next();
    if (candidate == NULL) {
        /* go to idle thread */
        if (multitask_started) {
            next_proc = &main_thread;
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

static process alloc_process(process parent, uint64_t cr3)
{
    process p = kmalloc(sizeof(*p));

    if (!p) {
        return NULL;
    }

    memset(p, 0, sizeof(*p));
    PROC_init_process(p, parent, cr3);

    return p;
}

static proc alloc_thread(process owner)
{
    proc t = kmalloc(sizeof(*t));

    if (!t) {
        return NULL;
    }

    memset(t, 0, sizeof(*t));
    PROC_init_thread(t, owner);
    PROC_link_thread(owner, t);

    return t;
}

static void destroy_process_address_space(process p)
{
    if (!p || !p->cr3 || p->cr3 == MMU_get_kernel_p4()) {
        return;
    }

    uint64_t old_cr3 = p->cr3;
    uint64_t kernel_cr3 = MMU_get_kernel_p4();
    MMU_switch_p4(kernel_cr3);
    p->cr3 = 0;
    MMU_destroy_userspace(old_cr3);
}

static int setup_kthread(proc new_proc, kproc_t entry_point, void *arg)
{
    void *stack = kmalloc(DEFAULT_STACK_BYTES);
    uintptr_t sp;
    uint64_t *stack_top;

    if (!stack) {
        printk("setup_kthread: failed to allocate stack\n");
        return -1;
    }

    new_proc->kstack = stack;
    sp = (uintptr_t)stack + DEFAULT_STACK_BYTES;
    sp = align_down(sp, x86_64_ALIGNMENT);
    stack_top = (uint64_t *)sp;

    /* "pushing" what is normally saved on an interrupt. */
    *--stack_top = 0;
    *--stack_top = (uint64_t)sp;
    *--stack_top = DEFAULT_RFLAGS;
    *--stack_top = KERNEL_CS;
    *--stack_top = (uint64_t)thread_start;

    *--stack_top = 0;
    *--stack_top = 0;

    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = (uint64_t)arg;
    *--stack_top = (uint64_t)entry_point;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;

    new_proc->state.rsp = (unsigned long)stack_top;

    return 0;
}

int PROC_setup_uthread(proc new_proc, kproc_t entry_point, void *arg,
                       uint64_t user_rsp)
{
    void *kstack = kmalloc(DEFAULT_STACK_BYTES);
    uintptr_t ksp;
    uintptr_t usp;
    uint64_t *kstack_top;

    if (!kstack) {
        printk("setup_uthread: failed to allocate stack\n");
        return -1;
    }

    new_proc->kstack = kstack;
    ksp = (uintptr_t)kstack + DEFAULT_STACK_BYTES;
    ksp = align_down(ksp, x86_64_ALIGNMENT);
    kstack_top = (uint64_t *)ksp;

    usp = align_down(user_rsp, x86_64_ALIGNMENT);

    *--kstack_top = USER_DS | USER_RPL;
    *--kstack_top = (uint64_t)usp;
    *--kstack_top = DEFAULT_RFLAGS;
    *--kstack_top = USER_CS | USER_RPL;
    *--kstack_top = (uint64_t)entry_point;

    *--kstack_top = 0;
    *--kstack_top = 0;

    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = (uint64_t)arg;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;
    *--kstack_top = 0;

    new_proc->ustack = (uint64_t *)user_rsp;
    new_proc->state.rsp = (unsigned long)kstack_top;

    return 0;
}

static void process_teardown(process exiting, int status)
{
    if (!exiting || exiting->zombie) {
        return;
    }

    exiting->exiting = 1;
    exiting->zombie = 1;
    exiting->exit_status = status;
    PROC_reparent_children(exiting);
    destroy_process_address_space(exiting);

    if (exiting->parent) {
        PROC_unblock_all(&exiting->parent->wait_child_exit);
    }
}

/*******************************************************************************
 * INTERNAL PROCESS HELPERS
 ******************************************************************************/
proc PROC_main_proc(void)
{
    return &main_thread;
}

process PROC_main_process(void)
{
    return &kernel_process;
}

void PROC_admit(proc p)
{
    sched->admit(p);
}

void PROC_remove(proc p)
{
    sched->remove(p);
}

void PROC_init_process(process p, process parent, uint64_t cr3)
{
    p->pid = next_pid++;
    p->cr3 = cr3;
    p->parent = parent;
    p->first_child = NULL;
    p->next_sibling = NULL;
    p->threads = NULL;
    p->live_threads = 0;
    p->exit_status = 0;
    p->exiting = 0;
    p->zombie = 0;
    PROC_init_queue(&p->wait_child_exit);
    PROC_init_queue(&p->wait_thread_exit);
}

void PROC_init_thread(proc t, process owner)
{
    t->tid = next_tid++;
    t->kstack = NULL;
    t->ustack = NULL;
    t->stacksize = DEFAULT_STACK_BYTES;
    t->owner = owner;
    memset(&t->state, 0, sizeof(t->state));
    t->run_state = PROC_READY;
    t->exit_status = 0;
    t->next_thread = NULL;
    t->lib_one = NULL;
    t->lib_two = NULL;
    t->sched_one = NULL;
    t->sched_two = NULL;
    t->exited = NULL;
}

void PROC_link_process_child(process parent, process child)
{
    if (!parent || !child) {
        return;
    }

    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

void PROC_unlink_process_child(process parent, process child)
{
    process prev = NULL;
    process curr;

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

process PROC_find_zombie_child(process parent)
{
    for (process child = parent ? parent->first_child : NULL;
         child;
         child = child->next_sibling) {
        if (child->zombie) {
            return child;
        }
    }

    return NULL;
}

void PROC_reparent_children(process exiting)
{
    process child;

    if (!exiting || !exiting->first_child) {
        return;
    }

    child = exiting->first_child;
    while (child->next_sibling) {
        child->parent = &kernel_process;
        child = child->next_sibling;
    }
    child->parent = &kernel_process;
    child->next_sibling = kernel_process.first_child;
    kernel_process.first_child = exiting->first_child;
    exiting->first_child = NULL;
}

void PROC_link_thread(process owner, proc t)
{
    if (!owner || !t) {
        return;
    }

    t->owner = owner;
    t->next_thread = owner->threads;
    owner->threads = t;
    owner->live_threads++;
}

void PROC_unlink_thread(process owner, proc t)
{
    proc prev = NULL;
    proc curr;

    if (!owner || !t) {
        return;
    }

    curr = owner->threads;
    while (curr) {
        if (curr == t) {
            if (prev) {
                prev->next_thread = curr->next_thread;
            } else {
                owner->threads = curr->next_thread;
            }
            curr->next_thread = NULL;
            return;
        }

        prev = curr;
        curr = curr->next_thread;
    }
}

proc PROC_find_thread(process owner, uint64_t tid)
{
    for (proc t = owner ? owner->threads : NULL; t; t = t->next_thread) {
        if (t->tid == tid) {
            return t;
        }
    }

    return NULL;
}

void PROC_free_thread(proc t)
{
    if (!t) {
        return;
    }

    if (t->kstack) {
        kfree(t->kstack);
    }
    kfree(t);
}

void PROC_free_process(process p)
{
    if (!p) {
        return;
    }

    kfree(p);
}

int PROC_exit_current_thread(int status, int whole_process)
{
    process owner;

    if (!curr_proc) {
        return -1;
    }

    if (curr_proc == &main_thread) {
        printk("exit called on main thread, halting\n");
        __asm__("hlt");
    }

    owner = curr_proc->owner;
    if (!owner) {
        return -1;
    }

    if (whole_process && owner != &kernel_process) {
        proc t = owner->threads;

        owner->exiting = 1;
        while (t) {
            if (t->run_state != PROC_ZOMBIE) {
                if (t->run_state == PROC_READY ||
                    t->run_state == PROC_RUNNING) {
                    PROC_remove(t);
                }
                t->run_state = PROC_ZOMBIE;
                t->exit_status = status;
            }
            t = t->next_thread;
        }

        owner->live_threads = 0;
        process_teardown(owner, status);
        PROC_unblock_all(&owner->wait_thread_exit);
    } else {
        curr_proc->run_state = PROC_ZOMBIE;
        curr_proc->exit_status = status;
        if (owner->live_threads > 0) {
            owner->live_threads--;
        }
        PROC_remove(curr_proc);
        PROC_unblock_all(&owner->wait_thread_exit);
        if (owner->live_threads == 0) {
            process_teardown(owner, status);
        }
    }

    curr_proc = NULL;
    PROC_reschedule();

    return 0;
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

    while (q->head) {
        proc p = dequeue(q);
        if (p && p->run_state != PROC_ZOMBIE &&
            (!p->owner || !p->owner->exiting)) {
            p->run_state = PROC_READY;
            sched->admit(p);
            return;
        }
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
