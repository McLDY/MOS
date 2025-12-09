#ifndef FBTEXT_H
#define FBTEXT_H

#include <stdint.h>

// 初始化（graphics_init_raw 已经设置了 framebuffer）
// 文本接口（图形模式下）
void fb_set_colors(uint32_t fg, uint32_t bg);
void fb_putc(char c);
void fb_puts(const char *s);
void fb_clear_text(void);

// 可选：设置文本起始位置（字符坐标）
void fb_set_cursor(uint32_t col, uint32_t row);

#endif // FBTEXT_H