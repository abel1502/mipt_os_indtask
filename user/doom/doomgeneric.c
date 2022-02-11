#include "doomgeneric.h"
#include <inc/lib.h>
#include <inc/framebuffer.h>

uint32_t *DG_ScreenBuffer = 0;
static int start_time = 0;
static uint32_t start_timems = 0;
static const unsigned improv_timer_iters_per_tick = 10000;
static       unsigned improv_timer_ticks_per_100ms = 0;


void dg_Create() {
    assert(framebuffer_init(&DG_ScreenBuffer, 640, 400) >= 0);
    // DG_ScreenBuffer = libc_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

    DG_Init();
}

extern int main(int argc, char **argv);

void umain(int argc, char **argv) {
    main(argc, argv);
}

static void improv_timer_tick() {
    for (unsigned i = 0; i < improv_timer_iters_per_tick; ++i) {
        asm volatile ("pause");  // Since this is technically a spin-wait loop
    }
}

void DG_Init() {
    start_timems = vsys_gettimems();

    unsigned ticks = 0;

    int time = vsys_gettime();
    assert(time > 0);

    start_time = time;

    while (true) {
        int new_time = vsys_gettime();
        assert(new_time > 0);

        if (new_time > time) {
            time = new_time;
            break;
        }
    }

    while (true) {
        improv_timer_tick();
        ticks++;

        int new_time = vsys_gettime();
        assert(new_time > 0);

        if (new_time > time) {
            break;
        }
    }

    improv_timer_ticks_per_100ms = ticks / 10;
    assert(improv_timer_ticks_per_100ms > 1);  // Otherwise the speed is way too high
}

void DG_DrawFrame() {
    // libc_printf("[Doom] flush\n");

    assert(framebuffer_flush() >= 0);
}


void DG_SleepMs(uint32_t ms) {
    #if 0
    if (ms < improv_timer_ticks_per_100ms) {
        sys_yield();
        return;
    }

    int start = vsys_gettime();
    assert(start >= 0);

    while (true) {
        int cur = vsys_gettime();
        assert(start >= 0);

        uint32_t delta = (cur - start) * 1000;
        if (delta >= ms) { ms -= delta; break; }
        ms -= delta;

        sys_yield();
    }

    while (ms >= 100) {
        for (unsigned i = 0; i < improv_timer_ticks_per_100ms; ++i) {
            improv_timer_tick();
        }
    }
    #else
    int seconds_last = vsys_gettime();

    while (ms > 1000) {
        int delta = vsys_gettime() - seconds_last;
        seconds_last += delta;

        if (delta * 1000 > ms) {
            return;
        }

        ms -= delta * 1000;

        sys_yield();
    }

    uint32_t timer_stop = vsys_gettimems() + ms;

    if (timer_stop == (uint32_t)-1) {
        timer_stop = 0;
    }

    while (vsys_gettimems() < timer_stop) {
        asm volatile ("pause");
    }
    #endif
}


uint32_t DG_GetTicksMs() {
    #if 0
    static unsigned cnt = 0;
    static unsigned last_sec = 0;

    unsigned cur_sec = (uint32_t)((vsys_gettime() - start_time));

    if (cur_sec > last_sec) {
        cnt = 0;
    } else {
        cnt++;
    }

    last_sec = cur_sec;

    return cur_sec * 1000 + cnt;
    #else
    return vsys_gettimems() - start_timems;
    #endif
}

int DG_GetKey(int* pressed, unsigned char* key) {
    uint8_t is_released = false;
    int c = sys_rcgetc(&is_released);
    if(c != 0) {
        libc_printf("Key 0x%x %s\n", c, is_released ? "released" : "pressed");
        *pressed = is_released == 0 ? 1 : 0;
        *key = c;
        return 1;
    }
    return 0;
}


void DG_SetWindowTitle(const char * title) {

}
