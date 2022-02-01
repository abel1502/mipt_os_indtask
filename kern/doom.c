#include <inc/lib.h>


FILE stderr = (FILE) 1;
FILE stdout = (FILE) 1;


void free(void *ptr) {
    // printf("hi");
}

void *malloc(int cnt) {
    return 0;
}

int abs(int x) {
    return x > 0 ? x : -x;
}

int atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + *s - '0';
    }
    return n;
}
