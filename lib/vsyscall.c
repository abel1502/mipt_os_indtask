#include <inc/vsyscall.h>
#include <inc/lib.h>

// Was `static inline uint64_t`
static inline int
vsyscall(int num) {
    // LAB 12: Your code here DONE
    if (num < 0 || num >= NVSYSCALLS) {
        return -E_NO_SYS;
    }

    return vsys[num];
}

int
vsys_gettime(void) {
    return vsyscall(VSYS_gettime);
}
