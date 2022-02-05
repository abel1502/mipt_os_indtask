
#include "pci.h"
#include <inc/x86.h>
#include <inc/stdio.h>
#include <inc/types.h>
#include <inc/assert.h>

static struct {
    uint8_t dev_class, dev_subclass;
    const char *name;
} pci_device_type_strings[] = {
    {0x00, 0x00, "Unknown device"},
    {0x00, 0x01, "VGA-compatible Device"},
    {0x01, 0x00, "SCSI Bus Controller"},
    {0x01, 0x01, "IDE Controller"},
    {0x01, 0x02, "Floppy Disk Controller"},
    {0x01, 0x03, "IPI Bus Controller"},
    {0x01, 0x04, "RAID Controller"},
    {0x01, 0x05, "ATA Controller"},
    {0x01, 0x06, "SATA Controller"},
    {0x01, 0x07, "Serial Attached SCSI Controller"},
    {0x01, 0x80, "Other Mass Storage Controller"},
    {0x02, 0x00, "Ethernet Controller"},
    {0x02, 0x01, "Token Ring Controller"},
    {0x02, 0x02, "FDDI Controller"},
    {0x02, 0x03, "ATM Controller"},
    {0x02, 0x04, "ISDN Controller"},
    {0x02, 0x05, "WorldFip Controller"},
    {0x02, 0x06, "PICMG 2.14 Multi Computing"},
    {0x02, 0x80, "Other Network Controller"},
    {0x03, 0x00, "VGA-compatible Controller"},
    {0x03, 0x01, "XGA Controller"},
    {0x03, 0x02, "3D Controller"},
    {0x03, 0x80, "Other Display Controller"},
    {0x04, 0x00, "Video Device"},
    {0x04, 0x01, "Audio Device"},
    {0x04, 0x02, "Computer Telephony Device"},
    {0x04, 0x80, "Other Multimedia Device"},
    {0x05, 0x00, "RAM Controller"},
    {0x05, 0x01, "Flash Controller"},
    {0x05, 0x80, "Other Memory Controller"},
    {0x06, 0x00, "Host Bridge"},
    {0x06, 0x01, "ISA Bridge"},
    {0x06, 0x02, "EISA Bridge"},
    {0x06, 0x03, "MCA Bridge"},
    {0x06, 0x04, "PCI-to-PCI Bridge"},
    {0x06, 0x05, "PCMCIA Bridge"},
    {0x06, 0x06, "NuBus Bridge"},
    {0x06, 0x07, "CardBus Bridge"},
    {0x06, 0x08, "RACEWay Bridge"},
    {0x06, 0x09, "PCI-to-PCI Bridge (Semi-Transparent)"},
    {0x06, 0x0A, "InfiniBand-to-PCI Host Bridge"},
    {0x06, 0x80, "Other Bridge Device"},
    {0x07, 0x00, "Serial Controller"},
    {0x07, 0x01, "Parallel Port"},
    {0x07, 0x02, "Multiport Serial Controller"},
    {0x07, 0x03, "Generic Modem"},
    {0x07, 0x04, "IEEE 488.1/2 (GPIB) Controller"},
    {0x07, 0x05, "Smart Card"},
    {0x07, 0x80, "Other Communication Device"},
    {0x08, 0x00, "Programmable Interrupt Controller"},
    {0x08, 0x01, "DMA Controller"},
    {0x08, 0x02, "System Timer"},
    {0x08, 0x03, "Real-Time Clock"},
    {0x08, 0x04, "Generic PCI Hot-Plug Controller"},
    {0x08, 0x80, "Other System Peripheral"},
    {0x09, 0x00, "Keyboard Controller"},
    {0x09, 0x01, "Digitizer"},
    {0x09, 0x02, "Mouse Controller"},
    {0x09, 0x03, "Scanner Controller"},
    {0x09, 0x04, "Gameport Controller"},
    {0x09, 0x80, "Other Input Controller"},
    {0x0A, 0x00, "Generic Docking Station"},
    {0x0A, 0x80, "Other Docking Station"},
    {0x0B, 0x00, "i386 CPU"},
    {0x0B, 0x01, "i486 CPU"},
    {0x0B, 0x02, "Pentium CPU"},
    {0x0B, 0x10, "Alpha CPU"},
    {0x0B, 0x20, "PowerPC CPU"},
    {0x0B, 0x30, "MIPS CPU"},
    {0x0B, 0x40, "Co-processor"},
    {0x0C, 0x00, "FireWire Controller"},
    {0x0C, 0x01, "ACCESS.bus Controller"},
    {0x0C, 0x02, "SSA Controller"},
    {0x0C, 0x03, "USB Controller"},
    {0x0C, 0x04, "Fibre Channel"},
    {0x0C, 0x05, "SMBus"},
    {0x0C, 0x06, "InfiniBand"},
    {0x0C, 0x07, "IPMI SMIC Interface"},
    {0x0C, 0x08, "SERCOS Interface"},
    {0x0C, 0x09, "CANbus Interface"},
    {0x0D, 0x00, "iRDA Compatible Controller"},
    {0x0D, 0x01, "Consumer IR Controller"},
    {0x0D, 0x10, "RF Controller"},
    {0x0D, 0x11, "Bluetooth Controller"},
    {0x0D, 0x12, "Broadband Controller"},
    {0x0D, 0x20, "802.11a (Wi-Fi) Ethernet Controller"},
    {0x0D, 0x21, "802.11b (Wi-Fi) Ethernet Controller"},
    {0x0D, 0x80, "Other Wireless Controller"},

    {0x00, 0x00, NULL}
};

