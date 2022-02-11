#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <inc/types.h>

// returns -1 if screen size is not supported
int framebuffer_init(uint32_t **framebuffer_ptr, uint32_t width, uint32_t height);
int framebuffer_deinit(uint32_t *framebuffer);

int framebuffer_flush();

#endif