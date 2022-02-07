/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>

#define LIBC_REMOVE_FPREFIX
#include <inc/libdoom.h>


void
umain(int argc, char **argv) {
    const char *buf = "abc 123 xyz\0";
    int n = 0;
    char *s1 = malloc(10);
    char *s2 = malloc(10);

    sscanf(buf, "%s %d %s", s1, &n, s2);
    printf("%s|%d|%s\n", s1, n, s2);

    // for (int i = 0; i < 10000; ++i) {
    //     char *s1 = malloc(10);
    //     char *s2 = malloc(10);
    //     char *s3 = malloc(10);

    //     memcpy(s1, "123", 3);
    //     memcpy(s2, "abc", 3);
    //     memcpy(s3, "xyz", 3);

    //     // printf("%s | %s | %s\n", s1, s2, s3);

    //     char *ss1 = strdup(s1); free(s1);
    //     char *ss2 = strdup(s2); free(s2);
    //     char *ss3 = strdup(s3); free(s3);

    //     printf("%d) %s | %s | %s\n", i, ss1, ss2, ss3);

    //     free(ss1);
    //     free(ss2);
    //     free(ss3);
    // }
}