static struct {
    uint16_t vendor;
    const char *name;
} pci_vendor_name_strings[] = {
    { PCI_VENDOR_INTEL, "Intel Corporation" },
    { PCI_VENDOR_NVIDIA, "NVIDIA" },
    { PCI_VENDOR_AMD, "Advanced Micro Devices Inc." },
    { PCI_VENDOR_REALTEK, "Realtek Semiconductor Corp." },
    { PCI_VENDOR_VIRTIO, "VirtIO" },
    { 0, NULL }
};

uint8_t
pci_read_confspc_byte(struct pci_addr_t addr, uint8_t offset) {
    assert(!(offset & 0));

    uint16_t word = pci_read_confspc_word(addr, offset & ~0b1);

    if (offset & 0b1) {
        word >>= 8;
    }

    return (uint8_t)(word & 0xff);
}

uint16_t
pci_read_confspc_word(struct pci_addr_t addr, uint8_t offset) {
    assert(!(offset & 0b1));

    uint32_t dword = pci_read_confspc_dword(addr, offset & ~0b11);

    if (offset & 0b10) {
        dword >>= 16;
    }

    return (uint16_t)(dword & 0xffff);
}

uint32_t
pci_read_confspc_dword(struct pci_addr_t addr, uint8_t offset) {
    assert(!(offset & 0b11));

    uint32_t address = ((uint32_t)0x80000000) |
        (((uint32_t)addr.bus ) << 16) |
        (((uint32_t)addr.slot) << 11) |
        (((uint32_t)addr.func) <<  8) |
        ((uint32_t)offset);

    outl(PCI_ADDRESS_PORT, address);
    return inl(PCI_DATA_PORT);
}

uint64_t
pci_read_confspc_qword(struct pci_addr_t addr, uint8_t offset) {
    assert(!(offset & 0b111));

    uint64_t dword0 = (uint64_t)pci_read_confspc_dword(addr, offset);
    uint64_t dword1 = (uint64_t)pci_read_confspc_dword(addr, offset + 4);

    return dword0 | (dword1 << 32);
}

