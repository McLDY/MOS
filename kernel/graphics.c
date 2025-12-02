#include "graphics.h"
#include <stdint.h>
#include <stddef.h>

// 全局帧缓冲信息指针
framebuffer_info_t *g_framebuffer = NULL;

// 初始化图形系统
void graphics_init(framebuffer_info_t *fb_info) {
    g_framebuffer = fb_info;
}

// 在指定位置绘制像素
void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) {
        return; // 帧缓冲未初始化
    }
    
    // 检查坐标是否在屏幕范围内
    if (x >= g_framebuffer->framebuffer_width || y >= g_framebuffer->framebuffer_height) {
        return;
    }
    
    // 计算像素在帧缓冲中的位置
    // 假设每像素32位（4字节），BGR格式
    uint32_t *framebuffer = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t pitch_in_pixels = g_framebuffer->framebuffer_pitch / 4; // 每行像素数
    
    // 计算像素索引
    uint32_t pixel_index = y * pitch_in_pixels + x;
    
    // 写入像素颜色
    framebuffer[pixel_index] = color;
}

// 清空屏幕
void clear_screen(uint32_t color) {
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) {
        return; // 帧缓冲未初始化
    }
    
    uint32_t *framebuffer = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t total_pixels = (g_framebuffer->framebuffer_pitch / 4) * g_framebuffer->framebuffer_height;
    
    // 填充整个帧缓冲
    for (uint32_t i = 0; i < total_pixels; i++) {
        framebuffer[i] = color;
    }
}

// 绘制矩形
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!g_framebuffer) {
        return;
    }
    
    // 确保矩形在屏幕范围内
    uint32_t max_x = x + width;
    uint32_t max_y = y + height;
    
    if (max_x > g_framebuffer->framebuffer_width) {
        max_x = g_framebuffer->framebuffer_width;
    }
    if (max_y > g_framebuffer->framebuffer_height) {
        max_y = g_framebuffer->framebuffer_height;
    }
    
    // 绘制矩形
    for (uint32_t rect_y = y; rect_y < max_y; rect_y++) {
        for (uint32_t rect_x = x; rect_x < max_x; rect_x++) {
            put_pixel(rect_x, rect_y, color);
        }
    }
}

// 绘制测试正方形
void draw_test_square(void) {
    if (!g_framebuffer) {
        return;
    }
    
    // 计算正方形位置（屏幕中央）
    uint32_t square_size = 100;
    if (square_size > g_framebuffer->framebuffer_width || 
        square_size > g_framebuffer->framebuffer_height) {
        square_size = 50; // 如果屏幕太小，使用较小的正方形
    }
    
    uint32_t x = (g_framebuffer->framebuffer_width - square_size) / 2;
    uint32_t y = (g_framebuffer->framebuffer_height - square_size) / 2;
    
    // 绘制蓝色正方形
    draw_rect(x, y, square_size, square_size, COLOR_BLUE);
}