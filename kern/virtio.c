#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/virtio.h>


static int
virtio_process_capability(struct virtio_device *device, struct virtio_pci_cap *cur_cap) {
    assert(device);
    assert(cur_cap);

    // TODO: Actually process

    return 0;
}

static int
virtio_identify_capabilities(struct virtio_device *device) {
    assert(device);

    int res = 0;

    pci_header_00 pci_header;
    pci_header00_read(&pci_header,
        device->pci_device_addr.bus,
        device->pci_device_addr.slot,
        device->pci_device_addr.func);

    if (!(pci_header.status & PCI_STATUS_CAPS_LIST)) {
        return -E_INVAL;
    }

    uint8_t cur_offset = pci_header.capabilities;
    assert(!(cur_offset & 0b11));

    // WARNING: Semi-hardcoded size!
    bool seen_caps[VIRTIO_PCI_CAP_PCI_CFG + 1] = {};

    while (cur_offset) {
        struct virtio_pci_cap cur_cap;
        static_assert(sizeof(cur_cap) % sizeof(uint32_t) == 0, "Bad struct size");

        for (unsigned i = 0; i < sizeof(cur_cap) / sizeof(uint32_t); ++i) {
            ((uint32_t *)&cur_cap)[i] = pci_read_confspc_dword(device->pci_device_addr, cur_offset + i * sizeof(uint32_t));
        }

        assert(cur_cap.cap_len >= sizeof(cur_cap));

        if (cur_cap.cfg_type <= VIRTIO_PCI_CAP_PCI_CFG &&
            !seen_caps[cur_cap.cfg_type]) {
            res = virtio_process_capability(device, &cur_cap);
            if (res < 0) {
                return res;
            }
        }

        cur_offset = cur_cap.cap_next;
    }

    return 0;
}

int
virtio_init(struct virtio_device *device, uint16_t virtio_device_id) {
    assert(device);

    int res = 0;

    // 0. Discover the device
    pci_find_device_by_id(VIRTIO_PCI_VENDOR, VIRTIO_PCI_DEVICE_ID_BASE + virtio_device_id,
                          &device->pci_device_addr.bus,
                          &device->pci_device_addr.slot,
                          &device->pci_device_addr.func);
    
    if (!is_valid_pci_addr(&device->pci_device_addr)) {
        return -E_NO_ENT;
    }

    res = virtio_identify_capabilities(device);
    if (res < 0) {
        return res;
    }

    // 1. Reset the device

    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.

    // 3. Set the DRIVER status bit: the guest OS knows how to drive the device.

    // 4. Read device feature bits, and write the subset of feature bits understood by the OS and driver to the
    // device. During this step the driver MAY read (but MUST NOT write) the device-specific configuration
    // fields to check that it can support the device before accepting it.

    // 5. Set the FEATURES_OK status bit. The driver MUST NOT accept new feature bits after this step.

    // 6. Re-read device status to ensure the FEATURES_OK bit is still set: otherwise, the device does not
    // support our subset of features and the device is unusable.

    // 7. Perform device-specific setup, including discovery of virtqueues for the device, optional per-bus setup,
    // reading and possibly writing the device’s virtio configuration space, and population of virtqueues.

    // 8. Set the DRIVER_OK status bit. At this point the device is “live”.

    return 0;
}