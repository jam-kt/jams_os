#ifndef __MULTITASK_H__
#define __MULTITASK_H__

#include <stddef.h>
#include <stdint-gcc.h>

void yield(void);
void multitask_init();


typedef struct __attribute__ ((aligned(16))) __attribute__ ((packed)) regs {
  unsigned long rax;            /* the sixteen architecturally-visible regs. */
  unsigned long rbx;
  unsigned long rcx;
  unsigned long rdx;
  unsigned long rsi;
  unsigned long rdi;
  unsigned long rbp;            /* base pointer (unknown addr)               */
  unsigned long rsp;            /* stack pointer (grows downwards)           */
  unsigned long r8;
  unsigned long r9;
  unsigned long r10;
  unsigned long r11;
  unsigned long r12;
  unsigned long r13;
  unsigned long r14;
  unsigned long r15;
} rfile;

#define NO_THREAD 0             /* an always invalid thread id */

typedef struct process_st *proc;
typedef struct process_st {
  unsigned long pid;            /* lightweight process id  */
  unsigned long *stack;         /* Base of allocated stack */
  size_t        stacksize;      /* Size of allocated stack */
  rfile         state;          /* saved registers         */
  unsigned int  status;         /* exited? exit status?    */
  proc          lib_one;        /* Two pointers reserved   */
  proc          lib_two;        /* for use by the library  */
  proc          sched_one;      /* Two more for            */
  proc          sched_two;      /* schedulers to use       */
  proc          exited;         /* and one for lwp_wait()  */
} context;

#endif