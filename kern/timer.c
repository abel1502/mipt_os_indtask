#include <inc/types.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/x86.h>
#include <inc/uefi.h>
#include <kern/timer.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define kilo      (1000ULL)
#define Mega      (kilo * kilo)
#define Giga      (kilo * Mega)
#define Tera      (kilo * Giga)
#define Peta      (kilo * Tera)
#define ULONG_MAX ~0UL

#if LAB <= 6
/* Early variant of memory mapping that does 1:1 aligned area mapping
 * in 2MB pages. You will need to reimplement this code with proper
 * virtual memory mapping in the future. */
void *
mmio_map_region(physaddr_t pa, size_t size) {
    void map_addr_early_boot(uintptr_t addr, uintptr_t addr_phys, size_t sz);
    const physaddr_t base_2mb = 0x200000;
    uintptr_t org = pa;
    size += pa & (base_2mb - 1);
    size += (base_2mb - 1);
    pa &= ~(base_2mb - 1);
    size &= ~(base_2mb - 1);
    map_addr_early_boot(pa, pa, size);
    return (void *)org;
}
void *
mmio_remap_last_region(physaddr_t pa, void *addr, size_t oldsz, size_t newsz) {
    return mmio_map_region(pa, newsz);
}
#endif

struct Timer timertab[MAX_TIMERS];
struct Timer *timer_for_schedule;

struct Timer timer_hpet0 = {
        .timer_name = "hpet0",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim0,
        .handle_interrupts = hpet_handle_interrupts_tim0,
};

struct Timer timer_hpet1 = {
        .timer_name = "hpet1",
        .timer_init = hpet_init,
        .get_cpu_freq = hpet_cpu_frequency,
        .enable_interrupts = hpet_enable_interrupts_tim1,
        .handle_interrupts = hpet_handle_interrupts_tim1,
};

struct Timer timer_acpipm = {
        .timer_name = "pm",
        .timer_init = acpi_enable,
        .get_cpu_freq = pmtimer_cpu_frequency,
};

void
acpi_enable(void) {
    FADT *fadt = get_fadt();
    outb(fadt->SMI_CommandPort, fadt->AcpiEnable);
    while ((inw(fadt->PM1aControlBlock) & 1) == 0) /* nothing */
        ;
}

static void *
acpi_find_table(const char *sign) {
    /*
     * This function performs lookup of ACPI table by its signature
     * and returns valid pointer to the table mapped somewhere.
     *
     * It is a good idea to checksum tables before using them.
     *
     * HINT: Use mmio_map_region/mmio_remap_last_region
     * before accessing table addresses
     * (Why mmio_remap_last_region is requrired?)
     * HINT: RSDP address is stored in uefi_lp->ACPIRoot
     * HINT: You may want to distunguish RSDT/XSDT
     */

    // LAB 5: Your code here DONE
    RSDP *rsdp = mmio_map_region(uefi_lp->ACPIRoot, sizeof(RSDP));
    assert(rsdp);

    assert(strncmp(rsdp->Signature, "RSD PTR ", sizeof(rsdp->Signature)) == 0);

    assert(rsdp->Revision >= 2);

    assert(rsdp->Length >= sizeof(RSDP));
    uint32_t rsdp_checksum = 0;
    for (unsigned i = 0; i < rsdp->Length; ++i) {
        if (i == offsetof(RSDP, Length)) {
            assert((rsdp_checksum & 0xff) == 0);
        }

        rsdp_checksum += ((uint8_t *)rsdp)[i];
    }
    assert((rsdp_checksum & 0xff) == 0);

    physaddr_t xsdt_pa = rsdp->XsdtAddress;

    XSDT *xsdt = mmio_remap_last_region(xsdt_pa, rsdp, sizeof(RSDP), sizeof(XSDT));
    assert(xsdt);
    rsdp = NULL;

    assert(strncmp(xsdt->h.Signature, "XSDT", sizeof(xsdt->h.Signature)) == 0);

    assert(xsdt->h.Length >= sizeof(XSDT));
    uint32_t xsdt_checksum = 0;
    for (unsigned i = 0; i < xsdt->h.Length; ++i) {
        xsdt_checksum += ((uint8_t *)xsdt)[i];
    }
    assert((xsdt_checksum & 0xff) == 0);

    // Doesn't matter where we point it now, so I chose the certainly valid XSDT header
    ACPISDTHeader *header = mmio_map_region(xsdt_pa, sizeof(ACPISDTHeader));
    assert(header);

    unsigned nTables = (xsdt->h.Length - sizeof(ACPISDTHeader)) / sizeof(xsdt->PointerToOtherSDT[0]);
    for (unsigned i = 0; i < nTables; ++i) {
        physaddr_t header_pa = xsdt->PointerToOtherSDT[i];

        header = mmio_remap_last_region(header_pa, header, sizeof(ACPISDTHeader), sizeof(ACPISDTHeader));
        assert(header);

        if (strncmp(header->Signature, sign, sizeof(header->Signature)) == 0) {
            assert(header->Length >= sizeof(ACPISDTHeader));
            uint32_t resultLen = header->Length;
            void *result = mmio_remap_last_region(header_pa, header, sizeof(ACPISDTHeader), resultLen);
            assert(result);

            uint32_t resultChecksum = 0;
            for (unsigned i = 0; i < resultLen; ++i) {
                resultChecksum += ((uint8_t *)result)[i];
            }
            assert((resultChecksum & 0xff) == 0);

            return result;
        }
    }

    return NULL;
}

