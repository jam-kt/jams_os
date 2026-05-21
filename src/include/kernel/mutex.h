#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <kernel/multitask.h>

void MUTEX_register_syscalls(void);
void MUTEX_destroy_process_mutexes(process owner);

#endif
