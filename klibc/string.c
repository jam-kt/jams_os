#include <stddef.h>

#include <string.h>

void *memset(void *s, int c, size_t n) 
{
    unsigned char *buf = (unsigned char *)s;

    for (size_t i = 0; i < n; i++) {
        buf[i] = (unsigned char)c;
    }

    return s;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n) 
{
    unsigned char *dbuf = (unsigned char *)dest;
    const unsigned char *sbuf = (const unsigned char *)src;

    for (size_t i = 0; i < n; i++) {
        dbuf[i] = sbuf[i];
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) 
{
	unsigned char *dbuf = (unsigned char *)dest;
	const unsigned char *sbuf = (const unsigned char *)src;

    /*
     * memory areas may overlap so we must copy backwards if the dest buffer 
     * is in higher memory than the src
     */
	if (dbuf < sbuf) {
		for (size_t i = 0; i < n; i++) {
            dbuf[i] = sbuf[i]; 
        }
	} else {
		for (size_t i = n; i > 0; i--) {
            dbuf[i-1] = sbuf[i-1]; 
        }	
	}

	return dest;
}

size_t strlen(const char *s)
{
    size_t len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    for ( ; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;

    while ((*str1 != '\0') && (*str1 == *str2)) {
        str1++;
        str2++;
    }

    return *str1 - *str2;
}

char *strchr(const char *s, int c)
{
    unsigned char *sptr = (unsigned char *)s;
    unsigned char uc = (unsigned char)c;
    
    while ((*sptr != uc) && (*sptr != '\0')) {
        sptr++;
    }

    if (*sptr == uc) {
        return (char *)sptr;
    }

    return NULL;
}