/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>

/* User-level fork with copy-on-write.
 * Create a child.
 * Lazily copy our address space and page fault handler setup to the child.
 * Then mark the child as runnable and return.
 *
 * Returns: child's envid to the parent, 0 to the child, < 0 on error.
 * It is also OK to panic on error.
 *
 * Hint:
 *   Use sys_map_region, it can perform address space copying in one call
 *   Remember to fix "thisenv" in the child process.
 */
envid_t
fork(void) {
    // LAB 9: Your code here DONE

    envid_t envid = 0;
    int res = 0;

    /* Allocate a new child environment.
     * The kernel will initialize it with a copy of our register state,
     * so that the child will appear to have called sys_exofork() too -
     * except that in the child, this "fake" call to sys_exofork()
     * will return 0 instead of the envid of the child. */
    envid = sys_exofork();
    if (envid < 0)
        panic("sys_exofork: %i", envid);
    if (envid == 0) {
        /* We're the child.
         * The copied value of the global variable 'thisenv'
         * is no longer valid (it refers to the parent!).
         * Fix it and return 0. */
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }

    /* We're the parent.
     * Eagerly lazily copy our entire address space into the child. */
    if ((res = sys_map_region(0, NULL, envid, NULL, MAX_USER_ADDRESS,
                              PROT_ALL | PROT_LAZY | PROT_COMBINE)) < 0) {
        panic("sys_map_region: %i", res);
    }

    if ((res = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)) < 0) {
        panic("sys_env_set_pgfault_upcall: %i", res);
    }

    /* Start the child environment running */
    if ((res = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        panic("sys_env_set_status: %i", res);

    return envid;
}

envid_t
sfork() {
    panic("sfork() is not implemented");
}
