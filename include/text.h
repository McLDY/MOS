#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

#define FONT_H 16
#define FONT_W 8

static const uint8_t font8x8_basic[96][8];

void put_char(char c, uint32_t x, uint32_t y, uint32_t color);
void print_string(const char *str, uint32_t x, uint32_t y, uint32_t color);

#define CHINESE_FONT_W 16
#define CHINESE_FONT_H 16


typedef struct {
    uint16_t unicode;
    uint8_t bitmap[32];
} chinese_glyph_t;

void put_chinese_char(uint16_t unicode, uint32_t x, uint32_t y, uint32_t color);
void print_chinese_string(const uint16_t *unicode_str, uint32_t x, uint32_t y, uint32_t color);

void print_utf8_string(const char *utf8_str, uint32_t x, uint32_t y, uint32_t color);

void print_mixed_string(const char *str, uint32_t x, uint32_t y, uint32_t color);

const chinese_glyph_t* find_chinese_glyph(uint16_t unicode);


#endif
