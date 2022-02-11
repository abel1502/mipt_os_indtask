//
// Created by Артем on 12.02.2022.
//

#include <inc/lib.h>
#include <inc/framebuffer.h>

int framebuffer_init(uint32_t **framebuffer_ptr, uint32_t width, uint32_t height) {
    if(width != 640) return -1;
    if(height > 480) return -1;
    int res = sys_virtiogpu_init(framebuffer_ptr);
    return res;
}

int framebuffer_deinit(uint32_t *framebuffer) {
    return 0;
}

int framebuffer_flush() {
    int res = sys_virtiogpu_flush();
    return res;
}