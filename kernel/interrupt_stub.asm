;
;                   _ooOoo_
;                  o8888888o
;                  88" . "88
;                  (| -_- |)
;                  O\  =  /O
;               ____/`---'\____
;             .'  \\|     |//  `.
;            /  \\|||  :  |||//  \
;           /  _||||| -:- |||||-  \
;           |   | \\\  -  /// |   |
;           | \_|  ''\---/''  |   |
;           \  .-\__  `-`  ___/-. /
;         ___`. .'  /--.--\  `. . __
;      ."" '<  `.___\_<|>_/___.'  >'"".
;     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
;     \  \ `-.   \_ __\ /__ _/   .-` /  /
;======`-.____`-.___\_____/___.-`____.-'======
;                   `=---='
;^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
;            佛祖保佑       永无BUG


[bits 64]
extern idt_handler
section .text

; --- 1. 定义中断入口宏 ---
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push qword 0      ; 伪错误码
    push qword %1     ; 中断号
    jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; CPU 已经自动压入错误码
    push qword %1     ; 中断号
    jmp isr_common
%endmacro

; --- 2. 生成 256 个入口点 ---
%assign i 0
%rep 256
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21 || i == 29 || i == 30
        ISR_ERRCODE i
    %else
        ISR_NOERRCODE i
    %endif
%assign i i+1
%endrep

; --- 3. 统一处理逻辑 ---
isr_common:
    ; 保存通用寄存器 (与 interrupt_frame_t 结构体对应)
    push rax; push rbx; push rcx; push rdx; push rsi; push rdi; push rbp
    push r8; push r9; push r10; push r11; push r12; push r13; push r14; push r15
    
    mov rdi, rsp      ; 第一个参数 (RDI) = 栈指针
    
    ; 64位调用约定建议栈 16 字节对齐
    mov rbp, rsp
    and rsp, -16
    call idt_handler
    mov rsp, rbp      ; 恢复栈
    
    pop r15; pop r14; pop r13; pop r12; pop r11; pop r10; pop r9; pop r8
    pop rbp; pop rdi; pop rsi; pop rdx; pop rcx; pop rbx; pop rax
    add rsp, 16       ; 清理中断号和错误码
    iretq

; --- 4. 生成地址表 ---
section .data
align 8
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    extern isr%+i     ; 显式告诉 NASM 这些符号在上面定义过
    dq isr%+i         ; 使用 %+ 连接符确保名称为 isr0, isr1...
%assign i i+1
%endrep