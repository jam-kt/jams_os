#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stddef.h>
#include <stdint-gcc.h>

#define NUM_SYSCALLS 256

struct syscall_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t vector;
    uint64_t err;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

/* callback function, returns uint64_t as a generic data type */
typedef uint64_t (*sys_t) (struct syscall_frame *frame);

/* an entry into the syscall table */
struct syscall_entry {
    sys_t handler;  /* function pointer to a specific syscall  */
};


void syscall_init();
void register_syscall(int sys_num, sys_t handler);


/* keep all syscall numbers here so we dont reuse (is this a good idea?) 
 * Mirror this style and have all interrupt vectors in one place too? */

#define SYS_YIELD_NUM 0
#define SYS_EXIT_NUM  1
#define SYS_KEXIT_NUM SYS_EXIT_NUM
#define SYS_GETC_NUM  2
#define SYS_PUTC_NUM  3
#define SYS_WAIT_NUM  4
#define SYS_EXEC_NUM  5
#define SYS_FORK_NUM  6

#endif
