#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>

#include <kernel/multitask.h>

/* Tuple that describes a scheduler */
typedef struct scheduler {
  void   (*init)(void);            /* initialize any structures     */
  void   (*shutdown)(void);        /* tear down any structures      */
  void   (*admit)(proc new);     /* add a proc to the pool      */
  void   (*remove)(proc victim); /* remove a proc from the pool */
  proc (*next)(void);            /* select a proc to schedule   */
  int    (*qlen)(void);            /* number of ready procs       */
} *scheduler;

extern scheduler round_robin;

#endif