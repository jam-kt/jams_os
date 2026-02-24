/*******************************************************************************
* File        : roundRobin.c
* Description : Implements a round robin scheduler compliant with the prototypes
*               given in lwp.h. Uses a circular singly linked list
* Dependencies: lwp.h
* Author      : James Torres (jktorres@calpoly.edu)
*******************************************************************************/
#include <string.h>
#include <stdio.h>

#include <kernel/scheduler.h>
#include <kernel/multitask.h>
#include <kernel/interrupts.h>


/* circular buffer pointers and total node count */
static proc Head = NULL;
static proc Tail = NULL;
static proc Current = NULL;
static int Count = 0;

static void rr_admit(proc new);
static void rr_remove(proc victim);
static proc rr_next();
static int rr_qlen(void);


/* allow access to the scheduler through the lwp.h scheduler struct */
static struct scheduler rr_publish = {
    NULL, 
    NULL, 
    rr_admit, 
    rr_remove, 
    rr_next, 
    rr_qlen 
};

scheduler round_robin = &rr_publish;


static void rr_admit(proc new)
{
    int ints_enabled = are_interrupts_enabled();
    if (ints_enabled) {
        CLI();
    }

    /* initialize the circular buffer if it doesn't exist */
    if(!Head) {
        Head = new;
        Tail = new;
        Current = new;
        new->sched_one = new;          /* sched_one used for next node */
    }
    
    /* else, append a new proc at the buffer's tail */
    else {
        new->sched_one = Head;
        Tail->sched_one = new;
        Tail = new;
    }

    Count++;

    if (ints_enabled) {
        STI();
    }
}


static void rr_remove(proc victim)
{
    int ints_enabled = are_interrupts_enabled();
    if (ints_enabled) {
        CLI();
    }

    proc prev = Tail;
    proc curr = Head;

    if(!victim) {
        printk("rr_remove: NULL proc pointer as victim\n");
        goto enable_and_return;
    }

    /* if there's only one proc, remove and reset the buffer */
    if(Count == 1) {
        Head = NULL;
        Tail = NULL;
        Current = NULL;
        Count = 0;

        goto enable_and_return;
    }

    if(Tail == NULL && Head) {
        printk("NULL tail and real head\n");
        printk("halting...\n");
        __asm__("hlt");
    }

    /* search the buffer and remove if found */
    while(curr) {
        if(curr == victim) {
            prev->sched_one = curr->sched_one;

            if(curr == Current) {
                Current = curr->sched_one;
            }
            if(curr == Head) {
                Head = curr->sched_one;
            }
            if(curr == Tail) {
                Tail = prev;
            }

            Count--;

            goto enable_and_return;
        }

        prev = curr;
        curr = curr->sched_one;
        
        /* prevents an infinite loop if the victim is not found */
        if(curr == Head) {
            printk("rr_remove: victim not found\n");
            goto enable_and_return;
        }
    }

enable_and_return:
    if (ints_enabled) {
        STI();
    }

    return;
}


static proc rr_next()
{
    int ints_enabled = are_interrupts_enabled();
    if (ints_enabled) {
        CLI();
    }

    if(!Head) {
        if (ints_enabled) {
            STI();
        }

        return NULL;
    }

    proc upNext = Current;
    Current = Current->sched_one;

    if (ints_enabled) {
        STI();
    }

    return upNext;
}


static int rr_qlen(void)
{
    return Count;
}