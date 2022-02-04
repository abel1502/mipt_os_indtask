#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/trap.h>
#include <kern/picirq.h>
#include <kern/pmap.h>
#include <kern/virtio.h>


#define MEM_BARRIER()  asm volatile("mfence":::"memory")


struct virtio_device *virtio_devices = NULL;
unsigned virtio_num_devices = 0;


void
virtio_init() {
    assert(virtio_devices == 0);

    virtio_devices = kzalloc_region(sizeof(struct virtio_device) * VIRTIO_MAX_DEVICES);
    virtio_num_devices = 0;

    return;
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
        device->mmio_cfg = (volatile struct virtio_pci_common_cfg *)mmio_map_region(addr, cur_cap->length);
        device->mmio_cfg_size = cur_cap->length;
    } break;

    case VIRTIO_PCI_CAP_NOTIFY_CFG: {
        device->mmio_ntf = (volatile void *)mmio_map_region(addr, cur_cap->length);
        device->mmio_ntf_size = cur_cap->length;
        device->vq_notify_off_mul = pci_read_confspc_dword(device->pci_device_addr, followup_offset);
    } break;

    case VIRTIO_PCI_CAP_ISR_CFG: {
        device->mmio_isr = (volatile uint8_t *)mmio_map_region(addr, cur_cap->length);
        device->mmio_isr_size = cur_cap->length;
    } break;

    case VIRTIO_PCI_CAP_DEVICE_CFG: {
        device->mmio_dev = (volatile void *)mmio_map_region(addr, cur_cap->length);
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

            seen_caps[cur_cap.cfg_type] = 1;
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
    assert(device->mmio_cfg);

    unsigned vq_cnt = device->mmio_cfg->num_queues;

    if (vq_cnt > VIRTIO_MAX_VIRTQS) {
        return -E_INVAL;
    }

    device->num_queues = vq_cnt;

    for (unsigned i = 0; i < vq_cnt; ++i) {
        device->mmio_cfg->queue_select = i;

        unsigned vq_size = device->mmio_cfg->queue_size;
        assert(vq_size);
        
        if (vq_size > VIRTIO_MAX_VQ_SIZE) {
            vq_size = VIRTIO_MAX_VQ_SIZE;
            device->mmio_cfg->queue_size = vq_size;
        }

        device->mmio_cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;

        device->queues[i].idx = i;
        device->queues[i].capacity = vq_size;
        device->queues[i].notify_offset = device->mmio_cfg->queue_notify_off;
        device->queues[i].seen_used_idx = 0;
        device->queues[i].waiting = false;

        /*
          Now we allocate the queue parts. We're gonna use the split virtq format.
          The regions are listed in a table below. It's important for every one of
          them to be physically contiguous in memory, and I'm not quite sure how 
          to ensure that...
          I guess kzalloc_region with a size equaling to CLASS_SIZE(...)
          provides a contiguous segment of space...
        */
        // |------------------|-----------|---------------------|
        // | Virtqueue Part   | Alignment | Size                |
        // |------------------|-----------|---------------------|
        // | Descriptor Table | 16        | 16∗(Queue Size)     |
        // | Available Ring   | 2         | 6 + 2∗(Queue Size)  |
        // | Used Ring        | 4         | 6 + 8∗(Queue Size)  |
        // |------------------|-----------|---------------------|

        // Page-aligned, so 16 as well.
        size_t desc_size = 16 * vq_size;
        size_t desc_offs = 0;
        // 16-aligned, so 2 as well
        size_t avail_size = 6 + 2 * vq_size;
        size_t avail_offs = desc_offs + desc_size;
        // 2-aligned, might need a 2-byte padding
        size_t used_size = 6 + 8 * vq_size;
        size_t used_offs = avail_offs + avail_size;
        if (used_offs % 4) {
            used_offs += 2;
            assert(used_offs % 4 == 0);
        }

        size_t total_min_size = used_offs + used_size;
        unsigned class = 0;
        while (class < MAX_CLASS && CLASS_SIZE(class) < total_min_size) {
            ++class;
        }
        assert(class < MAX_CLASS);

        size_t total_size = CLASS_SIZE(class);

        cprintf("Allocating %llu bytes-large page for a virtiqueue\n", CLASS_SIZE(class));

        volatile void *vq_region = kzalloc_region(total_size);
        physaddr_t vq_phys_region = lookup_physaddr((void *)vq_region, total_size);

        device->queues[i].desc  = (volatile struct virtq_desc  *)(vq_region + desc_offs);
        device->queues[i].avail = (volatile struct virtq_avail *)(vq_region + avail_offs);
        device->queues[i].used  = (volatile struct virtq_used  *)(vq_region + used_offs);

        device->mmio_cfg->queue_desc   = vq_phys_region + desc_offs;
        device->mmio_cfg->queue_driver = vq_phys_region + avail_offs;
        device->mmio_cfg->queue_device = vq_phys_region + used_offs;
    }

    return 0;
}

