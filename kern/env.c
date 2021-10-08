/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/monitor.h>
#include <kern/sched.h>
#include <kern/kdebug.h>
#include <kern/macro.h>
#include <kern/traceopt.h>

/* Currently active environment */
struct Env *curenv = NULL;

#ifdef CONFIG_KSPACE
/* All environments */
struct Env env_array[NENV];
struct Env *envs = env_array;
#else
/* All environments */
struct Env *envs = NULL;
#endif

/* Free environment list
 * (linked by Env->env_link) */
static struct Env *env_free_list;


/* NOTE: Should be at least LOGNENV */
#define ENVGENSHIFT 12

/* Converts an envid to an env pointer.
 * If checkperm is set, the specified environment must be either the
 * current environment or an immediate child of the current environment.
 *
 * RETURNS
 *     0 on success, -E_BAD_ENV on error.
 *   On success, sets *env_store to the environment.
 *   On error, sets *env_store to NULL. */
int
envid2env(envid_t envid, struct Env **env_store, bool need_check_perm) {
    struct Env *env;

    /* If envid is zero, return the current environment. */
    if (!envid) {
        *env_store = curenv;
        return 0;
    }

    /* Look up the Env structure via the index part of the envid,
     * then check the env_id field in that struct Env
     * to ensure that the envid is not stale
     * (i.e., does not refer to a _previous_ environment
     * that used the same slot in the envs[] array). */
    env = &envs[ENVX(envid)];
    if (env->env_status == ENV_FREE || env->env_id != envid) {
        *env_store = NULL;
        return -E_BAD_ENV;
    }

    /* Check that the calling environment has legitimate permission
     * to manipulate the specified environment.
     * If checkperm is set, the specified environment
     * must be either the current environment
     * or an immediate child of the current environment. */
    if (need_check_perm && env != curenv && env->env_parent_id != curenv->env_id) {
        *env_store = NULL;
        return -E_BAD_ENV;
    }

    *env_store = env;
    return 0;
}

/* Mark all environments in 'envs' as free, set their env_ids to 0,
 * and insert them into the env_free_list.
 * Make sure the environments are in the free list in the same order
 * they are in the envs array (i.e., so that the first call to
 * env_alloc() returns envs[0]).
 */
void
env_init(void) {
    env_free_list = envs;

    memset(envs, 0, NENV * sizeof(struct Env));

    for (unsigned i = 0; i < NENV; ++i) {
        envs[i].env_id = 0;
        envs[i].env_status = ENV_FREE;
        envs[i].env_link = envs + i + 1;
    }

    envs[NENV - 1].env_link = NULL;
}

/* Allocates and initializes a new environment.
 * On success, the new environment is stored in *newenv_store.
 *
 * Returns
 *     0 on success, < 0 on failure.
 * Errors
 *    -E_NO_FREE_ENV if all NENVS environments are allocated
 *    -E_NO_MEM on memory exhaustion
 */
