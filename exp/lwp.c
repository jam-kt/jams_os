/*******************************************************************************
* File        : lwp.c
* Description : Assignment 2. Imlpements lightweight user processes that confrom
*               to the prototypes given in lwp.h
* Dependencies: lwp.h, fp.h
* Author      : James Torres (jktorres@calpoly.edu)
*******************************************************************************/
#include "lwp.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>

#define DEFAULT_STACK_BYTES (8 * 1024 * 1024)      /* binary 8MB */
#define x86_64_ALIGNMENT 16

extern scheduler RoundRobin;

static scheduler Scheduler = NULL;
static tid_t CountID       = NO_THREAD;    /* to give unique IDs */
static thread Current      = NULL;   

/* These define 3 separate queues where threads can be sorted */ 
static thread LiveHead     = NULL;         /* running threads */
static thread LiveTail     = NULL;
static thread ZombHead     = NULL;         /* zombie threads  */
static thread ZombTail     = NULL;
static thread WaitHead     = NULL;         /* waiting threads */
static thread WaitTail     = NULL;


static void lwp_wrap(lwpfun fun, void *arg);
static size_t stack_size();
static unsigned long round_alignment(unsigned long sp);
static void lwp_enqueue(thread newThread, thread *head, thread *tail);
static thread lwp_dequeue(thread *head, thread *tail);
static void lwp_remove(thread victim, thread *head, thread *tail);
static thread find_tid(thread head, tid_t tid);


