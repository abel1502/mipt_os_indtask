/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>
#include <inc/libdoom.h>

// extern char libc_heap;
extern char end;

void
umain(int argc, char **argv) {
    char *str = libc_malloc(21);
    for (int i = 0; i < 20; ++i) {
        str[i] = 'a' + i;
    }

    cprintf("%s\n", str);

    char* str2 = libc_strdup(str);
    cprintf("%s\n", str2);

    libc_puts("{[");
    libc_putchar(']');
    libc_putchar('}');

    libc_printf(" -> %s <-", str2);
}