int
env_alloc(struct Env **newenv_store, envid_t parent_id, enum EnvType type) {
    struct Env *env;
    if (!(env = env_free_list))
        return -E_NO_FREE_ENV;

    /* Generate an env_id for this environment */
    int32_t generation = (env->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
    /* Don't create a negative env_id */
    if (generation <= 0) generation = 1 << ENVGENSHIFT;
    env->env_id = generation | (env - envs);

    /* Set the basic status variables */
    env->env_parent_id = parent_id;
#ifdef CONFIG_KSPACE
    env->env_type = ENV_TYPE_KERNEL;
#else
    env->env_type = type;
#endif
    env->env_status = ENV_RUNNABLE;
    env->env_runs = 0;

    /* Clear out all the saved register state,
     * to prevent the register values
     * of a prior environment inhabiting this Env structure
     * from "leaking" into our new environment */
    memset(&env->env_tf, 0, sizeof(env->env_tf));

    /* Set up appropriate initial values for the segment registers.
     * GD_UD is the user data (KD - kernel data) segment selector in the GDT, and
     * GD_UT is the user text (KT - kernel text) segment selector (see inc/memlayout.h).
     * The low 2 bits of each segment register contains the
     * Requestor Privilege Level (RPL); 3 means user mode, 0 - kernel mode.  When
     * we switch privilege levels, the hardware does various
     * checks involving the RPL and the Descriptor Privilege Level
     * (DPL) stored in the descriptors themselves */

#ifdef CONFIG_KSPACE
    env->env_tf.tf_ds = GD_KD;
    env->env_tf.tf_es = GD_KD;
    env->env_tf.tf_ss = GD_KD;
    env->env_tf.tf_cs = GD_KT;

    #define ENV_STACK_SIZE_ 0x2000
    static uintptr_t stack_top = 0x2000000;
    stack_top += ENV_STACK_SIZE_;
    env->env_tf.tf_rsp = stack_top;

    #undef ENV_STACK_SIZE_
#else
    env->env_tf.tf_ds = GD_UD | 3;
    env->env_tf.tf_es = GD_UD | 3;
    env->env_tf.tf_ss = GD_UD | 3;
    env->env_tf.tf_cs = GD_UT | 3;
    env->env_tf.tf_rsp = USER_STACK_TOP;
#endif

    /* Commit the allocation */
    env_free_list = env->env_link;
    *newenv_store = env;

    if (trace_envs) cprintf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, env->env_id);
    return 0;
}

/* Pass the original ELF image to binary/size and bind all the symbols within
 * its loaded address space specified by image_start/image_end.
 * Make sure you understand why you need to check that each binding
 * must be performed within the image_start/image_end range.
 */
static int
bind_functions(struct Env *env, uint8_t *binary, size_t size, uintptr_t image_start, uintptr_t image_end) {
    #define DEMAND_(STMT)           \
        if (!(STMT)) {              \
            return -E_INVALID_EXE;  \
        }

    DEMAND_(size > sizeof(struct Elf));
    struct Elf *elf_header = (struct Elf *)binary;

    DEMAND_(elf_header->e_magic == ELF_MAGIC);
    DEMAND_(elf_header->e_shentsize == sizeof(struct Secthdr));
    DEMAND_(elf_header->e_phentsize == sizeof(struct Proghdr));
    DEMAND_(elf_header->e_shstrndx < elf_header->e_shnum);
    DEMAND_(elf_header->e_shstrndx != ELF_SHN_UNDEF);

    DEMAND_(elf_header->e_shoff + elf_header->e_shnum * sizeof(struct Secthdr) <= size);
    struct Secthdr *sections = (struct Secthdr *)(binary + elf_header->e_shoff);

    DEMAND_(sections[elf_header->e_shstrndx].sh_offset < size);
    const char *shstrtab = (const char *)(binary + sections[elf_header->e_shstrndx].sh_offset);

    struct Elf64_Sym *symtab = NULL;
    uint64_t symtab_size = 0;
    const char *strtab = NULL;
    uint64_t strtab_size = 0;

    for (uint16_t i = 0; i < elf_header->e_shnum; ++i) {
        if (sections[i].sh_type == ELF_SHT_SYMTAB &&
            strcmp(shstrtab + sections[i].sh_name, ".symtab") == 0) {
            assert(!symtab);
            DEMAND_(sections[i].sh_offset + sections[i].sh_size < size);
            symtab = (struct Elf64_Sym *)(binary + sections[i].sh_offset);
            symtab_size = sections[i].sh_size;
        } else if (sections[i].sh_type == ELF_SHT_STRTAB &&
                   strcmp(shstrtab + sections[i].sh_name, ".strtab") == 0) {
            assert(!strtab);
            DEMAND_(sections[i].sh_offset + sections[i].sh_size < size);
            strtab = (const char *)(binary + sections[i].sh_offset);
            strtab_size = sections[i].sh_size;
        }
    }

    // TODO: Actually link based on plt, and not static-linkage tables...

    DEMAND_(symtab && strtab);

    uint64_t symtab_length = symtab_size / sizeof(struct Elf64_Sym);

    const char *excluded_names[] = {"binaryname", "thisenv", NULL};

    for (uint64_t i = 0; i < symtab_length; ++i) {
        uintptr_t relocation = symtab[i].st_value;

        DEMAND_(symtab[i].st_name < strtab_size);
        const char *func_name = strtab + symtab[i].st_name;

        if (relocation < image_start || relocation + sizeof(uintptr_t) > image_end ||
            ELF64_ST_BIND(symtab[i].st_info) != STB_GLOBAL ||
            ELF64_ST_TYPE(symtab[i].st_info) != STT_OBJECT) {
            continue;
        }

        int is_excluded = 0;
        for (const char **excl_name = excluded_names; *excl_name; ++excl_name) {
            if (strcmp(*excl_name, func_name) == 0) {
                is_excluded = 1;
                break;
            }
        }
        if (is_excluded) {
            continue;
        }

        uintptr_t function = find_function(func_name);
        if (!function) {
            cprintf("bind_functions: Unrecognized function reference: %s\n", func_name);
            continue;
        }

        cprintf("bind_functions: Bound %s() (st_info=%hhX) to %p\n", func_name, symtab[i].st_info, (void *)relocation);

        // decltype(&cprintf)
        // typedef void (*funcptr_t_)();
        // void (*)(void)
        // int (*)(const char *fmt, ...)
        *(uintptr_t *)relocation = function;
    }

    #undef DEMAND_

    return 0;
}