void
pci_write_confspc_byte(struct pci_addr_t addr, uint8_t offset, uint8_t data) {
    assert(!(offset & 0));

    uint16_t word = pci_read_confspc_word(addr, offset & ~0b1);

    if (offset & 0b1) {
        word = (word & 0x00ff) | (((uint16_t)data) << 8);
    } else {
        word = (word & 0xff00) | ((uint16_t)data);
    }

    pci_write_confspc_word(addr, offset & ~0b1, word);
}

void
pci_write_confspc_word(struct pci_addr_t addr, uint8_t offset, uint16_t data) {
    assert(!(offset & 0b1));

    uint32_t dword = pci_read_confspc_dword(addr, offset & ~0b11);

    if (offset & 0b10) {
        dword = (dword & 0x0000ffff) | (((uint32_t)data) << 16);
    } else {
        dword = (dword & 0xffff0000) | ((uint32_t)data);
    }

    pci_write_confspc_word(addr, offset & ~0b11, dword);
}

void
pci_write_confspc_dword(struct pci_addr_t addr, uint8_t offset, uint32_t data) {
    assert(!(offset & 0b11));

    uint32_t address = ((uint32_t)0x80000000) |
        (((uint32_t)addr.bus ) << 16) |
        (((uint32_t)addr.slot) << 11) |
        (((uint32_t)addr.func) <<  8) |
        ((uint32_t)offset);

    outl(PCI_ADDRESS_PORT, address);
    outl(PCI_DATA_PORT, data);
}

void
pci_write_confspc_qword(struct pci_addr_t addr, uint8_t offset, uint64_t data) {
    assert(!(offset & 0b111));

    pci_write_confspc_dword(addr, offset    , (uint32_t)(data      ));
    pci_write_confspc_dword(addr, offset + 4, (uint32_t)(data << 32));
}


void pci_read_confspc_data(struct pci_addr_t addr, uint8_t offset, void *dst, unsigned size) {
    assert(dst);
    assert((unsigned)offset + size < 0x100);

    #define READ_FWD_(TYPE, NAME)                                       \
        if (size >= sizeof(TYPE)) {                                     \
            *(TYPE *)dst = pci_read_confspc_##NAME(addr, offset);       \
            dst += sizeof(TYPE);                                        \
            offset += sizeof(TYPE);                                     \
            size -= sizeof(TYPE);                                       \
        }

    if (offset & 0b01) { READ_FWD_(uint8_t,  byte); }
    if (offset & 0b10) { READ_FWD_(uint16_t, word); }

    while (size >= sizeof(uint32_t)) {
        *((uint32_t *)dst) = pci_read_confspc_dword(addr, offset);

        dst += sizeof(uint32_t);
        size -= sizeof(uint32_t);
        offset += sizeof(uint32_t);
    }

    READ_FWD_(uint16_t, word);
    READ_FWD_(uint8_t,  byte);

    #undef READ_FWD_

    assert(size == 0);
}

void pci_write_confspc_data(struct pci_addr_t addr, uint8_t offset, const void *src, unsigned size) {
    assert(src);
    assert((unsigned)offset + size < 0x100);

    #define WRITE_FWD_(TYPE, NAME)                                      \
        if (size >= sizeof(TYPE)) {                                     \
            pci_write_confspc_##NAME(addr, offset, *(TYPE *)src);       \
            src += sizeof(TYPE);                                        \
            offset += sizeof(TYPE);                                     \
            size -= sizeof(TYPE);                                       \
        }

    if (offset & 0b01) { WRITE_FWD_(uint8_t,  byte); }
    if (offset & 0b10) { WRITE_FWD_(uint16_t, word); }

    while (size >= sizeof(uint32_t)) {
        pci_write_confspc_dword(addr, offset, *(uint32_t *)src);

        src += sizeof(uint32_t);
        size -= sizeof(uint32_t);
        offset += sizeof(uint32_t);
    }

    WRITE_FWD_(uint16_t, word);
    WRITE_FWD_(uint8_t,  byte);

    #undef WRITE_FWD_

    assert(size == 0);
}