static void
virtio_on_virtqs_update(struct virtio_device *device) {
    assert(device);

    for (unsigned i = 0; i < device->num_queues; ++i) {
        struct virtq *queue = &device->queues[i];

        if (queue->used->idx > queue->seen_used_idx) {
            assert(queue->used->idx == queue->seen_used_idx + 1);
            assert(queue->waiting);

            queue->seen_used_idx = queue->used->idx;
            queue->waiting = false;
        }
    }
}

int
virtio_init_device(struct virtio_device *device, uint16_t virtio_device_id) {
    assert(device);

    int res = 0;

    memset(device, 0, sizeof(*device));

    device->on_virtqs_update = &virtio_on_virtqs_update;

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
    device->mmio_cfg->msix_config = VIRTIO_MSI_NO_VECTOR;

    res = virtio_init_device_virtqs(device);
    if (res < 0) {
        return res;
    }

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

void
virtio_intr() {
    assert(virtio_devices);

    for (unsigned i = 0; i < virtio_num_devices; ++i) {
        struct virtio_device *device = &virtio_devices[i];
        uint8_t isr = *device->mmio_isr;

        if (isr & 0b01) {
            cprintf("Virtio device %p virtq used bufs returned\n", device);

            device->on_virtqs_update(device);

            // Deliberately no `continue`
        }

        if (isr & 0b10) {
            cprintf("Virtio device %p cfg update\n", device);
        }
    }

    // Do I even need it?
    // pic_send_eoi(IRQ_VIRTIO);
    cprintf("Virtio notification hit");
}


void
virtq_request(struct virtq *queue, physaddr_t buf, unsigned size, unsigned resp_offs) {
    assert(queue);
    assert(size >= resp_offs);
    assert(!queue->waiting);

    queue->waiting = true;

    queue->desc[0] = (struct virtq_desc){buf, resp_offs, VIRTQ_DESC_F_NEXT, 1};
    queue->desc[1] = (struct virtq_desc){buf + resp_offs, size - resp_offs, VIRTQ_DESC_F_WRITE, 0};

    queue->avail->ring[queue->avail->idx] = 0;

    MEM_BARRIER();

    queue->avail->idx++;

    MEM_BARRIER();

    if (queue->avail->flags & VIRTQ_AVAIL_F_NO_INTERRUPT) {
        *(volatile virtio_le16_t *)(
            queue->device->mmio_ntf + 
            queue->notify_offset * queue->device->vq_notify_off_mul
        ) = queue->idx;
    }

    // Probably suboptimal, but well sorry
    while (queue->waiting) {
        asm volatile ("pause");
    }

    // And now the result must have been provided
}

/*
vq_buf_handle
virtq_push(struct virtq *queue, const char *data, unsigned size, unsigned max_resp_size);

vq_buf_handle
virtq_next_handle(struct virtq *queue);

int
virtq_peek(struct virtq *queue, vq_buf_handle handle, char **buf, unsigned *limit);

int
virtq_pop(struct virtq *queue);
*/
