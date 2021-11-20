#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/dwarf.h>
#include <inc/elf.h>
#include <inc/x86.h>
#include <inc/error.h>

#include <kern/kdebug.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <inc/uefi.h>

void
load_kernel_dwarf_info(struct Dwarf_Addrs *addrs) {
    addrs->aranges_begin = (uint8_t *)(uefi_lp->DebugArangesStart);
    addrs->aranges_end = (uint8_t *)(uefi_lp->DebugArangesEnd);
    addrs->abbrev_begin = (uint8_t *)(uefi_lp->DebugAbbrevStart);
    addrs->abbrev_end = (uint8_t *)(uefi_lp->DebugAbbrevEnd);
    addrs->info_begin = (uint8_t *)(uefi_lp->DebugInfoStart);
    addrs->info_end = (uint8_t *)(uefi_lp->DebugInfoEnd);
    addrs->line_begin = (uint8_t *)(uefi_lp->DebugLineStart);
    addrs->line_end = (uint8_t *)(uefi_lp->DebugLineEnd);
    addrs->str_begin = (uint8_t *)(uefi_lp->DebugStrStart);
    addrs->str_end = (uint8_t *)(uefi_lp->DebugStrEnd);
    addrs->pubnames_begin = (uint8_t *)(uefi_lp->DebugPubnamesStart);
    addrs->pubnames_end = (uint8_t *)(uefi_lp->DebugPubnamesEnd);
    addrs->pubtypes_begin = (uint8_t *)(uefi_lp->DebugPubtypesStart);
    addrs->pubtypes_end = (uint8_t *)(uefi_lp->DebugPubtypesEnd);
}

void
load_user_dwarf_info(struct Dwarf_Addrs *addrs) {
    assert(curenv);
    assert(current_space == &curenv->address_space);

    uint8_t *binary = curenv->binary;
    assert(curenv->binary);

    struct {
        const uint8_t **end;
        const uint8_t **start;
        const char *name;
    } searched_sections[] = {
            {&addrs->aranges_end, &addrs->aranges_begin, ".debug_aranges"},
            {&addrs->abbrev_end, &addrs->abbrev_begin, ".debug_abbrev"},
            {&addrs->info_end, &addrs->info_begin, ".debug_info"},
            {&addrs->line_end, &addrs->line_begin, ".debug_line"},
            {&addrs->str_end, &addrs->str_begin, ".debug_str"},
            {&addrs->pubnames_end, &addrs->pubnames_begin, ".debug_pubnames"},
            {&addrs->pubtypes_end, &addrs->pubtypes_begin, ".debug_pubtypes"},
    };
    const size_t searched_sections_cnt = sizeof(searched_sections) / sizeof(searched_sections[0]);

    memset(addrs, 0, sizeof(*addrs));

    /* Load debug sections from curenv->binary elf image */
    // LAB 8: Your code here DONE

    struct Elf *elf_header = (struct Elf *)binary;

    struct Secthdr *sections = (struct Secthdr *)(binary + elf_header->e_shoff);

    const char *shstrtab = (const char *)(binary + sections[elf_header->e_shstrndx].sh_offset);

    for (size_t i = 0; i < searched_sections_cnt; ++i) {
        for (uint16_t curSect = 0; curSect < elf_header->e_shnum; ++curSect) {
            if (strcmp(shstrtab + sections[curSect].sh_name, searched_sections[i].name) == 0) {
                const uint8_t *start = binary + sections[curSect].sh_offset;
                *searched_sections[i].start = start;
                *searched_sections[i].end   = start + sections[curSect].sh_size;
            }
        }
    }
}

#define UNKNOWN       "<unknown>"
#define CALL_INSN_LEN 5

/* debuginfo_rip(addr, info)
 * Fill in the 'info' structure with information about the specified
 * instruction address, 'addr'.  Returns 0 if information was found, and
 * negative if not.  But even if it returns negative it has stored some
 * information into '*info'
 */
