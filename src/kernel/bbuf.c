#include <stddef.h>
#include <stdint-gcc.h>

#include <stdio.h>

#include <kernel/interrupts.h>
#include <kernel/bbuf.h>


void bbuf_init(struct bbuf_st *state, char *buf, size_t size)
{
    if (size < 2) {
        printk("Bounded Buffer size must be at least 2\n");
        return;
    }

    state->buf = buf;
    state->size = size;
    state->head = 0;
    state->tail = 0;
}

/* add to the buffer. Returns 0 on success, else -1 */
int bbuf_try_add(struct bbuf_st *st, char c)
{
    int enable_ints = 0;
    int ret = 0;

    if (are_interrupts_enabled()) {
        enable_ints = 1;
        CLI();
    }

    /* check if there is room in the buffer, add data */
    size_t next = (st->head + 1) % st->size;
    if (next == st->tail) {     
        ret = -1;
    } else {
        st->buf[st->head] = c;
        st->head = next;
    }

    if (enable_ints) {
        STI();
    }

    return ret;
}

/*
 * Attempts to consume from the BB.
 * - st: the bbuf state
 * - res: on success the consumed char will be placed in *res
 * Return: returns 0 for a successful consume, -1 if the buffer was empty 
 */
int bbuf_try_consume(struct bbuf_st *st, char *res)
{
    int ret = 0;

    if (res == NULL) {
        printk("bbuf_try_consume: NULL for res\n");
        return -1;
    }

    int enable_ints;
    if (are_interrupts_enabled()) {
        enable_ints = 1;
        CLI();
    }

    if (st->head == st->tail) {             /* check if buffer is empty     */
        ret = -1;
    } else {                                /* buffer was not empty, read   */
        *res = st->buf[st->tail];           
        st->tail = (st->tail + 1) % st->size;        
    }

    if (enable_ints) {
        STI();
    }

    return ret;
}