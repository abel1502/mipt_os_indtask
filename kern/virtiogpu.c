#include <inc/error.h>

#include <kern/virtiogpu.h>


struct virtio_device *virtio_gpu_device = NULL;
static physaddr_t virtio_gpu_reqpage_phys = 0;
static volatile struct virtio_gpu_bigfngreq *virtio_gpu_reqpage = NULL;
uint32_t *virtio_gpu_fb = NULL;


static void virtio_gpu_init_header() {
    assert(!virtio_gpu_reqpage);

    static_assert(VIRTIO_GPU_CONTROLPAGE_SIZE == VIRTIO_FRAMEBUFFER_HOLDER_SIZE, "Bad size");

    int res = 0;

    res = map_region(&kspace, (uintptr_t)VIRTIO_FRAMEBUFFER_HOLDER, NULL, 0, VIRTIO_FRAMEBUFFER_HOLDER_SIZE, PROT_R | PROT_W | ALLOC_ZERO | ALLOC_NOW);
    assert(res >= 0);

    virtio_gpu_reqpage = (volatile struct virtio_gpu_bigfngreq *)VIRTIO_FRAMEBUFFER_HOLDER;
    virtio_gpu_reqpage_phys = lookup_physaddr((const void *)virtio_gpu_reqpage, VIRTIO_GPU_CONTROLPAGE_SIZE);

    // Init (most of) the reqpage data:
    static const unsigned RES_ID = 1;
    static const unsigned SCREEN_WIDTH = 640;
    static const unsigned SCREEN_HEIGHT = 400;
    static const unsigned FB_SIZE = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(virtio_le32_t);

    static_assert(sizeof(*virtio_gpu_reqpage) + FB_SIZE <= VIRTIO_GPU_CONTROLPAGE_SIZE, "Bad size");
    static_assert(sizeof(*virtio_gpu_reqpage) < PAGE_SIZE, "Bad size");

    virtio_gpu_reqpage->display_info.req.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_GET_DISPLAY_INFO};

    virtio_gpu_reqpage->init.create.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_RESOURCE_CREATE_2D};
    virtio_gpu_reqpage->init.create.resource_id = RES_ID;
    virtio_gpu_reqpage->init.create.format = VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM;
    virtio_gpu_reqpage->init.create.width = SCREEN_WIDTH;
    virtio_gpu_reqpage->init.create.height = SCREEN_HEIGHT;
    virtio_gpu_reqpage->init.attach.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING};
    virtio_gpu_reqpage->init.attach.resource_id = RES_ID;
    virtio_gpu_reqpage->init.attach.nr_entries = 1;
    virtio_gpu_reqpage->init.scanout.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_SET_SCANOUT};

    virtio_gpu_reqpage->flush.transfer.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D};
    virtio_gpu_reqpage->flush.transfer.r = (struct virtio_gpu_rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    virtio_gpu_reqpage->flush.transfer.offset = 0;
    virtio_gpu_reqpage->flush.transfer.resource_id = RES_ID;
    virtio_gpu_reqpage->flush.flush.hdr = (struct virtio_gpu_ctrl_hdr){VIRTIO_GPU_CMD_RESOURCE_FLUSH};
    virtio_gpu_reqpage->flush.flush.r = (struct virtio_gpu_rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    virtio_gpu_reqpage->flush.flush.resource_id = RES_ID;

    virtio_gpu_reqpage->init.attachment_page = (struct virtio_gpu_mem_entry){
        virtio_gpu_reqpage_phys + PAGE_SIZE, FB_SIZE
    };

    virtio_gpu_fb = (uint32_t *)(((void *)virtio_gpu_reqpage) + PAGE_SIZE);
}


void
virtio_gpu_init() {
    int res = 0;

    if (virtio_gpu_device) {
        return;
    }

    virtio_gpu_device = virtio_create_device();
    res = virtio_init_device(virtio_gpu_device, VIRTIO_GPU_DEVICE_ID);
    assert(res >= 0);

    assert(virtio_gpu_device->num_queues >= 1);

    struct virtq *queue = &virtio_gpu_device->queues[0];

    virtio_gpu_init_header();

    size_t offs = offsetof(struct virtio_gpu_bigfngreq, display_info);

    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->display_info.req),
        sizeof(virtio_gpu_reqpage->display_info.resp)
    );
    offs += sizeof(virtio_gpu_reqpage->display_info) + sizeof(virtio_gpu_reqpage->display_info.req);

    // Now we have the display info in resp, if we need it for whatever reason...
    for (unsigned i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; ++i) {
        #define PMODE_ (virtio_gpu_reqpage->display_info.resp.pmodes[i])

        if (!PMODE_.enabled) {
            continue;
        }

        cprintf("%u %u %u %u\n", 
                PMODE_.r.x,
                PMODE_.r.y,
                PMODE_.r.width,
                PMODE_.r.height);

        #undef PMODE_
    }

    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->init.create),
        sizeof(virtio_gpu_reqpage->init.create_resp)
    );
    offs += sizeof(virtio_gpu_reqpage->init.create) + sizeof(virtio_gpu_reqpage->init.create_resp);

    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->init.attach),
        sizeof(virtio_gpu_reqpage->init.attach_resp)
    );
    offs += sizeof(virtio_gpu_reqpage->init.attach) + sizeof(virtio_gpu_reqpage->init.attachment_page)
          + sizeof(virtio_gpu_reqpage->init.attach_resp);
    
    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->init.scanout),
        sizeof(virtio_gpu_reqpage->init.scanout_resp)
    );
    offs += sizeof(virtio_gpu_reqpage->init.scanout) + sizeof(virtio_gpu_reqpage->init.scanout_resp);

    cprintf("Virtio gpu initialization complete\n");
}

int
virtio_gpu_flush() {
    if (!virtio_gpu_device) {
        return -E_INVAL;
    }

    assert(virtio_gpu_device->num_queues >= 1);

    struct virtq *queue = &virtio_gpu_device->queues[0];

    size_t offs = offsetof(struct virtio_gpu_bigfngreq, flush);

    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->flush.transfer),
        sizeof(virtio_gpu_reqpage->flush.transfer_resp)
    );
    offs += sizeof(virtio_gpu_reqpage->flush.transfer) + sizeof(virtio_gpu_reqpage->flush.transfer_resp);

    virtq_request(
        queue,
        virtio_gpu_reqpage_phys + offs,
        sizeof(virtio_gpu_reqpage->flush.flush),
        sizeof(virtio_gpu_reqpage->flush.flush_resp)
    );
    offs += sizeof(virtio_gpu_reqpage->flush.flush) + sizeof(virtio_gpu_reqpage->flush.flush_resp);

    return 0;
}
