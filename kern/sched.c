#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/monitor.h>


struct Taskstate cpu_ts;
_Noreturn void sched_halt(void);

/* Choose a user environment to run and run it */
_Noreturn void
sched_yield(void) {
    /* Implement simple round-robin scheduling.
     *
     * Search through 'envs' for an ENV_RUNNABLE environment in
     * circular fashion starting just after the env was
     * last running.  Switch to the first such environment found.
     *
     * If no envs are runnable, but the environment previously
     * running is still ENV_RUNNING, it's okay to
     * choose that environment.
     *
     * If there are no runnable environments,
     * simply drop through to the code
     * below to halt the cpu */

    struct Env *env_initial = curenv ? curenv : envs + NENV - 1;
    struct Env *env_cur = env_initial + 1;

    /*const char *state[] = {"FREE", "DYING", "RUNNABLE", "RUNNING", "NOT_RUNNABLE"};

    for (unsigned i = 0; i < NENV; ++i) {
        if (envs[i].env_status == ENV_FREE) {
            continue;
        }
        
        cprintf("> %08X %s\n",  envs[i].env_id, state[envs[i].env_status]);
    }*/

    do {
        if (env_cur >= envs + NENV) {
            env_cur = envs;
        }

        //if (env_cur->env_status != ENV_FREE)
        //    cprintf(">>> %08X %s\n",  env_cur->env_id, state[env_cur->env_status]);

        if (env_cur->env_status == ENV_RUNNABLE) {
            break;
        }

        env_cur++;
    } while (env_cur != env_initial);

    if (env_cur->env_status == ENV_RUNNABLE || env_cur->env_status == ENV_RUNNING) {
        env_run(env_cur);
    }

    assert(env_cur == env_initial);

    /* No runnable environments,
        * so just halt the cpu */
    cprintf("Halt\n");
    sched_halt();
}

/* Halt this CPU when there is nothing to do. Wait until the
 * timer interrupt wakes it up. This function never returns */
_Noreturn void
sched_halt(void) {

    /* For debugging and testing purposes, if there are no runnable
     * environments in the system, then drop into the kernel monitor */
    int i;
    for (i = 0; i < NENV; i++)
        if (envs[i].env_status == ENV_RUNNABLE ||
            envs[i].env_status == ENV_RUNNING) break;
    if (i == NENV) {
        cprintf("No runnable environments in the system!\n");
        for (;;) monitor(NULL);
    }

    /* Mark that no environment is running on CPU */
    curenv = NULL;

    /* Reset stack pointer, enable interrupts and then halt */
    asm volatile(
            "movq $0, %%rbp\n"
            "movq %0, %%rsp\n"
            "pushq $0\n"
            "pushq $0\n"
            "sti\n"
            "hlt\n" ::"a"(cpu_ts.ts_rsp0));

    /* Unreachable */
    for (;;)
        ;
}
