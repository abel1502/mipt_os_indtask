
#include "pci.h"
#include <inc/x86.h>
#include <inc/stdio.h>
#include <stdint.h>

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

void pci_write_confspc_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint16_t data)
{
    uint32_t addr;
    uint32_t bus32 = bus;
    uint32_t slot32 = slot;
    uint32_t func32 = function;
    addr = (uint32_t)((bus32 << 16) | (slot32 << 11) |
                      (func32 << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

    outl(PCI_ADDRESS_PORT, addr);
    uint32_t current_data = inl(PCI_DATA_PORT);
    if(offset & 1) {
        current_data &= 0x0000FFFF;
        current_data |= data << 8;
    } else {
        current_data &= 0xFFFF0000;
        current_data |= data;
    }
    outl(PCI_DATA_PORT, current_data);
}

uint16_t pci_read_confspc_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t addr;
    uint32_t bus32 = bus;
    uint32_t slot32 = slot;
    uint32_t func32 = function;
    addr = (uint32_t)((bus32 << 16) | (slot32 << 11) |
                      (func32 << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(PCI_ADDRESS_PORT, addr);
    return (uint16_t)((inl(PCI_DATA_PORT) >> ((offset & 2) * 8)) & 0xffff);
}

uint8_t
pci_get_class(uint8_t bus, uint8_t slot, uint8_t function) {
	return (uint8_t) (pci_read_confspc_word(bus, slot, function, 10) >> 8) /* shifting to leave only class id in the variable. */;
}

uint8_t
pci_get_subclass(uint8_t bus, uint8_t slot, uint8_t function) {
	return (uint8_t) pci_read_confspc_word(bus, slot, function, 10);
}

uint8_t
pci_get_hdr_type(uint8_t bus, uint8_t slot, uint8_t function) {
	return (uint8_t) (pci_read_confspc_word(bus, slot, function, 0x0E));
}

uint16_t
pci_get_vendor(uint8_t bus, uint8_t slot, uint8_t function) {
	return pci_read_confspc_word(bus, slot, function, 0);
}

uint16_t
pci_get_device(uint8_t bus, uint8_t slot, uint8_t function) {
	return pci_read_confspc_word(bus, slot, function, 2);
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

uint32_t
pci_get_bar(uint8_t hdrtype, uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_number, uint8_t *bar_type) {
	if((hdrtype & ~0x80) == 0)
	{
		uint8_t off = bar_number * 2;
		uint16_t bar_low  = pci_read_confspc_word(bus, slot, func, 0x10 + off);
		uint16_t bar_high = pci_read_confspc_word(bus, slot, func, 0x10 + off + 1);
		if((bar_low & 1) == 0)
		{
			if((bar_low & ~0b110) == 0x00) //32-bit bar, we don't support other :P
			{
				uint32_t ret = (uint32_t) bar_low | (uint32_t) (bar_high << 16);
				ret &= ~0b1111;
				*bar_type = 0;
				return ret;
			}
		}
		if((bar_low & 1) == 1)
		{
			uint32_t ret = (uint32_t) bar_low | (uint32_t) (bar_high << 16);
			ret &= ~0b11;
			*bar_type = 1;
			return ret;
		}
	}
	return 0;
}

void pci_find_device(uint16_t vendor, uint8_t device_class, uint8_t device_subclass, uint8_t *bus_ret, uint8_t *slot_ret, uint8_t *func_ret)
{
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

	for(unsigned bus = 0; bus < 256; bus++)
	{
		for(unsigned slot = 0; slot < 32; slot++)
		{
			unsigned func = 0;
            hdrtype = pci_get_hdr_type(bus, slot, func);
            clid = pci_get_class(bus, slot, func);
            vendor = pci_get_vendor(bus, slot, func);

			if(clid != 0xFF && vendor != 0xFFFF) {
				pci_print_device(bus, slot, func);
			}

			if((hdrtype & 0x80) == 0) {
				for(func = 1; func < 8; func++) {
                    clid = pci_get_class(bus, slot, func);
                    vendor = pci_get_vendor(bus, slot, func);

					if(clid != 0xFF && vendor != 0xFFFF) {
						pci_print_device(bus, slot, func);
					}
				}
			}
		}
	}
}

void pci_header00_read(pci_header_00* header, uint8_t bus, uint8_t slot, uint8_t func) {
    for(int i = 0 ; i < sizeof(pci_header_00); i += 2) {
        uint16_t data = pci_read_confspc_word(bus, slot, func, i);
        ((char*)header)[i]     = (uint8_t)(data);
        ((char*)header)[i + 1] = (uint8_t)(data >> 8);
    }
}

void pci_scan_msi_x_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_index) {
    uint32_t table_offset       = (uint32_t) pci_read_confspc_word(bus, slot, func, cap_index + 2) |
                            (uint32_t) pci_read_confspc_word(bus, slot, func, cap_index + 3) << 16;
    uint32_t pending_bit_offset = (uint32_t) pci_read_confspc_word(bus, slot, func, cap_index + 4) |
                                  (uint32_t) pci_read_confspc_word(bus, slot, func, cap_index + 5) << 16;
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
        uint16_t capability = pci_read_confspc_word(bus, slot, func, cap_index);

        uint8_t capability_index = (uint8_t) capability;
        uint8_t next_capability = (uint8_t)(capability >> 8);

        cprintf(" - [%02x]: Capability #%02x, metadata = %04x\n", cap_index, capability_index, (uint16_t) pci_read_confspc_word(bus, slot, func, cap_index + 1));

        if(capability_index == PCI_MSI_CAPABILITY) {
            pci_scan_msi_x_capability(bus, slot, func, cap_index);
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

    cprintf("Capabilities:\n");

    pci_scan_capabilities(bus, slot, func, controller_data.capabilities);

    return 1;
}