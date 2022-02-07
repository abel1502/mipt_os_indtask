/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>
#include <inc/libdoom.h>


void
umain(int argc, char **argv) {
    for (int i = 0; i < 10000; ++i) {
        char *s1 = libc_malloc(10);
        char *s2 = libc_malloc(10);
        char *s3 = libc_malloc(10);

        memcpy(s1, "123", 3);
        memcpy(s2, "abc", 3);
        memcpy(s3, "xyz", 3);

        // libc_printf("%s | %s | %s\n", s1, s2, s3);

        char *ss1 = libc_strdup(s1); libc_free(s1);
        char *ss2 = libc_strdup(s2); libc_free(s2);
        char *ss3 = libc_strdup(s3); libc_free(s3);

        libc_printf("%d) %s | %s | %s\n", i, ss1, ss2, ss3);

        libc_free(ss1);
        libc_free(ss2);
        libc_free(ss3);
    }
}
