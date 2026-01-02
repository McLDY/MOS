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

__attribute__((aligned(0x10))) static struct idt_entry idt[256];
static struct idt_ptr idtr;

static interrupt_handler_t interrupt_handlers[256];

void register_interrupt_handler(uint8_t n, interrupt_handler_t handler) {
    interrupt_handlers[n] = handler;
}


void idt_set_gate(uint8_t vector, void* isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;

    idt[vector].isr_low    = addr & 0xFFFF;
    idt[vector].kernel_cs  = 0x08;
    idt[vector].ist        = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid    = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high   = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved   = 0;
}

// 统一分发器 (由 interrupt.asm 调用)
void idt_handler(interrupt_frame_t *frame) {
    interrupt_handler_t handler = interrupt_handlers[frame->int_no];

    if (handler != 0) {
        handler(frame);
    } else {
        if (frame->int_no < 32) {
            serial_puts("\n!!! UNHANDLED EXCEPTION: ");
            serial_putdec64(frame->int_no);
            serial_puts("\n");
        }
    }

    if (frame->int_no >= 32 && frame->int_no <= 47) {
        send_eoi(frame->int_no);
    }
}

void send_eoi(int int_no) {
    if (int_no >= 40) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void idt_init() {
    for (int i = 0; i < 256; i++) {
        // 0x8E: P=1, DPL=00, Type=1110 (64-bit Interrupt Gate)
        idt_set_gate(i, isr_stub_table[i], 0x8E);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtr));
}
