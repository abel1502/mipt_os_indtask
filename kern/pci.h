#ifndef PCI_H
#define PCI_H
#include <stdint.h>
#define CLASS_DEVICE_TOO_OLD 0x00
#define CLASS_MASS_STORAGE 0x01
#define CLASS_NETWORK_CTRLR 0x02
#define CLASS_DISPLAY 0x03
#define CLASS_MULTIMEDIA 0x04
#define CLASS_MEMORY 0x05
#define CLASS_BRIDGE 0x06
#define CLASS_SCC 0x07
#define CLASS_SYSTEM 0x08
#define CLASS_INPUT 0x09
#define CLASS_DOCK 0x0A
#define CLASS_PROCESSOR 0x0B
#define CLASS_SERIAL_BUS 0x0C
#define CLASS_WIRELESS 0x0D
#define CLASS_INTELLIGENTIO 0x0E
#define CLASS_SATELLITE 0x0F
#define CLASS_ENCRYPT 0x10
#define CLASS_NO_DEVICE 0xFF

#define PCI_ADDRESS_PORT 0xCF8
#define PCI_DATA_PORT 0xCFC

#define PCI_VENDOR_NO_DEVICE 0xFFFF

#define PCI_VENDOR_VIRTIO 0x1AF4
#define PCI_VENDOR_INTEL 0x8086
#define PCI_VENDOR_NVIDIA 0x10DE
#define PCI_VENDOR_AMD 0x1002
#define PCI_VENDOR_REALTEK 0x10EC

#define PCI_MSI_CAPABILITY 0x11

typedef struct pci_header_00 {
	uint16_t vendor_id;
	uint16_t device_id;
    uint16_t command;
    uint16_t status;
	uint8_t  revision;
	uint8_t  prog_if;
	uint8_t  subclass_id;
	uint8_t  class_id;
	uint8_t  cache_line_size;
	uint8_t  latency_timer;
	uint8_t  hdr_type;
	uint8_t  bist;
	uint32_t bar[6];
	uint32_t cardbus_cis_ptr;
	uint16_t subsys_vendor;
	uint16_t subsys_id;
	uint32_t expansion_rom;
	uint8_t  capabilities;
	uint8_t  reserved[3];
	uint32_t reserved2;
	uint8_t  int_line;
	uint8_t  int_pin;
	uint8_t  min_grant;
	uint8_t  max_latency;
} pci_header_00;

void pci_header00_read(pci_header_00* header, uint8_t bus, uint8_t slot, uint8_t func);
void pci_header00_print(pci_header_00* header);

void pci_scan_msi_x_capability(uint8_t bus, uint8_t slot, uint8_t func, uint8_t cap_index);
void pci_scan_capabilities(uint8_t bus, uint8_t slot, uint8_t func, uint8_t capabilities);

void list_pci();
void pci_find_device(uint16_t vendor, uint8_t device_class, uint8_t device_subclass, uint8_t *bus_ret, uint8_t *slot_ret, uint8_t *func_ret);
uint32_t pci_get_bar(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t *);
uint8_t pci_get_class(uint8_t, uint8_t, uint8_t);
uint8_t pci_get_subclass(uint8_t, uint8_t, uint8_t);
uint8_t pci_get_hdr_type(uint8_t, uint8_t, uint8_t);
uint16_t pci_get_vendor(uint8_t, uint8_t, uint8_t);
uint16_t pci_get_device(uint8_t, uint8_t, uint8_t);
const char *pci_get_device_type(uint8_t, uint8_t);
uint16_t pci_read_confspc_word(uint8_t, uint8_t, uint8_t, uint8_t);
void pci_write_confspc_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint16_t data);
int configure_virtio_vga();

#endif
