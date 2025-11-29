#include "efi.h"

// 简单的宽字符串复制
void wcscpy(CHAR16 *dest, const CHAR16 *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = L'\0';
}
