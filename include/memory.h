// memory.h - 更新头文件
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define HEAP_BASE_ADDR 0x2000000
#define HEAP_SIZE (32 * 1024 * 1024)

void mem_init(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

void mem_info(void);

#endif
