#ifndef JOS_KERN_VIRTIO_H
#define JOS_KERN_VIRTIO_H

#include <inc/types.h>
#include <kern/pci.h>


/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER		2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK	4
/* Driver has finished configuring features */
#define VIRTIO_CONFIG_S_FEATURES_OK	8
/* Device entered invalid state, driver must reset it */
#define VIRTIO_CONFIG_S_NEEDS_RESET	0x40
/* We've given up on this device. */
#define VIRTIO_CONFIG_S_FAILED		0x80

/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG 1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG 3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG 5

/* Vector value used to disable MSI for queue */
#define VIRTIO_MSI_NO_VECTOR 0xffff

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as write-only (otherwise read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4

/* The device uses this in used->flags to advise the driver: don't kick me
* when you add a buffer. It's unreliable, so it's simply an
* optimization. */
#define VIRTQ_USED_F_NO_NOTIFY 1
/* The driver uses this in avail->flags to advise the device: don't
* interrupt me when you consume a buffer. It's unreliable, so it's
* simply an optimization. */
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
/* Support for indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC 28
/* Support for avail_event and used_event fields */
#define VIRTIO_F_EVENT_IDX 29
/* Arbitrary descriptor layouts. */
#define VIRTIO_F_ANY_LAYOUT 27

#define VIRTIO_PCI_VENDOR 0x1AF4
#define VIRTIO_PCI_DEVICE_ID_BASE 0x1040
#define VIRTIO_PCI_CAP_VENDOR 0x09
// VIRTIO_IRQ defined in trap.h

// Arbitrary limits to keep the size small
#define VIRTIO_MAX_DEVICES 0x8
#define VIRTIO_MAX_VIRTQS 0x8
#define VIRTIO_MAX_VQ_SIZE 0x1000


// Everything is little-endian, unless otherwise specified
// TODO: Somehow assert that our compiler is little-endian as well

typedef uint8_t  virtio_le8_t;
typedef uint16_t virtio_le16_t;
typedef uint32_t virtio_le32_t;
typedef uint64_t virtio_le64_t;


struct virtio_pci_cap {
    virtio_le8_t  cap_vndr; /* Generic PCI field: PCI_CAP_ID_VNDR */
    virtio_le8_t  cap_next; /* Generic PCI field: next ptr. */
    virtio_le8_t  cap_len; /* Generic PCI field: capability length */
    virtio_le8_t  cfg_type; /* Identifies the structure. */
    virtio_le8_t  bar; /* Where to find it. */
    virtio_le8_t  padding[3]; /* Pad to full dword. */
    virtio_le32_t offset; /* Offset within bar. */
    virtio_le32_t length; /* Length of the structure, in bytes. */
} __attribute__((packed));

struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    virtio_le32_t notify_off_multiplier; /* Multiplier for queue_notify_off. */
} __attribute__((packed));


struct virtio_pci_common_cfg {
    /* About the whole device. */
    virtio_le32_t device_feature_select; /* read-write */
    virtio_le32_t device_feature; /* read-only for driver */
    virtio_le32_t driver_feature_select; /* read-write */
    virtio_le32_t driver_feature; /* read-write */
    virtio_le16_t msix_config; /* read-write */
    virtio_le16_t num_queues; /* read-only for driver */
    virtio_le8_t  device_status; /* read-write */
    virtio_le8_t  config_generation; /* read-only for driver */
    
    /* About a specific virtqueue. */
    virtio_le16_t queue_select; /* read-write */
    virtio_le16_t queue_size; /* read-write */
    virtio_le16_t queue_msix_vector; /* read-write */
    virtio_le16_t queue_enable; /* read-write */
    virtio_le16_t queue_notify_off; /* read-only for driver */
    virtio_le64_t queue_desc; /* read-write */
    virtio_le64_t queue_driver; /* read-write */
    virtio_le64_t queue_device; /* read-write */
} __attribute__((packed));


/* Virtqueue descriptors: 16 bytes.
* These can chain together via "next". */
struct virtq_desc {
    /* Address (guest-physical). */
    virtio_le64_t addr;
    /* Length. */
    virtio_le32_t len;
    /* The flags as indicated above. */
    virtio_le16_t flags;
    /* We chain unused descriptors via this, too */
    virtio_le16_t next;
} __attribute__((packed));

struct virtq_avail {
    virtio_le16_t flags;
    virtio_le16_t idx;
    virtio_le16_t ring[];
    /* Only if VIRTIO_F_EVENT_IDX: virtio_le16_t used_event; */
} __attribute__((packed));

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
    /* Index of start of used descriptor chain. */
    virtio_le32_t id;
    /* Total length of the descriptor chain which was written to. */
    virtio_le32_t len;
} __attribute__((packed));

struct virtq_used {
    virtio_le16_t flags;
    virtio_le16_t idx;
    struct virtq_used_elem ring[];
    /* Only if VIRTIO_F_EVENT_IDX: virtio_le16_t avail_event; */
} __attribute__((packed));


struct virtq {
    struct virtio_device *device;
    unsigned idx;

    unsigned capacity;

    volatile bool waiting;

    volatile struct virtq_desc  *desc;
    volatile struct virtq_avail *avail;
    volatile struct virtq_used  *used;

    size_t notify_offset;

    uint32_t seen_used_idx;
};

typedef int vq_buf_handle;


struct virtio_device {
    struct pci_addr_t pci_device_addr;

    // ---- MMIO locations:
    // General configuration
    volatile struct virtio_pci_common_cfg *mmio_cfg;
    size_t mmio_cfg_size;

    // Notifications
    volatile void *mmio_ntf;
    size_t mmio_ntf_size;

    // ISR status
    volatile uint8_t *mmio_isr;
    size_t mmio_isr_size;

    // Device-specific configuration
    volatile void *mmio_dev;
    size_t mmio_dev_size;
    // ----

    unsigned cfg_active_generation;

    unsigned num_queues;
    struct virtq queues[VIRTIO_MAX_VIRTQS];
    uint32_t vq_notify_off_mul;

    // TODO
    void (*on_virtqs_update)(struct virtio_device *device);
};

// TODO: Some way to keep track of all deivces
extern struct virtio_device *virtio_devices;
extern unsigned virtio_num_devices;


void virtio_init();
struct virtio_device *virtio_create_device();
int virtio_init_device(struct virtio_device *device, uint16_t virtio_device_id);
bool virtio_is_cfg_up_to_date(struct virtio_device *device);
void virtio_mark_cfg_up_to_date(struct virtio_device *device);
void virtio_reset(struct virtio_device *device);
uint8_t virtio_get_status(struct virtio_device *device);
void virtio_fail(struct virtio_device *device);
bool virtio_needs_reset(struct virtio_device *device);
// TODO: Implement deinit?
void virtio_deinit(struct virtio_device *device);
// TODO: Handle incoming notifications
void virtio_intr();

// Negative values should be treated as standard errors
// vq_buf_handle virtq_push(struct virtq *queue, const char *data, unsigned size, unsigned max_resp_size);
// vq_buf_handle virtq_next_handle(struct virtq *queue);
// int virtq_peek(struct virtq *queue, vq_buf_handle handle, char **buf, unsigned *limit);
// int virtq_pop(struct virtq *queue);

void virtq_request(struct virtq *queue, physaddr_t buf, unsigned size, unsigned resp_offs);

// TODO: other methods


#endif