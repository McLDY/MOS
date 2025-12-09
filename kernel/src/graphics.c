#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

framebuffer_info_t *g_framebuffer = NULL;

// 内部缓存，以便减少重复字段访问
static uint8_t *fb_addr = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0; // bytes per scanline
static uint8_t fb_bpp = 0;
static uint32_t fb_format = 0; // 0=RGB,1=BGR

void graphics_init_raw(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp, uint32_t format) {
    static framebuffer_info_t fb;
    fb.framebuffer_addr = addr;
    fb.framebuffer_width = width;
    fb.framebuffer_height = height;
    fb.framebuffer_pitch = pitch;
    fb.framebuffer_bpp = bpp;
    fb.framebuffer_format = format;

    g_framebuffer = &fb;

    fb_addr = (uint8_t*)(uintptr_t)addr; // assume identity mapping / accessible
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bpp = bpp;
    fb_format = format;
}

void graphics_init(framebuffer_info_t *fb_info) {
    if (!fb_info) return;
    graphics_init_raw(fb_info->framebuffer_addr,
                      fb_info->framebuffer_width,
                      fb_info->framebuffer_height,
                      fb_info->framebuffer_pitch,
                      fb_info->framebuffer_bpp,
                      fb_info->framebuffer_format);
}

static inline void write_pixel_bytes(uint8_t *pixel_ptr, uint8_t r, uint8_t g, uint8_t b) {
    // Write bytes according to framebuffer bpp and format
    if (fb_bpp == 32) {
        if (fb_format == 0) {
            // RGB order
            pixel_ptr[0] = r;
            pixel_ptr[1] = g;
            pixel_ptr[2] = b;
            pixel_ptr[3] = 0x00; // alpha/reserved
        } else {
            // BGR order
            pixel_ptr[0] = b;
            pixel_ptr[1] = g;
            pixel_ptr[2] = r;
            pixel_ptr[3] = 0x00;
        }
    } else if (fb_bpp == 24) {
        if (fb_format == 0) {
            pixel_ptr[0] = r;
            pixel_ptr[1] = g;
            pixel_ptr[2] = b;
        } else {
            pixel_ptr[0] = b;
            pixel_ptr[1] = g;
            pixel_ptr[2] = r;
        }
    } else {
        // unsupported bpp, ignore
    }
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_framebuffer || !fb_addr) return;
    if (x >= fb_width || y >= fb_height) return;

    uint8_t b = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t r = color & 0xFF;

    uint8_t *row = fb_addr + (uint32_t)y * fb_pitch;
    uint8_t *pixel_ptr = row + (uint32_t)x * (fb_bpp / 8);

    write_pixel_bytes(pixel_ptr, r, g, b);
}

void clear_screen(uint32_t color) {
    if (!g_framebuffer || !fb_addr) return;

    uint8_t b = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t r = color & 0xFF;

    for (uint32_t y = 0; y < fb_height; y++) {
        uint8_t *row = fb_addr + y * fb_pitch;
        for (uint32_t x = 0; x < fb_width; x++) {
            uint8_t *pixel_ptr = row + x * (fb_bpp / 8);
            write_pixel_bytes(pixel_ptr, r, g, b);
        }
    }
}

void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!g_framebuffer) return;

    uint32_t max_x = x + width;
    uint32_t max_y = y + height;

    if (max_x > fb_width) max_x = fb_width;
    if (max_y > fb_height) max_y = fb_height;

    for (uint32_t yy = y; yy < max_y; yy++) {
        for (uint32_t xx = x; xx < max_x; xx++) {
            put_pixel(xx, yy, color);
        }
    }
}

void draw_test_square(void) {
    if (!g_framebuffer) return;

    uint32_t square_size = 100;
    if (square_size > fb_width || square_size > fb_height) {
        square_size = 50;
    }

    uint32_t x = (fb_width - square_size) / 2;
    uint32_t y = (fb_height - square_size) / 2;

    draw_rect(x, y, square_size, square_size, COLOR_BLUE);
}