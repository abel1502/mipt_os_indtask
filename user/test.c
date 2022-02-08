/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>

#define LIBC_REMOVE_FPREFIX
#include <inc/libdoom.h>


uint32_t *fb = NULL;


void
umain(int argc, char **argv) {
    // libc_FILE *file = fopen("lorem", "w");

    // libc_fprintf(file, "ABOBA[%s]", "typotype");

    // fclose(file);

    libc_FILE *file = fopen("lorem", "r");

    char buf[8096];

    libc_fread(buf, 1, 8096, file);
    printf("%s\n", buf);
}
