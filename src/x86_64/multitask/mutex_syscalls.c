#include <stddef.h>
#include <stdint-gcc.h>

#include <kernel/interrupts.h>
#include <kernel/kmalloc.h>
#include <kernel/multitask.h>
#include <kernel/mutex.h>
#include <kernel/syscall.h>

#include "multitask_internal.h"

typedef struct kernel_mutex_st {
    uint64_t id;
    process owner_process;
    proc owner_thread;
    proc_queue waiters;
    struct kernel_mutex_st *next;
} kernel_mutex_st;

/* kept as a linked list type structure */
static kernel_mutex_st *mutex_list = NULL;
static uint64_t next_mutex_id = 1;

static uint64_t syscall_mutex_create(struct syscall_frame *frame);
static uint64_t syscall_mutex_lock(struct syscall_frame *frame);
static uint64_t syscall_mutex_unlock(struct syscall_frame *frame);
static uint64_t syscall_mutex_destroy(struct syscall_frame *frame);
static kernel_mutex_st *find_mutex(process owner, uint64_t id);
static void unlink_mutex(kernel_mutex_st *mutex);
static void free_mutex(kernel_mutex_st *mutex);

void MUTEX_register_syscalls(void)
{
    register_syscall(SYS_MUTEX_CREATE_NUM, syscall_mutex_create);
    register_syscall(SYS_MUTEX_LOCK_NUM, syscall_mutex_lock);
    register_syscall(SYS_MUTEX_UNLOCK_NUM, syscall_mutex_unlock);
    register_syscall(SYS_MUTEX_DESTROY_NUM, syscall_mutex_destroy);
}

static uint64_t syscall_mutex_create(struct syscall_frame *frame)
{
    kernel_mutex_st *mutex;

    (void)frame;

    mutex = kmalloc(sizeof(*mutex));
    if (!mutex) {
        return 0;
    }

    mutex->id = next_mutex_id++;
    mutex->owner_process = curr_proc->owner;
    mutex->owner_thread = NULL;
    PROC_init_queue(&mutex->waiters);
    mutex->next = mutex_list;
    mutex_list = mutex;

    return mutex->id;
}

static uint64_t syscall_mutex_lock(struct syscall_frame *frame)
{
    uint64_t id = frame->rdi;
    kernel_mutex_st *mutex;

    if (!curr_proc || !curr_proc->owner || id == 0) {
        return (uint64_t)-1;
    }

    while (1) {
        CLI();
        mutex = find_mutex(curr_proc->owner, id);
        if (!mutex) {
            STI();
            return (uint64_t)-1;
        }

        /* mutex is free */
        if (!mutex->owner_thread) {
            mutex->owner_thread = curr_proc;
            STI();
            return 0;
        }

        /* we already own the mutex */
        if (mutex->owner_thread == curr_proc) {
            STI();
            return (uint64_t)-1;
        }

        /* mutex is not free, block and join wait queue */
        PROC_block_on(&mutex->waiters, 1);

        CLI();
        if (mutex->owner_thread == curr_proc) {
            STI();
            return 0;
        }
    }
}

static uint64_t syscall_mutex_unlock(struct syscall_frame *frame)
{
    uint64_t id = frame->rdi;
    kernel_mutex_st *mutex;
    proc next_owner;

    if (!curr_proc || !curr_proc->owner || id == 0) {
        return (uint64_t)-1;
    }

    CLI();
    mutex = find_mutex(curr_proc->owner, id);
    if (!mutex || mutex->owner_thread != curr_proc) {
        STI();
        return (uint64_t)-1;
    }

    next_owner = PROC_unblock_one(&mutex->waiters);
    mutex->owner_thread = next_owner;
    STI();

    return 0;
}

static uint64_t syscall_mutex_destroy(struct syscall_frame *frame)
{
    uint64_t id = frame->rdi;
    kernel_mutex_st *mutex;

    if (!curr_proc || !curr_proc->owner || id == 0) {
        return (uint64_t)-1;
    }

    CLI();
    mutex = find_mutex(curr_proc->owner, id);
    if (!mutex || mutex->owner_thread || mutex->waiters.head) {
        STI();
        return (uint64_t)-1;
    }

    unlink_mutex(mutex);
    STI();
    free_mutex(mutex);

    return 0;
}

void MUTEX_destroy_process_mutexes(process owner)
{
    kernel_mutex_st *curr;
    kernel_mutex_st *next;
    int ints_enabled;

    if (!owner) {
        return;
    }

    ints_enabled = are_interrupts_enabled();
    if (ints_enabled) {
        CLI();
    }

    curr = mutex_list;
    while (curr) {
        next = curr->next;
        if (curr->owner_process == owner) {
            unlink_mutex(curr);
            PROC_unblock_all(&curr->waiters);
            free_mutex(curr);
        }
        curr = next;
    }

    if (ints_enabled) {
        STI();
    }
}

static kernel_mutex_st *find_mutex(process owner, uint64_t id)
{
    kernel_mutex_st *mutex = mutex_list;

    while (mutex) {
        if (mutex->id == id && mutex->owner_process == owner) {
            return mutex;
        }
        mutex = mutex->next;
    }

    return NULL;
}

static void unlink_mutex(kernel_mutex_st *mutex)
{
    kernel_mutex_st *prev = NULL;
    kernel_mutex_st *curr = mutex_list;

    while (curr) {
        if (curr == mutex) {
            if (prev) {
                prev->next = curr->next;
            } else {
                mutex_list = curr->next;
            }
            curr->next = NULL;
            return;
        }

        prev = curr;
        curr = curr->next;
    }
}

static void free_mutex(kernel_mutex_st *mutex)
{
    kfree(mutex);
}
