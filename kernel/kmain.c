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
#include "stdint.h"
#include "graphics.h"

// 终端窗口配置
typedef struct {
    uint32_t x, y;               // 窗口左上角坐标
    uint32_t w, h;               // 窗口尺寸
    uint32_t cursor_x, cursor_y; // 终端内部光标位置
    uint32_t bg_color;
    uint32_t text_color;
} terminal_t;

// 终端配置信息
static terminal_t g_term = {
    .x = 80,
    .y = 80,
    .w = 600,
    .h = 400,
    .cursor_x = 8,
    .cursor_y = 8,
    .bg_color = 0x000000,
    .text_color = 0x00FF00 // 经典黑底绿字
};

// 上一次鼠标的位置，初始化为 -1 表示还未绘制过
static int32_t old_mouse_x = -1;
static int32_t old_mouse_y = -1;

// 鼠标
int32_t mouse_x = 0;
int32_t mouse_y = 0;

// 图形相关
uint32_t screen_width;
uint32_t screen_height;


void draw_terminal_window();
void term_putc(char c);
void on_keyboard_pressed(uint8_t scancode, uint8_t final_char);
void term_puts(const char* str);

__attribute__((ms_abi, target("no-sse"), target("general-regs-only"))) void kmain(void *params) {
    // 内核
    serial_init(0x3F8);
    serial_puts("Kernel starting...\n");
    
    // 基础硬件初始化
    gdt_init();
    idt_init();
    pic_remap(32, 40);
    asm volatile("sti");

    // 图形渲染初始化
    framebuffer_info_t *fb_info = (framebuffer_info_t*)params;
    if (!fb_info) {
        serial_puts("ERROR: Framebuffer info is null\n");
        while (1);
    }

    // 驱动初始化
    ide_init();
    keyboard_init();
    mouse_init();
    serial_puts("Drivers loaded successfully\n");
    
    graphics_init(fb_info);
    uint32_t screen_width = fb_info->framebuffer_width;
    uint32_t screen_height = fb_info->framebuffer_height;

    // 绘制桌面背景
    clear_screen(0x169de2); 
    
    // 初始化并绘制窗口
    draw_terminal_window();
    term_puts("MWOS Kernel initialized successfully!\n");
    term_puts("Welcome to the command line interface.\n");
    term_puts("Root@MWOS: /# ");

    while (1) {
        // 保持 CPU 冷却
        asm volatile("hlt");
    }
}

// 终端渲染
void draw_terminal_window() {
    // 绘制标题栏 (深蓝色)
    draw_rect(g_term.x, g_term.y, g_term.w, 28, 0x224488);
    print_string("MWOS System Terminal v1.0", g_term.x + 10, g_term.y + 6, 0xFFFFFF);
    // 绘制终端黑色背景区
    draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
}

void term_putc(char c) {
    // 字符实际渲染坐标 = 窗口起始点 + 内容区偏移 + 光标偏移
    uint32_t real_x = g_term.x + g_term.cursor_x;
    uint32_t real_y = g_term.y + 28 + g_term.cursor_y;

    if (c == '\n') {
        g_term.cursor_x = 8;
        g_term.cursor_y += 18;
    } else if (c == '\r') {
        g_term.cursor_x = 8;
    } else {
        put_char(c, real_x, real_y, g_term.text_color);
        g_term.cursor_x += 8;
        
        // 自动换行
        if (g_term.cursor_x + 16 > g_term.w) {
            g_term.cursor_x = 8;
            g_term.cursor_y += 18;
        }
    }

    // 简单的滚动检查：如果到底了就重置到顶部并清屏
    if (g_term.cursor_y + 18 > g_term.h - 30) {
        draw_rect(g_term.x, g_term.y + 28, g_term.w, g_term.h - 28, g_term.bg_color);
        g_term.cursor_y = 8;
    }
}

void term_puts(const char* str) {
    while (*str) {
        term_putc(*str++);
    }
}

// 键盘回调
void on_keyboard_pressed(uint8_t scancode, uint8_t final_char) {
    if (final_char != 0) {
        // 输出到串口监控
        serial_putc(final_char);
        // 输出到图形终端窗口
        term_putc((char)final_char);
        if (final_char == '\n')
        {
            term_puts("Root@MWOS: /# ");
        }
    }
}


// 鼠标回调
void on_mouse_update(int32_t x_rel, int32_t y_rel, uint8_t left_button, uint8_t middle_button, uint8_t right_button)
{
    // 擦除旧光标
    // 如果不是第一次绘制，先恢复上一次保存的背景
    if (old_mouse_x != -1) {
        restore_mouse_background();
    }

    // 更新并限制鼠标坐标
    mouse_x += x_rel;
    mouse_y -= y_rel; // PS/2 Y轴向上为正，屏幕向下为正，所以用减法

    // 边界检查（使用显存实际尺寸）
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int32_t)g_framebuffer->framebuffer_width) 
        mouse_x = g_framebuffer->framebuffer_width - 1;
    if (mouse_y >= (int32_t)g_framebuffer->framebuffer_height) 
        mouse_y = g_framebuffer->framebuffer_height - 1;

    // 保存新位置的背景
    // 在画新鼠标之前，把这块区域原来的像素存起来
    save_mouse_background(mouse_x, mouse_y);

    // 绘制新鼠标光标
    draw_mouse_cursor(mouse_x, mouse_y);

    // 记录当前位置为旧位置
    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;
}
