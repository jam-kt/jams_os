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
process PROC_main_process(void);
void PROC_init_process(process p, process parent, uint64_t cr3);
void PROC_init_thread(proc t, process owner);
void PROC_link_process_child(process parent, process child);
void PROC_unlink_process_child(process parent, process child);
process PROC_find_zombie_child(process parent);
void PROC_reparent_children(process exiting);
void PROC_link_thread(process owner, proc t);
void PROC_unlink_thread(process owner, proc t);
proc PROC_find_thread(process owner, uint64_t tid);
int PROC_setup_uthread(proc new_proc, kproc_t entry_point, void *arg,
                       uint64_t user_rsp);
void PROC_free_thread(proc t);
void PROC_free_process(process p);
int PROC_exit_current_thread(int status, int whole_process);
int PROC_copy_user_string(char *dst, const char *src, size_t dst_size);

#endif
