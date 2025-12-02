#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

// 帧缓冲结构体（与引导加载程序传递的结构体匹配）
typedef struct {
    uint64_t framebuffer_addr;    // 帧缓冲物理地址
    uint32_t framebuffer_width;   // 屏幕宽度（像素）
    uint32_t framebuffer_height;  // 屏幕高度（像素）
    uint32_t framebuffer_pitch;   // 每行字节数
    uint8_t  framebuffer_bpp;     // 每像素位数（通常为32）
} framebuffer_info_t;

// 32位颜色定义（BGR格式，高8位为Alpha通道）
#define COLOR_BLACK     0x00000000
#define COLOR_WHITE     0x00FFFFFF
#define COLOR_RED       0x000000FF
#define COLOR_GREEN     0x0000FF00
#define COLOR_BLUE      0x00FF0000
#define COLOR_CYAN      0x00FFFF00
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

#endif // GRAPHICS_H