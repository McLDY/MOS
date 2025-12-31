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

#include "bmp.h"
#include "drivers/fs/fat16.h"
#include "graphics.h"
#include "serial.h"
#include "memory.h"

// 读取小端16位值
static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

// 读取小端32位值
static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | 
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

// 读取小端有符号32位值
static int32_t read_le32_signed(const uint8_t *data) {
    return (int32_t)read_le32(data);
}

// 从FAT16文件加载BMP - 使用堆内存
int bmp_load_from_file(const char *filename, BMPImage *image) {
    Fat16File file;
    int32_t result;
    uint8_t header[54];
    
    serial_puts("\n=== Loading BMP: ");
    serial_puts(filename);
    serial_puts(" ===\n");
    
    // 打开文件
    if (fat16_open(filename, &file, 0) != 0) {
        serial_puts("Failed to open BMP file\n");
        return -1;
    }
    
    // 读取头部
    result = fat16_read(&file, header, 54);
    if (result != 54) {
        serial_puts("Failed to read BMP header\n");
        fat16_close(&file);
        return -1;
    }
    
    // 手动解析文件头
    image->file_header.signature = read_le16(&header[0]);
    image->file_header.file_size = read_le32(&header[2]);
    image->file_header.reserved = read_le32(&header[6]);
    image->file_header.data_offset = read_le32(&header[10]);
    
    // 手动解析信息头
    image->info_header.header_size = read_le32(&header[14]);
    image->info_header.width = read_le32_signed(&header[18]);
    image->info_header.height = read_le32_signed(&header[22]);
    image->info_header.planes = read_le16(&header[26]);
    image->info_header.bpp = read_le16(&header[28]);
    image->info_header.compression = read_le32(&header[30]);
    image->info_header.image_size = read_le32(&header[34]);
    image->info_header.x_pixels_per_m = read_le32_signed(&header[38]);
    image->info_header.y_pixels_per_m = read_le32_signed(&header[42]);
    image->info_header.colors_used = read_le32(&header[46]);
    image->info_header.important_colors = read_le32(&header[50]);
    
    // 验证文件头
    if (image->file_header.signature != 0x4D42) {
        serial_puts("Invalid BMP signature\n");
        fat16_close(&file);
        return -1;
    }
    
    // 验证压缩类型
    if (image->info_header.compression != 0) {
        serial_puts("Compressed BMP not supported. Use: ffmpeg -i mwos.bmp -compression none mwos_uncompressed.bmp\n");
        fat16_close(&file);
        return -1;
    }
    
    // 验证位深度
    if (image->info_header.bpp != 24 && image->info_header.bpp != 32) {
        serial_puts("Only 24-bit and 32-bit BMP supported\n");
        fat16_close(&file);
        return -1;
    }
    
    // 计算图像参数
    int32_t width = image->info_header.width;
    int32_t height = image->info_header.height;
    uint16_t bpp = image->info_header.bpp;
    
    // 防止除零错误
    if (bpp == 0) {
        serial_puts("Error: BMP has 0 bits per pixel\n");
        fat16_close(&file);
        return -1;
    }
    
    // 高度处理
    int is_top_down = (height < 0);
    uint32_t abs_height = is_top_down ? (uint32_t)(-height) : (uint32_t)height;
    
    // 计算行大小（32位对齐）
    uint32_t row_size = ((width * bpp + 31) / 32) * 4;
    
    // 计算像素数据大小
    if (image->info_header.image_size != 0) {
        image->data_size = image->info_header.image_size;
    } else {
        image->data_size = row_size * abs_height;
    }
    
    serial_puts("Image: ");
    serial_putdec32(width);
    serial_puts("x");
    serial_putdec32(abs_height);
    serial_puts(", ");
    serial_putdec32(bpp);
    serial_puts(" bpp, size=");
    serial_putdec32(image->data_size);
    serial_puts(" bytes\n");
    
    // 检查图像是否太大
    if (image->data_size > HEAP_SIZE - 1024 * 1024) { // 留出1MB余量
        serial_puts("BMP too large for available heap\n");
        serial_puts("Max allocatable: ");
        serial_putdec32(HEAP_SIZE - 1024 * 1024);
        serial_puts(" bytes\n");
        fat16_close(&file);
        return -1;
    }
    
    // 使用堆内存分配像素数据
    image->pixel_data = (uint8_t*)kmalloc(image->data_size);
    if (!image->pixel_data) {
        serial_puts("ERROR: Failed to allocate memory for BMP data\n");
        mem_info();
        fat16_close(&file);
        return -1;
    }
    
    serial_puts("Allocated BMP buffer at 0x");
    serial_puthex64((uint64_t)image->pixel_data);
    serial_puts("\n");
    
    // 跳转到像素数据位置
    if (fat16_seek(&file, image->file_header.data_offset) != 0) {
        serial_puts("Failed to seek to pixel data\n");
        kfree(image->pixel_data);
        image->pixel_data = NULL;
        fat16_close(&file);
        return -1;
    }
    
    // 读取像素数据
    result = fat16_read(&file, image->pixel_data, image->data_size);
    if (result != (int32_t)image->data_size) {
        serial_puts("Failed to read pixel data: got ");
        serial_putdec32(result);
        serial_puts(", expected ");
        serial_putdec32(image->data_size);
        serial_puts("\n");
        kfree(image->pixel_data);
        image->pixel_data = NULL;
        fat16_close(&file);
        return -1;
    }
    
    serial_puts("BMP loaded successfully\n");
    
    fat16_close(&file);
    return 0;
}

