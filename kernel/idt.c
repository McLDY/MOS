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

#include "idt.h"
#include "io.h"
#include "serial.h"

#define PIC1_COMMAND 0x20
#define PIC2_COMMAND 0xA0
#define PIC_EOI      0x20

// 声明外部汇编桩表（由 interrupt.asm 提供）
extern void* isr_stub_table[];

// 16字节对齐是 64 位 IDT 的硬件要求
__attribute__((aligned(0x10))) static struct idt_entry idt[256];
static struct idt_ptr idtr;

// 私有处理函数表
static interrupt_handler_t interrupt_handlers[256];

// 注册函数
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler) {
    interrupt_handlers[n] = handler;
}


// 设置 IDT 表项
void idt_set_gate(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;

    idt[vector].isr_low    = addr & 0xFFFF;             // 偏移量低 16 位
    idt[vector].kernel_cs  = 0x08;                      // GDT 中的内核代码段选择子
    idt[vector].ist        = 0;                         // 中断栈表索引 (0 表示不使用)
    idt[vector].attributes = flags;                     // 权限和类型
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;     // 偏移量中 16 位
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF; // 偏移量高 32 位
    idt[vector].reserved   = 0;                         // 必须置 0
}

// 统一分发器 (由 interrupt.asm 调用)
void idt_handler(interrupt_frame_t *frame) {
    // 尝试获取对应的驱动处理函数
    interrupt_handler_t handler = interrupt_handlers[frame->int_no];

    if (handler != 0) {
        handler(frame);
    } else {
        // 如果是异常且无处理程序，打印后挂起
        if (frame->int_no < 32) {
            serial_puts("\n!!! UNHANDLED EXCEPTION: ");
            serial_putdec64(frame->int_no);
            serial_puts("\n");
        }
    }

    // 发送结束信号 (针对 IRQ 0-15)
    if (frame->int_no >= 32 && frame->int_no <= 47) {
        send_eoi(frame->int_no);
    }
}

// 发送EOI信号
void send_eoi(int int_no) {
    if (int_no >= 40) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

// 初始化 IDT 并加载到 CPU
void idt_init() {
    // 1. 将 256 个向量全部指向对应的汇编桩
    for (int i = 0; i < 256; i++) {
        // 0x8E: P=1, DPL=00, Type=1110 (64-bit Interrupt Gate)
        idt_set_gate(i, isr_stub_table[i], 0x8E);
    }

    // 2. 设置 IDTR 寄存器的值
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    // 3. 加载 IDTR
    asm volatile ("lidt %0" : : "m"(idtr));
}