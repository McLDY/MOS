// bmp.h
#ifndef BMP_H
#define BMP_H

#include "kernel.h"
#include "drivers/fs/fat16.h"

#pragma pack(push, 1)

// BMP文件头
typedef struct {
    uint16_t signature;      // "BM" (0x4D42)
    uint32_t file_size;      // 文件大小
    uint32_t reserved;       // 保留
    uint32_t data_offset;    // 像素数据偏移
} BMPFileHeader;

// BMP信息头 (DIB header)
typedef struct {
    uint32_t header_size;    // 信息头大小 (40 bytes for BITMAPINFOHEADER)
    int32_t  width;          // 图像宽度（像素）
    int32_t  height;         // 图像高度（像素）
    uint16_t planes;         // 颜色平面数 (必须为1)
    uint16_t bpp;            // 每像素位数 (1,4,8,16,24,32)
    uint32_t compression;    // 压缩方式
    uint32_t image_size;     // 图像数据大小
    int32_t  x_pixels_per_m; // 水平分辨率
    int32_t  y_pixels_per_m; // 垂直分辨率
    uint32_t colors_used;    // 使用的颜色数
    uint32_t important_colors;// 重要颜色数
} BMPInfoHeader;

#pragma pack(pop)

// BMP图像结构
typedef struct {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;
    uint32_t *palette;       // 调色板（如果有）
    uint8_t *pixel_data;     // 像素数据（动态分配）
    uint32_t data_size;      // 像素数据大小
} BMPImage;

// 函数声明
int bmp_load_from_file(const char *filename, BMPImage *image);
int bmp_display(BMPImage *image, int32_t x, int32_t y);
void bmp_free(BMPImage *image);
int bmp_display_from_path(const char *path, int32_t x, int32_t y);
int bmp_test(const char *filename);

#endif // BMP_H