int
debuginfo_rip(uintptr_t addr, struct Ripdebuginfo *info) {
    if (!addr) return 0;

    /* Initialize *info */
    strcpy(info->rip_file, UNKNOWN);
    strcpy(info->rip_fn_name, UNKNOWN);
    info->rip_fn_namelen = sizeof UNKNOWN - 1;
    info->rip_filelen = sizeof UNKNOWN - 1;
    info->rip_line = 0;
    info->rip_fn_addr = addr;
    info->rip_fn_narg = 0;

    /* Temporarily load kernel cr3 and return back once done.
    * Make sure that you fully understand why it is necessary. */
    // LAB 8: Your code here DONE

    /* Load dwarf section pointers from either
     * currently running program binary or use
     * kernel debug info provided by bootloader
     * depending on whether addr is pointing to userspace
     * or kernel space */
    // LAB 8: Your code here DONE

    struct Dwarf_Addrs addrs;
    if (addr < KERN_BASE_ADDR) {
        load_user_dwarf_info(&addrs);
    } else {
        load_kernel_dwarf_info(&addrs);
    }

    Dwarf_Off offset = 0, line_offset = 0;
    int res = info_by_address(&addrs, addr, &offset);
    if (res < 0) goto error;

    char *tmp_buf = NULL;
    res = file_name_by_info(&addrs, offset, &tmp_buf, &line_offset);
    if (res < 0) goto error;
    strncpy(info->rip_file, tmp_buf, sizeof(info->rip_file));
    info->rip_filelen = strnlen(tmp_buf, sizeof(info->rip_file));

    /* Find line number corresponding to given address.
    * Hint: note that we need the address of `call` instruction, but rip holds
    * address of the next instruction, so we should substract 5 from it.
    * Hint: use line_for_address from kern/dwarf_lines.c */

    res = line_for_address(&addrs, addr - CALL_INSN_LEN, line_offset, &info->rip_line);
    if (res < 0) goto error;

    /* Find function name corresponding to given address.
    * Hint: note that we need the address of `call` instruction, but rip holds
    * address of the next instruction, so we should substract 5 from it.
    * Hint: use function_by_info from kern/dwarf.c
    * Hint: info->rip_fn_name can be not NULL-terminated,
    * string returned by function_by_info will always be */

    tmp_buf = NULL;
    res = function_by_info_nargs(&addrs, addr - CALL_INSN_LEN, offset, &tmp_buf, &info->rip_fn_addr, &info->rip_fn_narg);
    if (res < 0) goto error;
    strncpy(info->rip_fn_name, tmp_buf, sizeof(info->rip_fn_name));
    info->rip_fn_namelen = strnlen(tmp_buf, sizeof(info->rip_fn_name));

error:
    return res;
}

static int symtab_address_by_fname(const char *fname, uintptr_t *offset);

uintptr_t
find_function(const char *fname) {
    /* There are two functions for function name lookup.
     * address_by_fname, which looks for function name in section .debug_pubnames
     * and naive_address_by_fname which performs full traversal of DIE tree.
     * It may also be useful to look to kernel symbol table for symbols defined
     * in assembly. */

    int result = 0;

    struct Dwarf_Addrs addrs;
    load_kernel_dwarf_info(&addrs);

    uintptr_t offset = 0;

    result = address_by_fname(&addrs, fname, &offset);
    if (result >= 0) {
        return offset;
    }

    result = naive_address_by_fname(&addrs, fname, &offset);
    if (result >= 0) {
        return offset;
    }

    result = symtab_address_by_fname(fname, &offset);
    if (result >= 0) {
        return offset;
    }

    return 0;
}

static int
symtab_address_by_fname(const char *fname, uintptr_t *offset) {
    #define DEMAND_(STMT)           \
        if (!(STMT)) {              \
            return -E_INVALID_EXE;  \
        }
    
    assert(fname && offset);

    const struct Elf64_Sym *cur_symbol = (struct Elf64_Sym *)uefi_lp->SymbolTableStart;
    const struct Elf64_Sym *symtab_end = (struct Elf64_Sym *)uefi_lp->SymbolTableEnd;

    DEMAND_(symtab_end >= cur_symbol);

    const char *strtab     = (const char *)uefi_lp->StringTableStart;
    const char *strtab_end = (const char *)uefi_lp->StringTableEnd;

    DEMAND_(strtab_end >= strtab);

    for (; cur_symbol < symtab_end; ++cur_symbol) {
        if (ELF64_ST_TYPE(cur_symbol->st_info) != STT_FUNC) {
            continue;
        }

        const char *func_name = strtab + cur_symbol->st_name;

        DEMAND_(func_name < strtab_end);

        if (strcmp(fname, func_name) == 0) {
            *offset = (uintptr_t)cur_symbol->st_value;
            return 0;
        }
    }

    #undef DEMAND_

    return -E_NO_ENT;
}
