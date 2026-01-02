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

#include "kernel.h"

void serial_init(uint16_t port) {
    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    outb(port + 0, 0x03);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);
    outb(port + 2, 0xC7);
    outb(port + 4, 0x0B);
    serial_putc('\0');
}

int serial_received_port(uint16_t port) {
    return inb(port + 5) & 1;
}

char serial_getc_port(uint16_t port) {
    while (!serial_received_port(port));
    return inb(port);
}

int serial_is_transmit_empty_port(uint16_t port) {
    return inb(port + 5) & 0x20;
}

void serial_putc_port(uint16_t port, char c) {
    while (!serial_is_transmit_empty_port(port));
    outb(port, c);
}

void serial_puts_port(uint16_t port, const char* s) {
    while (*s) {
        if (*s == '\n') {
            serial_putc_port(port, '\r');
        }
        serial_putc_port(port, *s++);
    }
}

void serial_puthex8_port(uint16_t port, uint8_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_putc_port(port, hex[(value >> 4) & 0x0F]);
    serial_putc_port(port, hex[value & 0x0F]);
}

void serial_puthex16_port(uint16_t port, uint16_t value) {
    serial_puthex8_port(port, (value >> 8) & 0xFF);
    serial_puthex8_port(port, value & 0xFF);
}

void serial_puthex32_port(uint16_t port, uint32_t value) {
    serial_puthex16_port(port, (value >> 16) & 0xFFFF);
    serial_puthex16_port(port, value & 0xFFFF);
}

void serial_puthex64_port(uint16_t port, uint64_t value) {
    serial_puthex32_port(port, (value >> 32) & 0xFFFFFFFF);
    serial_puthex32_port(port, value & 0xFFFFFFFF);
}

void serial_putdec32_port(uint16_t port, uint32_t value) {
    if (value == 0) {
        serial_putc_port(port, '0');
        return;
    }

    char buffer[32];
    char* ptr = buffer + 31;
    *ptr = '\0';

    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }

    serial_puts_port(port, ptr);
}

void serial_putdec64_port(uint16_t port, uint64_t value) {
    if (value == 0) {
        serial_putc_port(port, '0');
        return;
    }

    char buffer[64];
    char* ptr = buffer + 63;
    *ptr = '\0';

    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }

    serial_puts_port(port, ptr);
}
