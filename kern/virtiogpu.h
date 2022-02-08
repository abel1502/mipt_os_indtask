#ifndef JOS_KERN_VIRTIOGPU_H
#define JOS_KERN_VIRTIOGPU_H

#include <inc/types.h>
#include <inc/assert.h>
#include <kern/pmap.h>

#include <kern/virtio.h>

#define VIRTIO_GPU_DEVICE_ID 16

#define VIRTIO_GPU_MAX_SCANOUTS 16
#define VIRTIO_GPU_FLAG_FENCE   (1 << 0)

// The size of the single page that would contain everything gpu-related in a single physically contiguous region
#define VIRTIO_GPU_CONTROLPAGE_CLASS 8
#define VIRTIO_GPU_CONTROLPAGE_SIZE  CLASS_SIZE(VIRTIO_GPU_CONTROLPAGE_CLASS)


enum virtio_gpu_ctrl_type {
    /* 2d commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,
    /* cursor commands */
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,
    /* success responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,
    /* error responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

enum virtio_gpu_formats {
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,
    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,
    VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
    VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
};

struct virtio_gpu_ctrl_hdr {
    virtio_le32_t type;
    virtio_le32_t flags;
    virtio_le64_t fence_id;
    virtio_le32_t ctx_id;
    virtio_le32_t padding;
} __attribute__((packed));

struct virtio_gpu_rect {
    virtio_le32_t x;
    virtio_le32_t y;
    virtio_le32_t width;
    virtio_le32_t height;
} __attribute__((packed));

struct virtio_gpu_get_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
} __attribute__((packed));

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;

    struct virtio_gpu_display_one {
        struct virtio_gpu_rect r;
        virtio_le32_t enabled;
        virtio_le32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed));

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;

    virtio_le32_t resource_id;
    virtio_le32_t format;
    virtio_le32_t width;
    virtio_le32_t height;
} __attribute__((packed));

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    virtio_le32_t scanout_id;
    virtio_le32_t resource_id;
} __attribute__((packed));

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    virtio_le32_t resource_id;
    virtio_le32_t padding;
} __attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    virtio_le64_t offset;
    virtio_le32_t resource_id;
    virtio_le32_t padding;
} __attribute__((packed));

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    virtio_le32_t resource_id;
    virtio_le32_t nr_entries;
} __attribute__((packed));

struct virtio_gpu_mem_entry {
    virtio_le64_t addr;
    virtio_le32_t length;
    virtio_le32_t padding;
} __attribute__((packed));

struct virtio_gpu_bigfngreq {
    struct {
        // VIRTIO_GPU_CMD_GET_DISPLAY_INFO
        struct virtio_gpu_get_display_info req;
        // VIRTIO_GPU_RESP_OK_DISPLAY_INFO
        struct virtio_gpu_resp_display_info resp;
    } display_info;

    struct {
        // VIRTIO_GPU_CMD_RESOURCE_CREATE_2D  // 640x400, VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM
        struct virtio_gpu_resource_create_2d create;
        struct virtio_gpu_ctrl_hdr create_resp;
        // VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING
        struct virtio_gpu_resource_attach_backing attach;
        struct virtio_gpu_mem_entry attachment_page;
        struct virtio_gpu_ctrl_hdr attach_resp;
        // VIRTIO_GPU_CMD_SET_SCANOUT
        struct virtio_gpu_set_scanout scanout;
        struct virtio_gpu_ctrl_hdr scanout_resp;
    } init;

    struct {
        // VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D
        struct virtio_gpu_transfer_to_host_2d transfer;
        struct virtio_gpu_ctrl_hdr transfer_resp;
        // VIRTIO_GPU_CMD_RESOURCE_FLUSH
        struct virtio_gpu_resource_flush flush;
        struct virtio_gpu_ctrl_hdr flush_resp;
    } flush;
} __attribute__((packed));
// size = ?

// Also need a framebuffer somewhere in (almost?) contiguous memory
// size = 0x3E800 * 4 - almost 0x100000 - class 8

// In fact, both should fit inside a class 8 page
static_assert(sizeof(struct virtio_gpu_bigfngreq) + 
              (640 * 400 * sizeof(virtio_le32_t)) <= VIRTIO_GPU_CONTROLPAGE_SIZE, 
              "Bad size");


extern struct virtio_device *virtio_gpu_device;
extern uint32_t *virtio_gpu_fb;


void virtio_gpu_init();

int virtio_gpu_flush();


#endif
