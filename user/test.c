/* Test Conversation between parent and child environment
 * Contributed by Varun Agrawal at Stony Brook */

#include <inc/lib.h>

#define LIBC_REMOVE_FPREFIX
#include <inc/libdoom.h>


// #define PLEASE_DRAW_SOME_PENS


#define SCREEN_H 400
#define SCREEN_W 640


uint32_t *fb = NULL;


struct vec2d {
    double x, y;
};

struct vec2i {
    int x, y;
};


void
draw_ball(struct vec2i center, int radius, uint32_t color) {
    for (int y = 0; y < SCREEN_H; ++y) {
        for (int x = 0; x < SCREEN_W; ++x) {
            struct vec2i delta = (struct vec2i){x - center.x, y - center.y};

            if (delta.x * delta.x + delta.y * delta.y <= radius * radius) {
                fb[y * SCREEN_W + x] = color;
            }
        }
    }
}

void
draw_rect(struct vec2i start, struct vec2i end, uint32_t color) {
    if (start.x > end.x) {
        int tmp = start.x;
        start.x = end.x;
        end.x = tmp;
    }

    if (start.y > end.y) {
        int tmp = start.y;
        start.y = end.y;
        end.y = tmp;
    }

    start.x = MAX(start.x, 0);
    start.y = MAX(start.y, 0);
    end.x = MIN(end.x, SCREEN_W);
    end.y = MIN(end.y, SCREEN_H);
    

    for (int y = start.y; y < end.y; ++y) {
        for (int x = start.x; x < end.x; ++x) {
            fb[y * SCREEN_W + x] = color;
        }
    }
}


void
draw_pens(struct vec2i root, unsigned ball_radius, unsigned stem_length, unsigned stem_radius, uint32_t color) {
    draw_ball((struct vec2i){root.x - ball_radius, root.y}, ball_radius, color);
    draw_ball((struct vec2i){root.x + ball_radius, root.y}, ball_radius, color);
    draw_rect((struct vec2i){root.x - stem_radius, root.y + ball_radius / 2}, (struct vec2i){root.x + stem_radius, root.y - stem_length}, color);
    draw_ball((struct vec2i){root.x, root.y - stem_length}, stem_radius, color);
}


void
umain(int argc, char **argv) {
    // const char *buf = "abc 123 xyz\0";
    // int n = 0;
    // char *s1 = malloc(10);
    // char *s2 = malloc(10);

    // sscanf(buf, "%s %d %s", s1, &n, s2);
    // printf("%s|%d|%s\n", s1, n, s2);

    int res = sys_virtiogpu_init(&fb);
    assert(res >= 0);
    printf("fb = %p\n", fb);

    // for (unsigned y = 0; y < SCREEN_H; ++y) {
    //     for (unsigned x = 0; x < SCREEN_W; ++x) {
    //         fb[y * SCREEN_W + x] = 0x000000FF;
    //     }
    // }

    draw_rect((struct vec2i){0, 0}, (struct vec2i){SCREEN_W, SCREEN_H}, 0xFF000000);

    #ifdef PLEASE_DRAW_SOME_PENS
    draw_pens((struct vec2i){SCREEN_W / 2, SCREEN_H - 50}, 25, 200, 10, 0xFF0000FF);
    #endif
    
    res = sys_virtiogpu_flush();
    assert(res >= 0);
    
}
