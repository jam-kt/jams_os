/*******************************************************************************
* File        : roundRobin.c
* Description : Implements a round robin scheduler compliant with the prototypes
*               given in lwp.h. Uses a circular singly linked list
* Dependencies: lwp.h
* Author      : James Torres (jktorres@calpoly.edu)
*******************************************************************************/
#include "lwp.h"
#include <stdio.h>
#include <stdlib.h>

/* circular buffer pointers and total node count */
static thread Head = NULL;
static thread Tail = NULL;
static thread Current = NULL;
static int Count = 0;

static void rr_admit(thread new);
static void rr_remove(thread victim);
static thread rr_next();
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

scheduler RoundRobin = &rr_publish;


static void rr_admit(thread new) {
    /* initialize the circular buffer if it doesn't exist */
    if(!Head) {
        Head = new;
        Tail = new;
        Current = new;
        new->sched_one = new;          /* sched_one used for next node */
    }
    
    /* else, append a new thread at the buffer's tail */
    else {
        new->sched_one = Head;
        Tail->sched_one = new;
        Tail = new;
    }

    Count++;
}


static void rr_remove(thread victim) {
    thread prev = Tail;
    thread curr = Head;

    if(!victim) {
        fprintf(stderr, "rr_remove: NULL thread pointer as victim\n");
        return;
    }

    /* if there's only one thread, remove and reset the buffer */
    if(Count == 1) {
        Head = NULL;
        Tail = NULL;
        Current = NULL;
        Count = 0;

        return;
    }

    if(Tail == NULL && Head) {
        fprintf(stderr, "NULL tail and real head\n");
        exit(1);
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

            return;
        }

        prev = curr;
        curr = curr->sched_one;
        
        /* prevents an infinite loop if the victim is not found */
        if(curr == Head) {
            fprintf(stderr, "rr_remove: victim not found\n");
            return;
        }
    }
}


static thread rr_next() {
    if(!Head) {
        return NULL;
    }

    thread upNext = Current;
    Current = Current->sched_one;

    return upNext;
}


static int rr_qlen(void) {
    return Count;
}