// 显示BMP图像（安全版本，带边界检查）
int bmp_display(BMPImage *image, int32_t x, int32_t y) {
    if (!image || !image->pixel_data) {
        serial_puts("Invalid BMP image\n");
        return -1;
    }
    
    // 获取图像参数
    int32_t width = image->info_header.width;
    int32_t height = image->info_header.height;
    uint16_t bpp = image->info_header.bpp;
    
    // 验证位深度
    if (bpp != 24 && bpp != 32) {
        serial_puts("Only 24-bit and 32-bit BMP supported for display\n");
        return -1;
    }
    
    // 防止除零错误
    if (bpp == 0) {
        serial_puts("Error: Bits per pixel is 0\n");
        return -1;
    }
    
    // 计算行大小
    uint32_t row_size = ((width * bpp + 31) / 32) * 4;
    
    // 高度处理
    int is_top_down = (height < 0);
    uint32_t abs_height = is_top_down ? (uint32_t)(-height) : (uint32_t)height;
    
    // 获取屏幕边界
    if (!g_framebuffer) {
        serial_puts("Error: Framebuffer not initialized\n");
        return -1;
    }
    
    uint32_t screen_width = g_framebuffer->framebuffer_width;
    uint32_t screen_height = g_framebuffer->framebuffer_height;
    
    serial_puts("Displaying BMP ");
    serial_putdec32(width);
    serial_puts("x");
    serial_putdec32(abs_height);
    serial_puts(" at (");
    serial_putdec32(x);
    serial_puts(",");
    serial_putdec32(y);
    serial_puts(") on ");
    serial_putdec32(screen_width);
    serial_puts("x");
    serial_putdec32(screen_height);
    serial_puts(" screen\n");
    
    // 计算可见区域
    int32_t start_x = (x < 0) ? 0 : x;
    int32_t start_y = (y < 0) ? 0 : y;
    int32_t end_x = x + width;
    int32_t end_y = y + abs_height;
    
    // 限制在屏幕范围内
    if (end_x > (int32_t)screen_width) end_x = screen_width;
    if (end_y > (int32_t)screen_height) end_y = screen_height;
    
    // 如果没有可见区域，直接返回
    if (start_x >= end_x || start_y >= end_y) {
        serial_puts("Image completely outside screen bounds\n");
        return 0; // 不是错误，只是不可见
    }
    
    // 计算图像中的起始位置
    int32_t img_start_x = start_x - x;
    int32_t img_start_y = start_y - y;
    int32_t display_width = end_x - start_x;
    int32_t display_height = end_y - start_y;
    
    serial_puts("Display region: ");
    serial_putdec32(display_width);
    serial_puts("x");
    serial_putdec32(display_height);
    serial_puts(" pixels\n");
    
    // 显示可见部分
    for (int32_t row = 0; row < display_height; row++) {
        // 检查行是否超出图像范围
        if (img_start_y + row >= (int32_t)abs_height) {
            break;
        }
        
        // 计算源数据行
        uint32_t src_row = is_top_down ? 
            (uint32_t)(img_start_y + row) : 
            (uint32_t)(abs_height - 1 - (img_start_y + row));
        
        // 获取行指针
        uint8_t *row_data = image->pixel_data + (src_row * row_size);
        
        // 移动到起始列
        row_data += img_start_x * (bpp / 8);
        
        // 显示该行像素
        for (int32_t col = 0; col < display_width; col++) {
            // 检查列是否超出图像范围
            if (img_start_x + col >= width) {
                break;
            }
            
            uint32_t color = 0;
            
            if (bpp == 24) {
                // 24位BMP: BGR格式
                uint32_t pixel_offset = col * 3;
                uint8_t b = row_data[pixel_offset];
                uint8_t g = row_data[pixel_offset + 1];
                uint8_t r = row_data[pixel_offset + 2];
                color = (r << 16) | (g << 8) | b;
            } else if (bpp == 32) {
                // 32位BMP: BGRA格式
                uint32_t pixel_offset = col * 4;
                uint8_t b = row_data[pixel_offset];
                uint8_t g = row_data[pixel_offset + 1];
                uint8_t r = row_data[pixel_offset + 2];
                uint8_t a = row_data[pixel_offset + 3];
                // 简单alpha混合：如果alpha<128，跳过
                if (a > 128) {
                    color = (r << 16) | (g << 8) | b;
                } else {
                    continue; // 跳过透明像素
                }
            }
            
            // 计算屏幕坐标
            int32_t screen_x = start_x + col;
            int32_t screen_y = start_y + row;
            
            // 绘制像素
            put_pixel((uint32_t)screen_x, (uint32_t)screen_y, color);
        }
    }
    
    serial_puts("BMP display completed\n");
    return 0;
}

