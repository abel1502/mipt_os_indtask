#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/trap.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/virtio.h>


struct virtio_device *virtio_devices = NULL;
unsigned virtio_num_devices = 0;


void
virtio_init() {
    assert(virtio_devices == 0);

    virtio_devices = kzalloc_region(sizeof(struct virtio_device) * VIRTIO_MAX_DEVICES);
    virtio_num_devices = 0;

    return 0;
}

struct virtio_device *
virtio_create_device() {
    assert(virtio_devices);

    if (virtio_num_devices == VIRTIO_MAX_DEVICES) {
        return NULL;
    }

    return &virtio_devices[virtio_num_devices++];
}


static int
virtio_process_capability(struct virtio_device *device, struct virtio_pci_cap *cur_cap, uint8_t followup_offset) {
    assert(device);
    assert(cur_cap);

    if (cur_cap->cap_vndr != VIRTIO_PCI_CAP_VENDOR) {
        return -E_INVAL;
    }

    physaddr_t addr = pci_get_bar(
        0,  /* TODO: Actually extract the header */
        device->pci_device_addr.bus,
        device->pci_device_addr.slot,
        device->pci_device_addr.func,
        cur_cap->bar,
        NULL  /* TODO: Maybe record the bar type? */
    ) + cur_cap->offset;

    switch (cur_cap->cfg_type) {
    case VIRTIO_PCI_CAP_COMMON_CFG: {
        device->mmio_cfg = (struct virtio_pci_common_cfg *)mmio_map_region(addr, cur_cap->length);
        device->mmio_cfg_size = cur_cap->length;
    } break;

    case VIRTIO_PCI_CAP_NOTIFY_CFG: {
        device->mmio_ntf = (void *)mmio_map_region(addr, cur_cap->length);
        device->mmio_ntf_size = cur_cap->length;
        device->vq_notify_off_mul = pci_read_confspc_dword(device->pci_device_addr, followup_offset);
    } break;

    case VIRTIO_PCI_CAP_ISR_CFG: {
        device->mmio_isr = (uint8_t *)mmio_map_region(addr, cur_cap->length);
        device->mmio_isr_size = cur_cap->length;
    } break;

    case VIRTIO_PCI_CAP_DEVICE_CFG: {
        device->mmio_dev = (void *)mmio_map_region(addr, cur_cap->length);
        device->mmio_dev_size = cur_cap->length;
    } break;

    case VIRTIO_PCI_CAP_PCI_CFG: {
        // Unused
    } break;

    default:
        break;
    }

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

        pci_read_confspc_data(device->pci_device_addr, cur_offset, &cur_cap, sizeof(cur_cap));

        assert(cur_cap.cap_len >= sizeof(cur_cap));

        if (cur_cap.cfg_type <= VIRTIO_PCI_CAP_PCI_CFG &&
            !seen_caps[cur_cap.cfg_type]) {
            res = virtio_process_capability(device, &cur_cap, cur_offset);
            if (res < 0) {
                return res;
            }
        }

        cur_offset = cur_cap.cap_next;
    }

    for (unsigned i = VIRTIO_PCI_CAP_COMMON_CFG; i <= VIRTIO_PCI_CAP_PCI_CFG; ++i)
    if (!seen_caps[i]) {
        return -E_INVAL;
    }

    return 0;
}

static int
virtio_init_device_virtqs(struct virtio_device *device) {
    assert(device);

    return 0;
}

int
virtio_init_device(struct virtio_device *device, uint16_t virtio_device_id) {
    assert(device);

    int res = 0;

    memset(device, 0, sizeof(*device));

    // 0. Discover the device. DONE
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

    // 1. Reset the device.
    virtio_reset(device);

    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    // 3. Set the DRIVER status bit: the guest OS knows how to drive the device.
    device->mmio_cfg->device_status |=
        VIRTIO_CONFIG_S_ACKNOWLEDGE |
        VIRTIO_CONFIG_S_DRIVER;

    // 4. Read device feature bits, and write the subset of feature bits understood by the OS and driver to the
    // device. During this step the driver MAY read (but MUST NOT write) the device-specific configuration
    // fields to check that it can support the device before accepting it.
    device->mmio_cfg->driver_feature_select = 0;  // The first page is the only one in use so far
    device->mmio_cfg->driver_feature = 0;  // And (hopefully) we can live without any optional features

    // 5. Set the FEATURES_OK status bit. The driver MUST NOT accept new feature bits after this step.
    device->mmio_cfg->device_status |= VIRTIO_CONFIG_S_FEATURES_OK;

    // 6. Re-read device status to ensure the FEATURES_OK bit is still set: otherwise, the device does not
    // support our subset of features and the device is unusable.
    if (!(device->mmio_cfg->device_status &
          VIRTIO_CONFIG_S_FEATURES_OK)) {
        // This shouldn't have failed, but ok
        return -E_NOT_SUPP;
    }

    // 7. Perform device-specific setup, including discovery of virtqueues for the device, optional per-bus setup,
    // reading and possibly writing the device’s virtio configuration space, and population of virtqueues.
    // TODO: Finish!!!

    // 8. Set the DRIVER_OK status bit. At this point the device is “live”.
    device->mmio_cfg->device_status |= VIRTIO_CONFIG_S_DRIVER_OK;

    pic_irq_unmask(IRQ_VIRTIO);

    return 0;
}

void
virtio_reset(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    device->mmio_cfg->device_status = 0;
}

uint8_t
virtio_get_status(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    return device->mmio_cfg->device_status;
}

bool
virtio_is_cfg_up_to_date(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    return device->mmio_cfg->config_generation == device->cfg_active_generation;
}

void
virtio_mark_cfg_up_to_date(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    device->cfg_active_generation = device->mmio_cfg->config_generation;
}

void
virtio_fail(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    device->mmio_cfg->device_status |= VIRTIO_CONFIG_S_FAILED;
}

bool
virtio_needs_reset(struct virtio_device *device) {
    assert(device);
    assert(device->mmio_cfg);

    return device->mmio_cfg->device_status & VIRTIO_CONFIG_S_NEEDS_RESET;
}

static void
virtio_on_virtqs_update(struct virtio_device *device) {
    cprintf("Virtio device %p virtq used bufs returned\n", device);

    // TODO: Actually process
}

static void
virtio_on_cfg_update(struct virtio_device *device) {
    cprintf("Virtio device %p cfg update\n", device);
}

void
virtio_intr() {
    assert(virtio_devices);

    for (unsigned i = 0; i < virtio_num_devices; ++i) {
        struct virtio_device *device = &virtio_devices[i];
        uint8_t isr = *device->mmio_isr;

        if (isr & 0b01) {
            virtio_on_virtqs_update(device);
        }

        if (isr & 0b10) {
            virtio_on_cfg_update(device);
        }
    }

    // Do I even need it?
    // pic_send_eoi(IRQ_VIRTIO);
    cprintf("Virtio notification hit");
}