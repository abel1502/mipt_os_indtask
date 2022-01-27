#ifndef JOS_KERN_VIRTIO_H
#define JOS_KERN_VIRTIO_H

#include <inc/types.h>
#include <kern/pci.h>


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


// Everything is little-endian, unless otherwise specified

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
};

struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    virtio_le32_t notify_off_multiplier; /* Multiplier for queue_notify_off. */
};


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
};


struct virtio_config {
    // TODO
};


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
};

struct virtq_avail {
    virtio_le16_t flags;
    virtio_le16_t idx;
    virtio_le16_t ring[];
    /* Only if VIRTIO_F_EVENT_IDX: virtio_le16_t used_event; */
};

/* le32 is used here for ids for padding reasons. */
struct virtq_used_elem {
    /* Index of start of used descriptor chain. */
    virtio_le32_t id;
    /* Total length of the descriptor chain which was written to. */
    virtio_le32_t len;
};

struct virtq_used {
    virtio_le16_t flags;
    virtio_le16_t idx;
    struct virtq_used_elem ring[];
    /* Only if VIRTIO_F_EVENT_IDX: virtio_le16_t avail_event; */
};

struct virtq {
    unsigned size;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
};


int 


#endif