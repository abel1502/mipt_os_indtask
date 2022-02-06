#include <inc/libdoom.h>
#include <inc/lib.h>


int libc_abs(int x) {
    return (x >= 0) ? x : -x;
}

int libc_atoi(const char *s) {
    int rc = 0;

    char sign = '+';

    /* TODO: In other than "C" locale, additional patterns may be defined     */
    while ( *s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\r' )
    {
        ++s;
    }

    if ( *s == '+' )
    {
        ++s;
    }
    else if ( *s == '-' )
    {
        sign = *( s++ );
    }

    /* TODO: Earlier version was missing tolower() but was not caught by tests */
    while ( *s >= '0' && *s <= '9' )
    {
        rc = rc * 10 + (*s - '0');
        s++;
    }

    return ( sign == '+' ) ? rc : -rc;
}


char heap[2 * 1024 * 1024]; // 2MB heap

void *HEAP_HEAD = heap; // current free position in heap

void libc_free(void *ptr) {
    return;
}

void *libc_malloc(int cnt) {
    void *ret = HEAP_HEAD;

    HEAP_HEAD += cnt;

    return ret;
}

void *libc_realloc(void *ptr, size_t newsize) {
    void *new_ptr = libc_malloc(newsize);
    if (!new_ptr) {
        return NULL;
    }

    // memcpy(new_ptr, ptr, ???);
    libc_free(ptr);
    
    return new_ptr;
}

void *libc_calloc(size_t num, size_t size) {
    return libc_malloc(num * size);
}
