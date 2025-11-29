[BITS 64]

section .text
    global write_vga_message

; 
; void write_vga_message();
; 将消息写入 VGA 缓冲区第一行
;
write_vga_message:
    ; 使用栈帧对齐（64位 ABI 要求）
    push rbp
    mov rbp, rsp
    sub rsp, 32 ; 分配一些阴影空间 (Windows ABI) 或只是对齐 (SysV ABI)

    mov edi, 0xB8000        ; EDI 寄存器指向 VGA 缓冲区物理地址
    mov esi, message_str    ; ESI 寄存器指向要打印的字符串
    mov ecx, message_len    ; ECX 寄存器用作循环计数器
    mov rbx, 0x07           ; 属性字节：0x07 = 浅灰色背景，白色前景
    
write_loop:
    ; 检查是否到达字符串末尾 (可选，如果确定长度正确)
    ; cmp cl, 0
    ; je end_write

    mov al, [esi]           ; 从字符串中加载一个 ASCII 字符到 AL
    mov ah, bl              ; 将颜色属性加载到 AH
    mov word [edi], ax      ; 将 16 位字符/属性对写入 VGA 缓冲区

    add esi, 1              ; 移动到下一个源字符
    add edi, 2              ; 移动到 VGA 缓冲区的下一个字符槽（每槽2字节：字符+颜色）

    loop write_loop         ; 循环直到 ECX 归零

end_write:
    ; 恢复栈帧并返回
    add rsp, 32
    pop rbp
    ret