uint8_t
pci_get_class(uint8_t bus, uint8_t slot, uint8_t function) {
    return pci_read_confspc_byte((struct pci_addr_t){bus, slot, function}, offsetof(pci_header_00, class_id));
}

uint8_t
pci_get_subclass(uint8_t bus, uint8_t slot, uint8_t function) {
    return pci_read_confspc_byte((struct pci_addr_t){bus, slot, function}, offsetof(pci_header_00, subclass_id));
}

uint8_t
pci_get_hdr_type(uint8_t bus, uint8_t slot, uint8_t function) {
    return pci_read_confspc_byte((struct pci_addr_t){bus, slot, function}, offsetof(pci_header_00, hdr_type));
}

uint16_t
pci_get_vendor(uint8_t bus, uint8_t slot, uint8_t function) {
    return pci_read_confspc_word((struct pci_addr_t){bus, slot, function}, offsetof(pci_header_00, vendor_id));
}

uint16_t
pci_get_device(uint8_t bus, uint8_t slot, uint8_t function) {
    return pci_read_confspc_word((struct pci_addr_t){bus, slot, function}, offsetof(pci_header_00, device_id));
}

const char *
pci_get_device_type(uint8_t dev_class, uint8_t dev_subclass) {
    for(int i = 0; pci_device_type_strings[i].name != NULL; i++) {
        if(pci_device_type_strings[i].dev_class == dev_class && pci_device_type_strings[i].dev_subclass == dev_subclass) {
            return pci_device_type_strings[i].name;
        }
    }
    return NULL;
}

const char *
pci_get_vendor_name(uint16_t vendor, int* found) {
    for(int i=0; pci_vendor_name_strings[i].name != NULL; i++) {
        if(pci_vendor_name_strings[i].vendor == vendor) {
            *found = 1;
            return pci_vendor_name_strings[i].name;
        }
    }

    *found = 0;
    return "unknown";
}

uint64_t
pci_get_bar(uint8_t hdrtype, uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_number, uint8_t *bar_type) {
    uint8_t bar_type_placeholder = 0;

    if (!bar_type) {
        bar_type = &bar_type_placeholder;
    }

	if ((hdrtype & ~0x80) == 0) {
		uint8_t off = bar_number * sizeof(uint32_t);
		uint32_t bar = pci_read_confspc_dword((struct pci_addr_t){bus, slot, func}, offsetof(pci_header_00, bar) + off);

		if (!(bar & 1)) {
			if ((bar & 0b110) == 0b000) {
				*bar_type = 0;
				return bar & ~0b1111;

			} else if ((bar & 0b110) == 0b100) {
                uint32_t next_bar = pci_read_confspc_dword((struct pci_addr_t){bus, slot, func}, offsetof(pci_header_00, bar) + off + sizeof(uint32_t));

                *bar_type = 2;
                return (uint64_t)((bar & 0xFFFFFFF0) + ((uint64_t)(next_bar & 0xFFFFFFFF) << 32));
            }

            assert(false);
		} else  {
			*bar_type = 1;

			return bar & ~0b11;
		}
	}

    assert(false);
	return 0;
}

void
pci_find_device(uint16_t vendor, uint8_t device_class, uint8_t device_subclass, uint8_t *bus_ret, uint8_t *slot_ret, uint8_t *func_ret) {
    for(unsigned bus=0; bus<256; bus++) {
        for(unsigned slot=0; slot<32; slot++) {
            for(unsigned func=0; func<8; func++) {
                if(pci_get_class(bus, slot, func) == device_class &&
                    pci_get_subclass(bus, slot, func) == device_subclass &&
                    pci_get_vendor(bus, slot, func) == vendor)
                {
                    *bus_ret  = bus;
                    *slot_ret = slot;
                    *func_ret = func;
                    return;
                }
            }
        }
    }

    *bus_ret = *slot_ret = *func_ret = 0xFF;
}

