#include <stdint.h>
#include "graphics.h"
#include "io.h"

// 主函数（接收引导程序参数）
void kmain(void *params) {
    // 将参数转换为帧缓冲信息结构体
    framebuffer_info_t *fb_info = (framebuffer_info_t*)params;
    
    // 初始化图形系统
    graphics_init(fb_info);
    
    // 测试图形功能
    // if (g_framebuffer && g_framebuffer->framebuffer_addr) {
        // 清空屏幕为黑色
        clear_screen(COLOR_BLACK);
        
        // 测试显示
        draw_test_square();
        
        // 在屏幕顶部绘制彩色条纹作为测试
        for (uint32_t x = 0; x < g_framebuffer->framebuffer_width; x++) {
            uint32_t color;
            if (x < g_framebuffer->framebuffer_width / 6) {
                color = COLOR_RED;
            } else if (x < g_framebuffer->framebuffer_width / 3) {
                color = COLOR_GREEN;
            } else if (x < g_framebuffer->framebuffer_width / 2) {
                color = COLOR_BLUE;
            } else if (x < g_framebuffer->framebuffer_width * 2 / 3) {
                color = COLOR_CYAN;
            } else if (x < g_framebuffer->framebuffer_width * 5 / 6) {
                color = COLOR_MAGENTA;
            } else {
                color = COLOR_YELLOW;
            }
            
            // 在顶部绘制彩色条纹
            for (uint32_t y = 0; y < 20; y++) {
                put_pixel(x, y, color);
            }
        }
    // }
    
    // 保持显示
    while(1) {
        // 可以添加其他逻辑
    }
}