/* Obtain and map FADT ACPI table address. */
FADT *
get_fadt(void) {
    // LAB 5: Your code here DONE
    // (use acpi_find_table)
    // HINT: ACPI table signatures are
    //       not always as their names

    static FADT *kfadt = NULL;

    if (!kfadt) {
        kfadt = acpi_find_table("FACP");
        assert(kfadt);
        // cprintf("%.4s v%hhu %u %zu\n", kfadt->h.Signature, kfadt->h.Revision, kfadt->h.Length, sizeof(*kfadt));
        assert(kfadt->h.Length >= sizeof(*kfadt));
    }

    return kfadt;
}

/* Obtain and map RSDP ACPI table address. */
HPET *
get_hpet(void) {
    // LAB 5: Your code here DONE
    // (use acpi_find_table)

    static HPET *khpet = NULL;

    if (!khpet) {
        khpet = acpi_find_table("HPET");
        assert(khpet);
        assert(khpet->h.Length >= sizeof(*khpet));
    }

    return khpet;
}

/* Getting physical HPET timer address from its table. */
HPETRegister *
hpet_register(void) {
    HPET *hpet_timer = get_hpet();
    if (!hpet_timer->address.address) panic("hpet is unavailable\n");

    uintptr_t paddr = hpet_timer->address.address;
    return mmio_map_region(paddr, sizeof(HPETRegister));
}

/* Debug HPET timer state. */
void
hpet_print_struct(void) {
    HPET *hpet = get_hpet();
    cprintf("signature = %s\n", (hpet->h).Signature);
    cprintf("length = %08x\n", (hpet->h).Length);
    cprintf("revision = %08x\n", (hpet->h).Revision);
    cprintf("checksum = %08x\n", (hpet->h).Checksum);

    cprintf("oem_revision = %08x\n", (hpet->h).OEMRevision);
    cprintf("creator_id = %08x\n", (hpet->h).CreatorID);
    cprintf("creator_revision = %08x\n", (hpet->h).CreatorRevision);

    cprintf("hardware_rev_id = %08x\n", hpet->hardware_rev_id);
    cprintf("comparator_count = %08x\n", hpet->comparator_count);
    cprintf("counter_size = %08x\n", hpet->counter_size);
    cprintf("reserved = %08x\n", hpet->reserved);
    cprintf("legacy_replacement = %08x\n", hpet->legacy_replacement);
    cprintf("pci_vendor_id = %08x\n", hpet->pci_vendor_id);
    cprintf("hpet_number = %08x\n", hpet->hpet_number);
    cprintf("minimum_tick = %08x\n", hpet->minimum_tick);

    cprintf("address_structure:\n");
    cprintf("address_space_id = %08x\n", (hpet->address).address_space_id);
    cprintf("register_bit_width = %08x\n", (hpet->address).register_bit_width);
    cprintf("register_bit_offset = %08x\n", (hpet->address).register_bit_offset);
    cprintf("address = %08lx\n", (unsigned long)(hpet->address).address);
}

static volatile HPETRegister *hpetReg;
/* HPET timer period (in femtoseconds) */
static uint64_t hpetFemto = 0;
/* HPET timer frequency */
static uint64_t hpetFreq = 0;

/* HPET timer initialisation */
void
hpet_init() {
    if (hpetReg == NULL) {
        nmi_disable();
        hpetReg = hpet_register();
        uint64_t cap = hpetReg->GCAP_ID;
        hpetFemto = (uintptr_t)(cap >> 32);
        if (!(cap & HPET_LEG_RT_CAP)) panic("HPET has no LegacyReplacement mode");

        /* cprintf("hpetFemto = %llu\n", hpetFemto); */
        hpetFreq = (1 * Peta) / hpetFemto;
        /* cprintf("HPET: Frequency = %d.%03dMHz\n", (uintptr_t)(hpetFreq / Mega), (uintptr_t)(hpetFreq % Mega)); */
        /* Enable ENABLE_CNF bit to enable timer */
        hpetReg->GEN_CONF |= HPET_ENABLE_CNF;
        nmi_enable();
    }
}

