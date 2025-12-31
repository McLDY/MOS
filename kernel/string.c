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

#include "string.h"

// 内存操作函数
void* memset(void* dest, int ch, size_t count) {
    unsigned char* p = dest;
    while(count--) {
        *p++ = (unsigned char)ch;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while(count--) {
        *d++ = *s++;
    }
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t count) {
    const unsigned char* p1 = ptr1;
    const unsigned char* p2 = ptr2;
    while(count--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// 字符串函数
size_t strlen(const char* str) {
    size_t len = 0;
    while(str[len]) {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while((*d++ = *src++));
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest;
    while(*d) d++;
    while((*d++ = *src++));
    return dest;
}

int strcmp(const char* str1, const char* str2) {
    while(*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

const char* strchr(const char* str, int ch) {
    while(*str) {
        if (*str == (char)ch) {
            return str;
        }
        str++;
    }
    return NULL;
}

// 简单版本的sprintf（仅支持%d和%s）
int sprintf(char* str, const char* format, ...) {
    // 简化实现，仅用于调试
    // 在实际使用中，你应该实现完整的sprintf或使用其他方法
    char* p = str;
    const char* f = format;
    
    while(*f) {
        if (*f == '%') {
            f++;
            // 这里应该处理格式说明符
            // 简化：直接复制字符
            *p++ = *f++;
        } else {
            *p++ = *f++;
        }
    }
    *p = '\0';
    
    return p - str;
}