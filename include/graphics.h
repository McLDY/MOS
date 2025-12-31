#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "kernel.h"

#pragma pack(push, 1)
typedef struct {
    uint64_t framebuffer_addr;    // 帧缓冲物理地址
    uint32_t framebuffer_width;   // 屏幕宽度（像素）
    uint32_t framebuffer_height;  // 屏幕高度（像素）
    uint32_t framebuffer_pitch;   // 每行字节数
    uint32_t framebuffer_bpp;     // 每像素位数（通常为32）- 改为uint32_t
    uint8_t reserved[16];         // 保留字段，确保未来兼容性
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

// 全局帧缓冲信息指针
extern framebuffer_info_t *g_framebuffer;

// 函数声明
void graphics_init(framebuffer_info_t *fb_info);
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_test_square(void);
void print_fb_info();

#endif // GRAPHICS_H