/* Set up the initial program binary, stack, and processor flags
 * for a user process.
 * This function is ONLY called during kernel initialization,
 * before running the first environment.
 *
 * This function loads all loadable segments from the ELF binary image
 * into the environment's user memory, starting at the appropriate
 * virtual addresses indicated in the ELF program header.
 * At the same time it clears to zero any portions of these segments
 * that are marked in the program header as being mapped
 * but not actually present in the ELF file - i.e., the program's bss section.
 *
 * All this is very similar to what our boot loader does, except the boot
 * loader also needs to read the code from disk.  Take a look at
 * LoaderPkg/Loader/Bootloader.c to get ideas.
 *
 * Finally, this function maps one page for the program's initial stack.
 *
 * load_icode returns -E_INVALID_EXE if it encounters problems.
 *  - How might load_icode fail?  What might be wrong with the given input?
 *
 * Hints:
 *   Load each program segment into memory
 *   at the address specified in the ELF section header.
 *   You should only load segments with ph->p_type == ELF_PROG_LOAD.
 *   Each segment's address can be found in ph->p_va
 *   and its size in memory can be found in ph->p_memsz.
 *   The ph->p_filesz bytes from the ELF binary, starting at
 *   'binary + ph->p_offset', should be copied to address
 *   ph->p_va.  Any remaining memory bytes should be cleared to zero.
 *   (The ELF header should have ph->p_filesz <= ph->p_memsz.)
 *
 *   All page protection bits should be user read/write for now.
 *   ELF segments are not necessarily page-aligned, but you can
 *   assume for this function that no two segments will touch
 *   the same page.
 *
 *   You must also do something with the program's entry point,
 *   to make sure that the environment starts executing there.
 *   What?  (See env_run() and env_pop_tf() below.) */