/* HPET register contents debugging. */
void
hpet_print_reg(void) {
    cprintf("GCAP_ID = %016lx\n", (unsigned long)hpetReg->GCAP_ID);
    cprintf("GEN_CONF = %016lx\n", (unsigned long)hpetReg->GEN_CONF);
    cprintf("GINTR_STA = %016lx\n", (unsigned long)hpetReg->GINTR_STA);
    cprintf("MAIN_CNT = %016lx\n", (unsigned long)hpetReg->MAIN_CNT);
    cprintf("TIM0_CONF = %016lx\n", (unsigned long)hpetReg->TIM0_CONF);
    cprintf("TIM0_COMP = %016lx\n", (unsigned long)hpetReg->TIM0_COMP);
    cprintf("TIM0_FSB = %016lx\n", (unsigned long)hpetReg->TIM0_FSB);
    cprintf("TIM1_CONF = %016lx\n", (unsigned long)hpetReg->TIM1_CONF);
    cprintf("TIM1_COMP = %016lx\n", (unsigned long)hpetReg->TIM1_COMP);
    cprintf("TIM1_FSB = %016lx\n", (unsigned long)hpetReg->TIM1_FSB);
    cprintf("TIM2_CONF = %016lx\n", (unsigned long)hpetReg->TIM2_CONF);
    cprintf("TIM2_COMP = %016lx\n", (unsigned long)hpetReg->TIM2_COMP);
    cprintf("TIM2_FSB = %016lx\n", (unsigned long)hpetReg->TIM2_FSB);
}

/* HPET main timer counter value. */
uint64_t
hpet_get_main_cnt(void) {
    return hpetReg->MAIN_CNT;
}

/* - Configure HPET timer 0 to trigger every 0.5 seconds on IRQ_TIMER line
 * - Configure HPET timer 1 to trigger every 1.5 seconds on IRQ_CLOCK line
 *
 * HINT To be able to use HPET as PIT replacement consult
 *      LegacyReplacement functionality in HPET spec.
 * HINT Don't forget to unmask interrupt in PIC */
void
hpet_enable_interrupts_tim0(void) {
    // LAB 5: Your code here DONE
    nmi_disable();

    assert(hpetReg);
    assert(hpetReg->GCAP_ID & HPET_LEG_RT_CAP);
    
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    hpetReg->TIM0_CONF |= HPET_TN_INT_ENB_CNF | HPET_TN_TYPE_CNF;
    hpetReg->TIM0_COMP = 50 * Mega;
    pic_irq_unmask(IRQ_TIMER);

    hpet_print_reg();

    nmi_enable();
}

void
hpet_enable_interrupts_tim1(void) {
    // LAB 5: Your code here DONE
    nmi_disable();

    assert(hpetReg);
    assert(hpetReg->GCAP_ID & HPET_LEG_RT_CAP);
    
    hpetReg->GEN_CONF |= HPET_LEG_RT_CNF;
    hpetReg->TIM1_CONF |= HPET_TN_INT_ENB_CNF | HPET_TN_TYPE_CNF;
    hpetReg->TIM1_COMP = 150 * Mega;
    pic_irq_unmask(IRQ_CLOCK);

    nmi_enable();
}

// TODO: Maybe implement somehow?
void
hpet_handle_interrupts_tim0(void) {
    // cprintf("Timer 0!\n");
    pic_send_eoi(IRQ_TIMER);
}

void
hpet_handle_interrupts_tim1(void) {
    // cprintf("Timer 1!\n");
    pic_send_eoi(IRQ_CLOCK);
}

/* Calculate CPU frequency in Hz with the help with HPET timer.
 * HINT Use hpet_get_main_cnt function and do not forget about
 * about pause instruction. */
uint64_t
hpet_cpu_frequency(void) {
    static uint64_t cpu_freq = 0;

    // LAB 5: Your code here DONE
    //if (!cpu_freq) {
        uint64_t hpet_cnt0 = hpet_get_main_cnt();
        uint64_t tsc_cnt0 = read_tsc();

        for (unsigned i = 0; i < 100; ++i)
            asm volatile ("pause");

        uint64_t hpet_cnt1 = hpet_get_main_cnt();
        uint64_t tsc_cnt1 = read_tsc();

        cpu_freq = hpetFreq * (tsc_cnt1 - tsc_cnt0) / (hpet_cnt1 - hpet_cnt0);
    //}

    return cpu_freq;
}

uint32_t
pmtimer_get_timeval(void) {
    FADT *fadt = get_fadt();
    return inl(fadt->PMTimerBlock);
}

/* Calculate CPU frequency in Hz with the help with ACPI PowerManagement timer.
 * HINT Use pmtimer_get_timeval function and do not forget that ACPI PM timer
 *      can be 24-bit or 32-bit. */
uint64_t
pmtimer_cpu_frequency(void) {
    static uint64_t cpu_freq = 0;

    // LAB 5: Your code here DONE
    //if (!cpu_freq) {
        uint64_t pm_cnt0 = pmtimer_get_timeval();
        uint64_t tsc_cnt0 = read_tsc();
        uint64_t added_tsc_cnt = 0;

        for (unsigned i = 0; i < 100; ++i) {
            asm volatile ("pause");
            if (read_tsc() < tsc_cnt0) {
                // 24-bit counter overflow
                added_tsc_cnt += 1 << 24;
            }
        }

        uint64_t pm_cnt1 = pmtimer_get_timeval();
        uint64_t tsc_cnt1 = read_tsc();

        cpu_freq = PM_FREQ * (tsc_cnt1 + added_tsc_cnt - tsc_cnt0) / (pm_cnt1 - pm_cnt0);
    //}

    return cpu_freq;
}
