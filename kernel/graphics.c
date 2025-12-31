/*
                   _ooOoo_
                  o8888888o
                  88" . "88
                  (| -_- |)
                  O\  =  /O
               ____/`---'\____
             .'  \\|     |//  `.
            /  \\|||  :  |||//  \
           /  _||||| -:- |||||-  \
           |   | \\\  -  /// |   |
           | \_|  ''\---/''  |   |
           \  .-\__  `-`  ___/-. /
         ___`. .'  /--.--\  `. . __
      ."" '<  `.___\_<|>_/___.'  >'"".
     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
     \  \ `-.   \_ __\ /__ _/   .-` /  /
======`-.____`-.___\_____/___.-`____.-'======
                   `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            佛祖保佑       永无BUG
*/

#include "kernel.h"

framebuffer_info_t *g_framebuffer = NULL;

void graphics_init(framebuffer_info_t *fb_info) {
    g_framebuffer = fb_info;
}

void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) {
        return; 
    }
    
    if (x >= g_framebuffer->framebuffer_width || y >= g_framebuffer->framebuffer_height) {
        return;
    }
    
    uint32_t *framebuffer = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t pitch_in_pixels = g_framebuffer->framebuffer_width; // 每行像素数
    
    uint32_t pixel_index = y * pitch_in_pixels + x;
    
    framebuffer[pixel_index] = color;
}

// 清空屏幕
void clear_screen(uint32_t color) {
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) {
        return; 
    }
    
    uint32_t *framebuffer = (uint32_t*)g_framebuffer->framebuffer_addr;
    uint32_t total_pixels = (g_framebuffer->framebuffer_width) * g_framebuffer->framebuffer_height;  //g_framebuffer->framebuffer_height * g_framebuffer->framebuffer_width;
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        framebuffer[i] = color;
    }
}

// 绘制矩形
void draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    if (!g_framebuffer) {
        return;
    }
    
    uint32_t max_x = x + width;
    uint32_t max_y = y + height;
    
    if (max_x > g_framebuffer->framebuffer_width) {
        max_x = g_framebuffer->framebuffer_width;
    }
    if (max_y > g_framebuffer->framebuffer_height) {
        max_y = g_framebuffer->framebuffer_height;
    }
    
    for (uint32_t rect_y = y; rect_y < max_y; rect_y++) {
        for (uint32_t rect_x = x; rect_x < max_x; rect_x++) {
            put_pixel(rect_x, rect_y, color);
        }
    }
}

void draw_test_square(void) {
    if (!g_framebuffer) {
        return;
    }
    
    uint32_t square_size = 100;
    if (square_size > g_framebuffer->framebuffer_width || 
        square_size > g_framebuffer->framebuffer_height) {
        square_size = 50;
    }
    
    uint32_t x = (g_framebuffer->framebuffer_width - square_size) / 2;
    uint32_t y = (g_framebuffer->framebuffer_height - square_size) / 2;
    
    draw_rect(x, y, square_size, square_size, COLOR_BLUE);
}

void print_fb_info(){
    if (!g_framebuffer || !g_framebuffer->framebuffer_addr) {
        return; 
    }
    //serial_init(0x3F8);
    serial_puts("Framebuffer info:\n");
    serial_puts("  addr: ");
    serial_puthex64(g_framebuffer->framebuffer_addr);
    serial_puts("\n");
    
    serial_puts("  width: ");
    serial_puthex64(g_framebuffer->framebuffer_width);
    serial_puts("\n");
    
    serial_puts("  height: ");
    serial_puthex64(g_framebuffer->framebuffer_height);
    serial_puts("\n");
    
    serial_puts("  bpp: ");
    serial_puthex64(g_framebuffer->framebuffer_bpp);
    serial_puts("\n");
}

