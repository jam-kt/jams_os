/*
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * Taken from https://github.com/Zunawe/md5-c and modified to work within this
 * kernel.
 */

#ifndef MD5_H
#define MD5_H

#include <stdint-gcc.h>
#include <stddef.h>

#include <kernel/vfs.h>

typedef struct{
    uint64_t size;        // Size of input in bytes
    uint32_t buffer[4];   // Current accumulation of hash
    uint8_t input[64];    // Input to be used in the next step
    uint8_t digest[16];   // Result of algorithm
}MD5Context;

void md5Init(MD5Context *ctx);
void md5Update(MD5Context *ctx, uint8_t *input, size_t input_len);
void md5Finalize(MD5Context *ctx);
void md5Step(uint32_t *buffer, uint32_t *input);

void md5String(char *input, uint8_t *result);

/* Modified to take your kernel's file structure */
void md5File(struct file *file, uint8_t *result);

#endif