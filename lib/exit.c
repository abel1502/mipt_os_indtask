
#include <inc/lib.h>

void
exit(void) {
    // LAB 10: Uncomment DONE
    close_all();
    sys_env_destroy(0);
}