// 直接从路径加载并显示BMP（简化接口）
int bmp_display_from_path(const char *path, int32_t x, int32_t y) {
    BMPImage image;
    
    // 清零结构体
    for (uint32_t i = 0; i < sizeof(BMPImage); i++) {
        ((uint8_t*)&image)[i] = 0;
    }
    
    // 加载BMP
    if (bmp_load_from_file(path, &image) != 0) {
        serial_puts("Failed to load BMP from path\n");
        return -1;
    }
    
    // 显示BMP
    int result = bmp_display(&image, x, y);
    
    // 释放内存
    bmp_free(&image);
    
    return result;
}

// 释放BMP图像内存
void bmp_free(BMPImage *image) {
    if (image && image->pixel_data) {
        kfree(image->pixel_data);
        image->pixel_data = NULL;
    }
}

// 简单的BMP测试函数
int bmp_test(const char *filename) {
    serial_puts("\n=== BMP Test: ");
    serial_puts(filename);
    serial_puts(" ===\n");
    
    // 只验证头部，不加载像素数据
    Fat16File file;
    uint8_t header[54];
    
    if (fat16_open(filename, &file, 0) != 0) {
        serial_puts("Failed to open file\n");
        return -1;
    }
    
    if (fat16_read(&file, header, 54) != 54) {
        serial_puts("Failed to read header\n");
        fat16_close(&file);
        return -1;
    }
    
    // 验证签名
    if (header[0] != 'B' || header[1] != 'M') {
        serial_puts("Not a valid BMP file\n");
        fat16_close(&file);
        return -1;
    }
    
    // 解析基本信息
    uint32_t width = read_le32(&header[18]);
    int32_t height = read_le32_signed(&header[22]);
    uint16_t bpp = read_le16(&header[28]);
    uint32_t compression = read_le32(&header[30]);
    
    serial_puts("BMP Info: ");
    serial_putdec32(width);
    serial_puts("x");
    serial_putdec32(height < 0 ? -height : height);
    serial_puts(", ");
    serial_putdec32(bpp);
    serial_puts(" bpp, compression=");
    serial_putdec32(compression);
    serial_puts("\n");
    
    if (compression != 0) {
        serial_puts("WARNING: Compressed BMP may not display correctly\n");
    }
    
    fat16_close(&file);
    return 0;
}