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

#include "memory.h"
#include "serial.h"

// 我们将堆放在32MB处（0x2000000），远离内核代码和数据
#define HEAP_BASE_ADDR 0x2000000  // 32MB
#define HEAP_SIZE (32 * 1024 * 1024)  // 32MB堆

static uint32_t heap_used = 0;

// 分配记录（简化版）
typedef struct alloc_info {
    uint32_t size;
    uint32_t magic;  // 用于验证
} alloc_info_t;

#define ALLOC_MAGIC 0xDEADBEEF

void mem_init(void) {
    heap_used = 0;
    serial_puts("Memory manager initialized at 0x");
    serial_puthex64(HEAP_BASE_ADDR);
    serial_puts(" (");
    serial_putdec32(HEAP_SIZE / 1024 / 1024);
    serial_puts(" MB heap)\n");
}

// 获取堆基址
static uint8_t* get_heap_base(void) {
    return (uint8_t*)HEAP_BASE_ADDR;
}

void* kmalloc(uint32_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // 添加分配信息头
    uint32_t total_size = size + sizeof(alloc_info_t);
    
    // 对齐到8字节边界（兼容大多数数据类型）
    total_size = (total_size + 7) & ~7;
    
    // 检查是否有足够空间
    if (heap_used + total_size > HEAP_SIZE) {
        serial_puts("kmalloc failed: out of memory! Requested ");
        serial_putdec32(size);
        serial_puts(" bytes, available ");
        serial_putdec32(HEAP_SIZE - heap_used);
        serial_puts(" bytes\n");
        return NULL;
    }
    
    // 分配内存
    uint8_t* heap_base = get_heap_base();
    alloc_info_t *info = (alloc_info_t*)(heap_base + heap_used);
    info->size = size;
    info->magic = ALLOC_MAGIC;
    
    void *ptr = (void*)(info + 1);  // 跳过信息头
    
    heap_used += total_size;
    
    serial_puts("kmalloc: allocated ");
    serial_putdec32(size);
    serial_puts(" bytes at 0x");
    serial_puthex64((uint64_t)ptr);
    serial_puts("\n");
    
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }
    
    // 获取分配信息
    alloc_info_t *info = (alloc_info_t*)ptr - 1;
    
    // 验证magic值
    if (info->magic != ALLOC_MAGIC) {
        serial_puts("kfree: invalid pointer or memory corruption detected!\n");
        return;
    }
    
    serial_puts("kfree: freed ");
    serial_putdec32(info->size);
    serial_puts(" bytes at 0x");
    serial_puthex64((uint64_t)ptr);
    serial_puts("\n");
    
    // 为了安全，清零释放的内存
    for (uint32_t i = 0; i < info->size; i++) {
        ((uint8_t*)ptr)[i] = 0;
    }
}

// 获取内存使用信息
void mem_info(void) {
    serial_puts("\n=== Memory Information ===\n");
    serial_puts("Heap base: 0x");
    serial_puthex64(HEAP_BASE_ADDR);
    serial_puts("\n");
    
    serial_puts("Heap size: ");
    serial_putdec32(HEAP_SIZE);
    serial_puts(" bytes (");
    serial_putdec32(HEAP_SIZE / 1024 / 1024);
    serial_puts(" MB)\n");
    
    serial_puts("Heap used: ");
    serial_putdec32(heap_used);
    serial_puts(" bytes (");
    serial_putdec32(heap_used / 1024);
    serial_puts(" KB)\n");
    
    serial_puts("Heap free: ");
    serial_putdec32(HEAP_SIZE - heap_used);
    serial_puts(" bytes (");
    serial_putdec32((HEAP_SIZE - heap_used) / 1024);
    serial_puts(" KB)\n");
    
    serial_puts("Usage: ");
    serial_putdec32(heap_used * 100 / HEAP_SIZE);
    serial_puts("%\n");
}