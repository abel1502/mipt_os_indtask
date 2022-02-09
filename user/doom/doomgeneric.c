#include "doomgeneric.h"
#include <inc/lib.h>

uint32_t *DG_ScreenBuffer = 0;
static const unsigned improv_timer_iters_per_tick = 100000;
static       unsigned improv_timer_ticks_per_100ms = 0;
static int start_time = 0;



void dg_Create() {
    int res = sys_virtiogpu_init(&DG_ScreenBuffer);
    assert(res >= 0);
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

    libc_printf("%d\n", ticks);
    improv_timer_ticks_per_100ms = ticks / 10;
    assert(improv_timer_ticks_per_100ms > 1);  // Otherwise the speed is way too high
}


void DG_DrawFrame() {
    printf("Flushing after doom...\n");
    int res = sys_virtiogpu_flush();
    assert(res >= 0);
}


void DG_SleepMs(uint32_t ms) {
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
}


uint32_t DG_GetTicksMs() {
    return (uint32_t)((start_time - vsys_gettime()) * 1000);
}


int DG_GetKey(int* pressed, unsigned char* key) {
    return 0;
}


void DG_SetWindowTitle(const char * title) {

}
