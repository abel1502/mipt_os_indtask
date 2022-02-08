/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>

#define LIBC_REMOVE_FPREFIX
#include <inc/libdoom.h>


uint32_t *fb = NULL;


void
umain(int argc, char **argv) {
    // const char *buf = "abc 123 xyz\0";
    // int n = 0;
    // char *s1 = malloc(10);
    // char *s2 = malloc(10);

    // sscanf(buf, "%s %d %s", s1, &n, s2);
    // printf("%s|%d|%s\n", s1, n, s2);

    int res = sys_virtiogpu_init(&fb);
    assert(res >= 0);
    printf("fb = %p\n", fb);
    
}
