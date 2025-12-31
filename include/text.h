#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

// 字体大小
#define FONT_H 16
#define FONT_W 8

// 字体
static const uint8_t font8x8_basic[96][8];

// 文本接口
void put_char(char c, uint32_t x, uint32_t y, uint32_t color);
void print_string(const char *str, uint32_t x, uint32_t y, uint32_t color);

// 中文字符字库 - 16x16像素
// 每个字符需要32字节（16行 x 16位 = 256位 = 32字节）
#define CHINESE_FONT_W 16
#define CHINESE_FONT_H 16


// 中文字符数据结构
typedef struct {
    uint16_t unicode;  // Unicode编码
    uint8_t bitmap[32]; // 16x16位图数据
} chinese_glyph_t;

// 中文字符显示函数
void put_chinese_char(uint16_t unicode, uint32_t x, uint32_t y, uint32_t color);
void print_chinese_string(const uint16_t *unicode_str, uint32_t x, uint32_t y, uint32_t color);

// UTF-8字符串显示函数（简化版）
void print_utf8_string(const char *utf8_str, uint32_t x, uint32_t y, uint32_t color);

// 混合显示：ASCII + 中文
void print_mixed_string(const char *str, uint32_t x, uint32_t y, uint32_t color);

// 查找字库中的字符
const chinese_glyph_t* find_chinese_glyph(uint16_t unicode);


#endif // TEXT_H