void
pci_find_device_by_id(uint16_t vendor, uint16_t device_id, uint8_t *bus_ret, uint8_t *slot_ret, uint8_t *func_ret) {
    for (unsigned bus = 0; bus < 256; bus++) {
        for (unsigned slot = 0; slot < 32; slot++) {
            for (unsigned func = 0; func < 8; func++) {
                if (pci_get_device(bus, slot, func) == device_id &&
                    pci_get_vendor(bus, slot, func) == vendor) {

                    *bus_ret  = bus;
                    *slot_ret = slot;
                    *func_ret = func;
                    return;
                }
            }
        }
    }

    *bus_ret = *slot_ret = *func_ret = 0xFF;
}

void
pci_print_device(unsigned bus, unsigned slot, unsigned func) {
    uint8_t clid = pci_get_class(bus, slot, func);
    uint8_t sclid = pci_get_subclass(bus, slot, func);
    uint16_t vendor = pci_get_vendor(bus, slot, func);
    uint16_t hdrtype = pci_get_hdr_type(bus, slot, func);
    uint16_t device = pci_get_device(bus, slot, func);

    int found = 0;
    const char* vendor_name = pci_get_vendor_name(vendor, &found);
    if(found) {
        cprintf("%02u:%02u.%u %s: %s, device: 0x%04x, header type 0x%04x\n", bus, slot, func, pci_get_device_type(clid, sclid), vendor_name, device, hdrtype);
    } else {
        cprintf("%02u:%02u.%u %s: Vendor 0x%04x, device: 0x%04x, header type 0x%04x\n", bus, slot, func, pci_get_device_type(clid, sclid), vendor, device, hdrtype);
    }
}

void
list_pci() {
    uint8_t hdrtype;
    uint8_t clid;
    uint16_t vendor;

    cprintf("PCI device listing:\n");

    for (unsigned bus = 0; bus < 256; bus++) {
        for (unsigned slot = 0; slot < 32; slot++) {
            unsigned func = 0;
            hdrtype = pci_get_hdr_type(bus, slot, func);
            clid = pci_get_class(bus, slot, func);
            vendor = pci_get_vendor(bus, slot, func);

            if (clid != 0xFF && vendor != 0xFFFF) {
                pci_print_device(bus, slot, func);
            }

            if ((hdrtype & 0x80) == 0) {
                for (func = 1; func < 8; func++) {
                    clid = pci_get_class(bus, slot, func);
                    vendor = pci_get_vendor(bus, slot, func);

                    if (clid != 0xFF && vendor != 0xFFFF) {
                        pci_print_device(bus, slot, func);
                    }
                }
            }
        }
    }
}

void pci_header00_read(pci_header_00 *header, uint8_t bus, uint8_t slot, uint8_t func) {
    static_assert(sizeof(pci_header_00) % 8 == 0, "Bad pci header size");

    for(int i = 0 ; i < sizeof(pci_header_00); i += 8) {
        uint64_t data = pci_read_confspc_qword((struct pci_addr_t){bus, slot, func}, i);
        ((uint64_t *)header)[i / 8] = data;
    }
}

void pci_scan_msi_x_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_index) {
    // TODO: Fix. Broken as hell due to unaligned reads

    uint32_t table_offset = (uint32_t) pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index + 2) |
                            (uint32_t) pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index + 3) << 16;
    uint32_t pending_bit_offset = (uint32_t) pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index + 4) |
                                  (uint32_t) pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index + 5) << 16;
    uint8_t bir = table_offset & 3;
    uint8_t pending_bit_bir = pending_bit_offset & 3;

    table_offset &= ~3;
    pending_bit_offset &= ~3;

    cprintf("  - MSI-X Capability found: table_offset = 0x%08x, pending_bit_offset = 0x%08x, bir = %d, pending_bit_bit = %d\n",
            table_offset, pending_bit_offset, bir, pending_bit_bir);
}

