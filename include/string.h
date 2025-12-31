// string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
const char* strchr(const char* str, int ch);
int sprintf(char* str, const char* format, ...);
// 在string.h中添加
int memcmp(const void* ptr1, const void* ptr2, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memset(void* dest, int ch, size_t count);

#endif // STRING_H