#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <stddef.h>
#include <stdint-gcc.h>

#define NUM_SYSCALLS 256

/* callback function */
typedef void (*sys_t) (uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
                        uint64_t a5, uint64_t a6);

/* an entry into the Syscall table */
struct syscall_entry {
    sys_t handler;  /* function pointer to a specific syscall  */
};


void syscall_init();
void register_syscall(int sys_num, sys_t handler);


/* keep all syscall numbers here so we dont reuse (is this a good idea?) 
 * Mirror this style and have all interrupt vectors in one place too? */

#define SYS_YIELD_NUM 0
#define SYS_KEXIT_NUM 1

#endif