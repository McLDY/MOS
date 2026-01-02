#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;  // 内核代码段选择子 (通常是 0x08)
    uint8_t  ist;
    uint8_t  attributes; // 类型与权限位 (通常是 0x8E)
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// 中断上下文帧 (用于 idt_handler 参数)
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef void (*interrupt_handler_t)(interrupt_frame_t*);

void idt_init();
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);
void send_eoi(int int_no);

#endif
