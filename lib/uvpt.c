/* User virtual page table helpers */

#include <inc/lib.h>
#include <inc/mmu.h>

extern volatile pte_t uvpt[];     /* VA of "virtual page table" */
extern volatile pde_t uvpd[];     /* VA of current page directory */
extern volatile pdpe_t uvpdp[];   /* VA of current page directory pointer */
extern volatile pml4e_t uvpml4[]; /* VA of current page map level 4 */

pte_t
get_uvpt_entry(void *va) {
    if (!(uvpml4[VPML4(va)] & PTE_P)) return uvpml4[VPML4(va)];
    if (!(uvpdp[VPDP(va)] & PTE_P) || (uvpdp[VPDP(va)] & PTE_PS)) return uvpdp[VPDP(va)];
    if (!(uvpd[VPD(va)] & PTE_P) || (uvpd[VPD(va)] & PTE_PS)) return uvpd[VPD(va)];
    return uvpt[VPT(va)];
}

int
get_prot(void *va) {
    pte_t pte = get_uvpt_entry(va);
    int prot = pte & PTE_AVAIL & ~PTE_SHARE;
    if (pte & PTE_P) prot |= PROT_R;
    if (pte & PTE_W) prot |= PROT_W;
    if (!(pte & PTE_NX)) prot |= PROT_X;
    if (pte & PTE_SHARE) prot |= PROT_SHARE;
    return prot;
}

bool
is_page_dirty(void *va) {
    pte_t pte = get_uvpt_entry(va);
    return pte & PTE_D;
}

bool
is_page_present(void *va) {
    return get_uvpt_entry(va) & PTE_P;
}

int
foreach_shared_region(int (*fun)(void *start, void *end, void *arg), void *arg) {
    /* Calls fun() for every shared region */
    // LAB 11: Your code here DONE
    assert(fun);

    uint64_t addr = 0;
    
    static_assert(sizeof(uint64_t) == sizeof(void *), "I assumed we were on x64");

    #define LEVEL_OFFS_(LEVEL) \
        (12 + 9 * (LEVEL - 1))

    #define LEVEL_MASK_(LEVEL) \
        (0b111111111ull << LEVEL_OFFS_(LEVEL))

    #define ADDR_GET_(LEVEL) \
        (0b111111111ull & (addr >> LEVEL_OFFS_(LEVEL)))
    
    #define ADDR_SET_(LEVEL, VALUE) \
        addr = (addr & ~LEVEL_MASK_(LEVEL)) | (((uint64_t)(VALUE) << LEVEL_OFFS_(LEVEL)) & LEVEL_MASK_(LEVEL))

    #define ADDR_INC_(LEVEL) \
        ADDR_SET_(LEVEL, ADDR_GET_(LEVEL) + 1)

    static const size_t SIZES[5] = {1ull, 1ull << 12, 1ull << 21, 1ull << 30, 1ull << 39};

    #define LOOP_(LEVEL, TABLE, MACRO, ...)                                     \
        for (unsigned i##LEVEL = 0; i##LEVEL < PT_ENTRY_COUNT; ++i##LEVEL) {    \
            pte = TABLE[MACRO(addr)];                                           \
                                                                                \
            if (!(pte & PTE_P)) {                                               \
                ADDR_INC_(LEVEL);                                               \
                continue;                                                       \
            }                                                                   \
                                                                                \
            if (LEVEL == 1 || (LEVEL != 4 && (pte & PTE_PS))) {                 \
                if (pte & PTE_SHARE) {                                          \
                    fun((void *)addr, (void *)addr + SIZES[LEVEL], arg);        \
                }                                                               \
                                                                                \
                ADDR_INC_(LEVEL);                                               \
                continue;                                                       \
            }                                                                   \
                                                                                \
            { __VA_ARGS__; }                                                    \
                                                                                \
            ADDR_INC_(LEVEL);                                                   \
        }

    pte_t pte = 0;
    LOOP_(4, uvpml4, VPML4,
        LOOP_(3, uvpdp, VPDP,
            LOOP_(2, uvpd, VPD,
                LOOP_(1, uvpt, VPT,
                );
            );
        );
    );

    #undef LOOP_

    assert(addr == 0);

    #undef ADDR_INC_
    #undef ADDR_SET_
    #undef ADDR_GET_
    #undef LEVEL_MASK_
    #undef LEVEL_OFFS_

    return 0;
}
