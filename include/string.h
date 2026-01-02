// string.h
#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
const char* strchr(const char* str, int ch);
const char* strrchr(const char* str, int ch);
int strncmp(const char* str1, const char* str2, size_t n);
char* strncpy(char* dest, const char* src, size_t n);
char* strncat(char* dest, const char* src, size_t n);

int memcmp(const void* ptr1, const void* ptr2, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
void* memset(void* dest, int ch, size_t count);

int sprintf(char* str, const char* format, ...);

char* itoa(int value, char* str, int base);
char* utoa(unsigned int value, char* str, int base);
char* lltoa(long long value, char* str, int base);
char* ulltoa(unsigned long long value, char* str, int base);

int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);

#endif
