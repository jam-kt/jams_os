#ifndef __MULTITASK_INTERNAL_H__
#define __MULTITASK_INTERNAL_H__

#include <stdint-gcc.h>
#include <kernel/multitask.h>

extern proc curr_proc;
extern proc next_proc;

void PROC_register_syscalls(void);
proc PROC_main_proc(void);
void PROC_admit(proc p);
void PROC_remove(proc p);
void PROC_init_fields(proc p, proc parent);
void PROC_link_child(proc parent, proc child);
void PROC_unlink_child(proc parent, proc child);
proc PROC_find_zombie_child(proc parent);
void PROC_reparent_children(proc exiting);
int PROC_copy_user_string(char *dst, const char *src, size_t dst_size);

#endif
