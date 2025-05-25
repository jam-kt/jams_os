#ifndef __BBUF_H__
#define __BBUF_H__

#include <stddef.h>
#include <stdint-gcc.h>


/* buffer state, initialize for each instance of the bounded buffer used */
struct bbuf_st {
    char *buf;      /* pointer to a staticlly allocated array   */
    size_t size;    /* size of the array (indicies)             */
    size_t head;    /* next write index                         */
    size_t tail;    /* next read index                          */
};

void bbuf_init(struct bbuf_st *state, char *buf, size_t size);
int bbuf_try_add(struct bbuf_st *st, char c);
int bbuf_try_consume(struct bbuf_st *st, char *res);


#endif