#ifndef __MULTITASK_H__
#define __MULTITASK_H__

#include <stddef.h>
#include <stdint-gcc.h>

typedef void (*kproc_t)(void *);

void yield(void);
void kexit(void);
void multitask_init();
void PROC_run(void);
void PROC_create_kthread(kproc_t entry_point, void *arg);
void PROC_reschedule(void);


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
typedef struct process_st {
  uint64_t      pid;            /* lightweight process id  */
  uint64_t      *stack;         /* Base of allocated stack */
  size_t        stacksize;      /* Size of allocated stack */
  rfile         state;          /* saved registers         */
  uint32_t      status;         /* exited? exit status?    */
  proc          lib_one;        /* Two pointers reserved   */
  proc          lib_two;        /* for use by the library  */
  proc          sched_one;      /* Two more for            */
  proc          sched_two;      /* schedulers to use       */
  proc          exited;         /* and one for lwp_wait()  */
} process_st;

#endif