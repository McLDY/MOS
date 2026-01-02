#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "kernel.h"

#pragma pack(push, 1)
typedef struct {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_bpp;
    uint8_t reserved[16];
} framebuffer_info_t;
#pragma pack(pop)

#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_RED       0x00FF0000
#define COLOR_GREEN     0x0000FF00
#define COLOR_BLUE      0x000000FF
#define COLOR_CYAN      0x0000FFFF
#define COLOR_MAGENTA   0x00FF00FF
#define COLOR_YELLOW    0x0000FFFF

extern framebuffer_info_t *g_framebuffer;

void graphics_init(framebuffer_info_t *fb_info);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_test_square(void);
void print_fb_info();

#endif