void pci_scan_capabilities(uint8_t bus, uint8_t slot, uint8_t func, uint8_t capabilities) {
    uint8_t cap_index = capabilities;
    cap_index &= ~3;

    while(cap_index != 0) {
        uint16_t capability = pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index);

        uint8_t capability_index = (uint8_t) capability;
        uint8_t next_capability = (uint8_t)(capability >> 8);

        cprintf(" - [%02x]: Capability #%02x, metadata = %04x\n",
            cap_index, capability_index,
            pci_read_confspc_word((struct pci_addr_t){bus, slot, func}, cap_index + 2));

        if(capability_index == PCI_MSI_CAPABILITY) {
            // pci_scan_msi_x_capability(bus, slot, func, cap_index);
        }

        cap_index = next_capability;
    }
}

void pci_header00_print(pci_header_00* header) {
    cprintf(" - vendor_id: 0x%04x\n", header->vendor_id);
    cprintf(" - device_id: 0x%04x\n", header->device_id);
    cprintf(" - command: 0x%04x\n", header->command);
    cprintf(" - status: 0x%04x\n", header->status);
    cprintf(" - revision: 0x%02x\n", header->revision);
    cprintf(" - prog_if: 0x%02x\n", header->prog_if);
    cprintf(" - subclass_id: 0x%02x\n", header->subclass_id);
    cprintf(" - class_id: 0x%02x\n", header->class_id);
    cprintf(" - cache_line_size: 0x%02x\n", header->cache_line_size);
    cprintf(" - latency_timer: 0x%02x\n", header->latency_timer);
    cprintf(" - hdr_type: 0x%02x\n", header->hdr_type);
    cprintf(" - bist: 0x%02x\n", header->bist);
    cprintf(" - bar: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
            header->bar[0],
            header->bar[1],
            header->bar[2],
            header->bar[3],
            header->bar[4],
            header->bar[5]);
    cprintf(" - cardbus_cis_ptr: 0x%08x\n", header->cardbus_cis_ptr);
    cprintf(" - subsys_vendor: 0x%04x\n", header->subsys_vendor);
    cprintf(" - subsys_id: 0x%04x\n", header->subsys_id);
    cprintf(" - expansion_rom: 0x%08x\n", header->expansion_rom);
    cprintf(" - capabilities: 0x%02x\n", header->capabilities);
    cprintf(" - reserved: 0x%02x, 0x%02x, 0x%02x\n",
            header->reserved[0],
            header->reserved[1],
            header->reserved[2]);
    cprintf(" - reserved2: 0x%08x\n", header->reserved2);
    cprintf(" - int_line: 0x%02x\n", header->int_line);
    cprintf(" - int_pin: 0x%02x\n", header->int_pin);
    cprintf(" - min_grant: 0x%02x\n", header->min_grant);
    cprintf(" - max_latency: 0x%02x\n", header->max_latency);
}

int configure_virtio_vga() {
    uint8_t bus, slot, func;

    pci_find_device(PCI_VENDOR_VIRTIO, 0x03, 0x00, &bus, &slot, &func);

    if(func == 0xFF) return -1;

    cprintf("VirtIO VGA Controller found at %02u:%02u.%u\n", bus, slot, func);

    if(pci_get_hdr_type(bus, slot, func) != 0) {
        cprintf("VirtIO VGA Controller has invalid header type\n");
        return -1;
    }

    pci_header_00 controller_data;

    pci_header00_read(&controller_data, bus, slot, func);

    cprintf("Printing VirtIO VGA Controller data:\n");

    pci_header00_print(&controller_data);

    cprintf("Capabilities (%hhu):\n", controller_data.capabilities);

    pci_scan_capabilities(bus, slot, func, controller_data.capabilities);

    return 1;
}