static int
load_icode(struct Env *env, uint8_t *binary, size_t size) {
    assert(env);
    assert(binary);

    int result = 0;
    uint8_t *cur = binary;

    #define DEMAND_(STMT)                           \
        if (!(STMT)) {                              \
            return -E_INVALID_EXE;                  \
        }

    #define READ_FROM_OFFS_(DEST, OFFSET, AMOUNT)   \
        READ_FROM_PTR_((DEST), binary + (OFFSET), (AMOUNT));

    #define READ_FROM_PTR_(DEST, PTR, AMOUNT) {     \
        uint8_t *ptr_ = (PTR);                      \
        size_t amount_ = (AMOUNT);                  \
                                                    \
        DEMAND_(ptr_ + amount_ - binary <= size);   \
                                                    \
        memcpy((DEST), ptr_, amount_);              \
    }

    #define READ_(DEST, AMOUNT) {                   \
        size_t amount_ = (AMOUNT);                  \
                                                    \
        DEMAND_(cur + amount_ - binary <= size);    \
                                                    \
        memcpy((DEST), cur, amount_);               \
                                                    \
        cur += amount_;                             \
    }

    // TODO: Maybe not copy
    struct Elf elf_header;
    READ_(&elf_header, sizeof(elf_header));

    DEMAND_(elf_header.e_magic == ELF_MAGIC);
    DEMAND_(elf_header.e_shentsize == sizeof(struct Secthdr));
    DEMAND_(elf_header.e_phentsize == sizeof(struct Proghdr));
    // Turns out it isn't actually necessary, but a check is always fine
    DEMAND_(elf_header.e_shstrndx < elf_header.e_shnum);
    DEMAND_(elf_header.e_shstrndx != ELF_SHN_UNDEF);

    // TODO: Overflow intrinsics
    DEMAND_(elf_header.e_shoff + elf_header.e_shnum * sizeof(struct Secthdr) <= size);
    // Unused, currently
    // struct Secthdr *sections = (struct Secthdr *)(binary + elf_header.e_shoff);

    DEMAND_(elf_header.e_phoff + elf_header.e_phnum * sizeof(struct Proghdr) <= size);
    struct Proghdr *program_headers = (struct Proghdr *)(binary + elf_header.e_phoff);

    uintptr_t image_start = (uintptr_t)-1;
    uintptr_t image_end = 0;

    for (uint16_t i = 0; i < elf_header.e_phnum; ++i) {
        struct Proghdr *ph = &program_headers[i];

        if (ph->p_type == ELF_PROG_LOAD) {
            DEMAND_(ph->p_filesz <= ph->p_memsz);

            READ_FROM_OFFS_((uint8_t *)ph->p_va, ph->p_offset, ph->p_filesz);

            memset((uint8_t *)ph->p_va + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);

            if (ph->p_va < image_start) {
                image_start = ph->p_va;
            }

            if (ph->p_va + ph->p_memsz > image_end) {
                image_end = ph->p_va + ph->p_memsz;
            }
        }
    }

    env->binary = binary;
    DEMAND_(image_start <= elf_header.e_entry && elf_header.e_entry < image_end);
    env->env_tf.tf_rip = elf_header.e_entry;

    result = bind_functions(env, binary, size, image_start, image_end);
    DEMAND_(result >= 0);

    #undef READ_
    #undef READ_FROM_PTR_
    #undef READ_FROM_OFFS_
    #undef DEMAND_

    return 0;
}

/* Allocates a new env with env_alloc, loads the named elf
 * binary into it with load_icode, and sets its env_type.
 * This function is ONLY called during kernel initialization,
 * before running the first user-mode environment.
 * The new env's parent ID is set to 0.
 */
void
env_create(uint8_t *binary, size_t size, enum EnvType type) {
    int result = 0;
    struct Env *env = NULL;

    #define VALIDATE_(MSG, ...) {                       \
        if (result < 0) {                               \
            panic(MSG " (%i)", ##__VA_ARGS__, result);  \
        }                                               \
    }

    result = env_alloc(&env, 0, type);
    VALIDATE_("env_alloc failed");
    assert(env);

    result = load_icode(env, binary, size);
    VALIDATE_("load_icode failed");

    env->env_type = ENV_TYPE_KERNEL;

    #undef VALIDATE_
}


/* Frees env and all memory it uses */
void
env_free(struct Env *env) {
    if (!env) {
        return;
    }

    /* Note the environment's demise. */
    if (trace_envs) cprintf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, env->env_id);

    /* Return the environment to the free list */
    env->env_status = ENV_FREE;
    env->env_link = env_free_list;
    env_free_list = env;
}

/* Frees environment env
 *
 * If env was the current one, then runs a new environment
 * (and does not return to the caller)
 */
