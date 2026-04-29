#ifndef __MULTITASK_H__
#define __MULTITASK_H__

#include <stddef.h>
#include <stdint-gcc.h>

#define DEFAULT_STACK_BYTES (8 * 1024 * 1024)   /* binary 8MB */

typedef void (*kproc_t)(void *);
struct inode;

void yield(void);
void kexit(void);
void multitask_init();
void PROC_run(void);
void PROC_create_kthread(kproc_t entry_point, void *arg);
int PROC_create_uthread(kproc_t entry_point, void *arg, uint64_t cr3,
                        void *ustack);
void PROC_reschedule(void);
void PROC_set_root(struct inode *root);


/* unused right now, maybe we can use it to clean up code later */
typedef struct __attribute__ ((aligned(16))) __attribute__ ((packed)) regs {
    uint64_t rax;            /* the sixteen architecturally-visible regs. */
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;            /* base pointer (unknown addr)               */
    uint64_t rsp;            /* stack pointer (grows downwards)           */
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} rfile;

#define NO_THREAD 0             /* an always invalid thread id */

typedef struct process_st *proc;
/* queue for blocked processes */
typedef struct proc_queue {
    proc head;
    proc tail;
} proc_queue;

typedef enum proc_state {
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
} proc_state_t;

typedef struct process_st {
    uint64_t    pid;            /* lightweight process id  */
    uint64_t    *kstack;        /* Base of kernel stack    */
    uint64_t    *ustack;        /* Base of user stack      */
    size_t      stacksize;      /* Size of the two stack   */
    uint64_t    cr3;            /* addr of user P4 table   */
    rfile       state;          /* saved registers         */
    proc_state_t run_state;
    int         exit_status;
    proc        parent;
    proc        first_child;
    proc        next_sibling;
    proc_queue  wait_child_exit;
    proc        lib_one;        /* Two pointers reserved   */
    proc        lib_two;        /* for use by the library  */
    proc        sched_one;      /* Two more for            */
    proc        sched_two;      /* schedulers to use       */
    proc        exited;         /* and one for lwp_wait()  */
} process_st;


void PROC_init_queue(proc_queue *q);
void PROC_block_on(proc_queue *q, int enable_ints);
void PROC_unblock_head(proc_queue *q);
void PROC_unblock_all(proc_queue *q);
int num_proc_runnable();

#define wait_event_interruptable(queue, condition) \
    CLI();                          \
    while(!(condition)) {           \
        PROC_block_on(&(queue), 1); \
        CLI();                      \
    }                               \
    STI();                          \

#endif
