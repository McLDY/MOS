bits 64
global _start

section .text

_start:
    ; 最简单的内核 - 只做HLT循环
    cli
.halt:
    hlt
    jmp .halt