void
env_destroy(struct Env *env) {
    /* If env is currently running on other CPUs, we change its state to
     * ENV_DYING. A zombie environment will be freed the next time
     * it traps to the kernel. */
    assert(env);
    //cprintf("KILL %08X\n", env->env_id);

    //env->env_status = ENV_DYING;
    env_free(env);

    if (env == curenv) {
        sched_yield();
    }
}

#ifdef CONFIG_KSPACE
void
csys_exit(void) {
    if (!curenv) panic("curenv = NULL");
    env_destroy(curenv);
}

void
csys_yield(struct Trapframe *tf) {
    memcpy(&curenv->env_tf, tf, sizeof(struct Trapframe));
    sched_yield();
}
#endif

/* Restores the register values in the Trapframe with the 'ret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */

_Noreturn void
env_pop_tf(struct Trapframe *tf) {
    /* Push RIP on program stack */
    tf->tf_rsp -= sizeof(uintptr_t);
    *((uintptr_t *)tf->tf_rsp) = tf->tf_rip;
    /* Push RFLAGS on program stack */
    tf->tf_rsp -= sizeof(uintptr_t);
    *((uintptr_t *)tf->tf_rsp) = tf->tf_rflags;

    asm volatile(
            "movq %0, %%rsp\n"
            "movq 0(%%rsp), %%r15\n"
            "movq 8(%%rsp), %%r14\n"
            "movq 16(%%rsp), %%r13\n"
            "movq 24(%%rsp), %%r12\n"
            "movq 32(%%rsp), %%r11\n"
            "movq 40(%%rsp), %%r10\n"
            "movq 48(%%rsp), %%r9\n"
            "movq 56(%%rsp), %%r8\n"
            "movq 64(%%rsp), %%rsi\n"
            "movq 72(%%rsp), %%rdi\n"
            "movq 80(%%rsp), %%rbp\n"
            "movq 88(%%rsp), %%rdx\n"
            "movq 96(%%rsp), %%rcx\n"
            "movq 104(%%rsp), %%rbx\n"
            "movq 112(%%rsp), %%rax\n"
            "movw 120(%%rsp), %%es\n"
            "movw 128(%%rsp), %%ds\n"
            "movq (128+48)(%%rsp), %%rsp\n"
            "popfq; ret" ::"g"(tf)
            : "memory");

    /* Mostly to placate the compiler */
    panic("Reached unreacheble\n");
}

/* Context switch from curenv to env.
 * This function does not return.
 *
 * Step 1: If this is a context switch (a new environment is running):
 *       1. Set the current environment (if any) back to
 *          ENV_RUNNABLE if it is ENV_RUNNING (think about
 *          what other states it can be in),
 *       2. Set 'curenv' to the new environment,
 *       3. Set its status to ENV_RUNNING,
 *       4. Update its 'env_runs' counter,
 * Step 2: Use env_pop_tf() to restore the environment's
 *       registers and starting execution of process.

 * Hints:
 *    If this is the first call to env_run, curenv is NULL.
 *
 *    This function loads the new environment's state from
 *    env->env_tf.  Go back through the code you wrote above
 *    and make sure you have set the relevant parts of
 *    env->env_tf to sensible values.
 */
_Noreturn void
env_run(struct Env *env) {
    assert(env);

    if (trace_envs_more) {
        const char *state[] = {"FREE", "DYING", "RUNNABLE", "RUNNING", "NOT_RUNNABLE"};
        if (curenv) cprintf("[%08X] env stopped: %s\n", curenv->env_id, state[curenv->env_status]);
        cprintf("[%08X] env started: %s\n", env->env_id, state[env->env_status]);
    }

    if (env->env_status == ENV_RUNNING) {
        assert(env == curenv);
        env_pop_tf(&env->env_tf);
    }

    assert(env->env_status == ENV_RUNNABLE);

    if (env != curenv && curenv) {
        // TODO: Maybe save curenv trapframe
        if (curenv->env_status == ENV_RUNNING) {
            curenv->env_status = ENV_RUNNABLE;
        }
    }

    curenv = env;
    env->env_status = ENV_RUNNABLE;
    env->env_runs++;

    env_pop_tf(&env->env_tf);

    panic("Shouldn't be reachable");
}
