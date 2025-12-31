// memory.h - 更新头文件
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

// 将堆放在32MB处，远离内核代码和数据
#define HEAP_BASE_ADDR 0x2000000   // 32MB
#define HEAP_SIZE (32 * 1024 * 1024) // 32MB堆

// 基本内存管理
void mem_init(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);

// 内存信息
void mem_info(void);

#endif // MEMORY_H