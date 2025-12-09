#ifndef IO_H
#define IO_H

#include <stdint.h>

void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);

#endif // IO_H