tid_t lwp_create(lwpfun fun, void *arg) {
    if(!Scheduler) {
        Scheduler = RoundRobin;
    }

    /* create and allocate a zero-initialized thread context */
    thread newThread = calloc(1, sizeof(context));
    if(!newThread) {
        perror("lwp_create: calloc for a thread context failed");
        return NO_THREAD;
    }

    /* create and allocate a stack for the thread */
    size_t stackSize = stack_size();
    void *stack = mmap(NULL, stackSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if(stack == MAP_FAILED) {
        perror("lwp_create: stack creation failed\n");
        free(newThread);
        return NO_THREAD;
    }

    CountID++;

    /* set up context for the new thread */
    newThread->tid          = CountID;
    newThread->stack        = (unsigned long *)stack;
    newThread->stacksize    = stackSize;
    newThread->exited       = NULL;
    newThread->status       = MKTERMSTAT(LWP_LIVE, 0);
    newThread->state.fxsave = FPU_INIT;
    newThread->state.rdi    = (unsigned long)fun;
    newThread->state.rsi    = (unsigned long)arg;

    /* set up the stack for the new thread */
    unsigned long *stackTop = (unsigned long *)((char *)stack + stackSize);
    stackTop = (unsigned long*)round_alignment((unsigned long)stackTop);

    *--stackTop = 0;                            /* anything works */
    *--stackTop = (unsigned long)lwp_wrap;      /* return address to wrapper */
    *--stackTop = 0;                            /* anything works */

    newThread->state.rbp = (unsigned long)stackTop;
    newThread->state.rsp = (unsigned long)stackTop;
    
    /* admit to the scheduler */
    lwp_enqueue(newThread, &LiveHead, &LiveTail);
    Scheduler->admit(newThread);

    return newThread->tid;
}


void lwp_start() {
    if(!Scheduler) {
        Scheduler = RoundRobin;
    }

    /* create and allocate a zero-initialized thread context */
    thread newThread = calloc(1, sizeof(context));
    if(!newThread) {
        perror("lwp_start: calloc for a thread context failed");
        exit(1);
    }

    /* set up context for the main thread */
    CountID++;
    newThread->tid    = CountID;
    newThread->stack  = NULL;
    newThread->exited = NULL;
    newThread->status = MKTERMSTAT(LWP_LIVE, 0);

    Current = newThread;

    /* admit to the scheduler and begin threading */
    lwp_enqueue(newThread, &LiveHead, &LiveTail);
    Scheduler->admit(newThread);
    lwp_yield();
}


void lwp_yield() {
    thread next = Scheduler->next();
    thread old = Current;

    /* check if there are any runnable threads left */
    if(!next) {
        exit(Current->status);
    }

    /* context switch */
    Current = next;
    swap_rfiles(&(old->state), &(next->state));
}


void lwp_exit(int status) {
    /* remove the thread from the scheduler and turn it into a zombie */
    Scheduler->remove(Current);

    /* If there is a waiting thread associate to it then reschedule it */
    if(WaitHead) {
        thread waiting = lwp_dequeue(&WaitHead, &WaitTail);
        waiting->exited = Current;
        Scheduler->admit(waiting);
        lwp_enqueue(waiting, &LiveHead, &LiveTail);
    }

    /* no waiting threads, so add self to a queue of zombies */
    else {
        lwp_remove(Current, &LiveHead, &LiveTail);
        lwp_enqueue(Current, &ZombHead, &ZombTail);
    }

    Current->status = MKTERMSTAT(LWP_TERM, status);

    lwp_yield();
}


tid_t lwp_wait(int *status) {
    thread zombie = lwp_dequeue(&ZombHead, &ZombTail);
    /* check if there are any zombie threads to clean */
    if(!zombie) {
        /* no runnable threads? */
        if(Scheduler->qlen() <= 1) {
            return NO_THREAD;
        }

        /* deschedule self and block */
        else {
            /* move to waiting queue */
            lwp_remove(Current, &LiveHead, &LiveTail);
            lwp_enqueue(Current, &WaitHead, &WaitTail);
            Scheduler->remove(Current);
            lwp_yield();

            /* if control reaches here, a terminated thread has called us */
            zombie = Current->exited;
            lwp_enqueue(Current, &LiveHead, &LiveTail);
        }
    }

    /* When control reaches here a thread is ready to be cleaned up */
    tid_t zombTid = zombie->tid;

    if(status) {
        *status = zombie->status;
    }

    /* unmap the stack of the user created threads */
    if(zombie->stack) {
        if(munmap(zombie->stack, zombie->stacksize) == -1) {
            perror("lwp_exit: munmap failed\n");
            exit(1);
        }
    }

    free(zombie);

    return zombTid;
}


tid_t lwp_gettid() {
    if(!Current) {
        return NO_THREAD;
    }

    return Current->tid;
}


thread tid2thread(tid_t tid) {
    /* iterate over each list separetly. Little crude, but the lack of a global
    list makes it necessary */
    thread found = find_tid(LiveHead, tid);
    if(found) {
        return found;
    }

    found = find_tid(WaitHead, tid);
    if(found) {
        return found;
    }

    return find_tid(ZombHead, tid);
}


void lwp_set_scheduler(scheduler sched) {
    /* ensure a current scheduler exists */
    if(!Scheduler) {
        Scheduler = RoundRobin;
    }

    /* if passed scheduler is NULL, use round-robin */
    if(!sched) {
        sched = RoundRobin;
    }

    if(sched->init) {
        sched->init();
    }

    /* prevents an infinite loop of transfering threads to oneself */
    if(sched == Scheduler) {
        return;
    }
    
    /* transfer threads to the new scheduler */
    thread next = Scheduler->next();
    while(next) {
        Scheduler->remove(next);
        sched->admit(next);

        next = Scheduler->next();
    }

    if(Scheduler->shutdown) {
        Scheduler->shutdown();
    }

    Scheduler = sched;
}


scheduler lwp_get_scheduler(void) {
    return Scheduler;
}


static void lwp_wrap(lwpfun fun, void *arg) {
    int rval = fun(arg);
    lwp_exit(rval);
}


/* calculate the size of a thread's stack based on system limits */
static size_t stack_size() {
    size_t stackSize;

    long pageSize = sysconf(_SC_PAGE_SIZE);
    if(pageSize == -1) {
        perror("stack_size: page size error, using default size\n");
        return (size_t)DEFAULT_STACK_BYTES;
    }

    struct rlimit rlim;
    if(getrlimit(RLIMIT_STACK, &rlim) == -1) {
        perror("stack_size: stack limit error, using default size\n");
        return (size_t)DEFAULT_STACK_BYTES;
    }

    /* use a default size if there is no limit */
    if(rlim.rlim_cur == RLIM_INFINITY || rlim.rlim_cur == 0) {
        stackSize = (size_t)DEFAULT_STACK_BYTES;
    }

    else {
        stackSize = (size_t)rlim.rlim_cur;
    }

    /* round the stack size to the nearest multiple of the page size */
    stackSize = (stackSize - 1) / (size_t)pageSize + 1;
    stackSize *= (size_t)pageSize;
    
    return stackSize;
}


/* round a pointer down for alignment, used for the top of the stack */
static unsigned long round_alignment(unsigned long ptr) {
    return ptr & ~x86_64_ALIGNMENT;
}


/*
* Adds a thread into the queue defined by the head and tail nodes.
* Adding a thread into any one of the queues will overwrite references to 
* previous queue neigbors. A thread can only live in one queue at a time
*
* thread newThread: The thread to be added into a linked list FIFO 
* thread *head    : Ptr to the head of the destination FIFO
* thread *tail    : Ptr to the tail of the destination FIFO
*/
static void lwp_enqueue(thread newThread, thread *head, thread *tail) {
    newThread->lib_one = NULL;

    /* initialze the FIFO if it doesn't exist*/
    if(!*head) {
        *head = newThread;
        *tail = newThread;
    }

    else {
        (*tail)->lib_one = newThread;
        *tail = newThread;       
    }
}


/* 
* Remove/return the first node in the queue defined by the head and tail nodes
* thread *head: Ptr to the head of the target FIFO
* thread *tail: Ptr to the tail of the target FIFO
*/
static thread lwp_dequeue(thread *head, thread *tail) {
    /* check if the queue is empty */
    if(!*head) {
        //fprintf(stderr, "lwp_dequeue: dequeue from an empty queue\n");
        return NULL;
    }

    thread temp = *head;

    /* check if the queue only has one node */
    if(!temp->lib_one) {
        *head = NULL;
        *tail = NULL;
    }

    else {
        *head = temp->lib_one;
    }

    temp->lib_one = NULL;
    return temp;
}


/* 
* Remove the victim from the queue defined by the head and tail nodes
* thread *head: Ptr to the head of the target FIFO
* thread *tail: Ptr to the tail of the target FIFO
*/
static void lwp_remove(thread victim, thread *head, thread *tail) {
    if (!victim || !*head) {
        fprintf(stderr, "lwp_remove: NULL victim or head\n");
        return;  
    }

    /* check if the victim is the first (or only) node */
    if (*head == victim) {
        *head = victim->lib_one;

        if(!*head) {
            *tail = NULL;
        }

        victim->lib_one = NULL;
        return;
    }

    /* iterate through the queue to find the victim */
    thread prev = *head;
    while (prev->lib_one) {
        if (prev->lib_one == victim) {
            prev->lib_one = victim->lib_one;
            
            /* check if the victim was the tail */
            if (!victim->lib_one) {
                *tail = prev;
            }
            
            victim->lib_one = NULL;
            return;
        }

        prev = prev->lib_one;
    }

    //fprintf(stderr, "lwp_remove: victim not found\n");
}


/* 
* iterate over the given linked lists to find a thread given a tid 
* thread head: Head of the target list to search
* tid_t tid  : The tid to look for
*/
static thread find_tid(thread head, tid_t tid) {
    thread curr = head;
    while (curr) {
        if (curr->tid == tid) {
            return curr;
        }
        curr = curr->lib_one;
    }

    return NULL;
}