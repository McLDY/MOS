#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stddef.h>

// 简单的帧缓冲信息（本项目内部使用）
typedef struct {
    uint64_t framebuffer_addr;    // 帧缓冲物理/虚拟地址（需要在当前地址空间可访问）
    uint32_t framebuffer_width;   // 屏幕宽度（像素）
    uint32_t framebuffer_height;  // 屏幕高度（像素）
    uint32_t framebuffer_pitch;   // 每行字节数
    uint8_t  framebuffer_bpp;     // 每像素位数（通常为32或24）
    uint32_t framebuffer_format;  // 0 = RGB, 1 = BGR (use limine enum values)
} framebuffer_info_t;

// 颜色定义，采用 0xRRGGBB 格式（24-bit），高字节保留为 0
#define COLOR_BLACK     0x000000
#define COLOR_WHITE     0xFFFFFF
#define COLOR_RED       0xFF0000
#define COLOR_GREEN     0x00FF00
#define COLOR_BLUE      0x0000FF
#define COLOR_CYAN      0x00FFFF
#define COLOR_MAGENTA   0xFF00FF
#define COLOR_YELLOW    0xFFFF00

extern framebuffer_info_t *g_framebuffer;

// 初始化（原始字段） —— 通常从 Limine framebuffer 填充后调用
void graphics_init_raw(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp, uint32_t format);

// 直接使用旧风格结构初始化（保持兼容）
void graphics_init(framebuffer_info_t *fb_info);

// 像素 / 基本绘图接口
void put_pixel(uint32_t x, uint32_t y, uint32_t color);
void clear_screen(uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_test_square(void);

#endif // GRAPHICS_H