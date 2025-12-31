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

#include "drivers/pic.h"
#include "io.h"
#include "drivers/ide.h"
/**
 * 重映射 PIC
 * @param offset1 主片起始向量号 (建议 32)
 * @param offset2 从片起始向量号 (建议 40)
 */
void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t a1, a2;

    // 1. 保存当前的屏蔽位 (Masks)
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    // 2. 开始初始化序列 (使用 ICW1)
    outb(PIC1, ICW1_INIT | ICW1_ICW4); 
    io_wait(); // 等待 I/O 操作完成
    outb(PIC2, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // 3. 设置偏移量 (ICW2)
    outb(PIC1_DATA, offset1); 
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    // 4. 告知主片和从片如何级联 (ICW3)
    outb(PIC1_DATA, 4);        // 主片告知从片在 IRQ2 (00000100b)
    io_wait();
    outb(PIC2_DATA, 2);        // 从片告知其标识号为 2
    io_wait();

    // 5. 设置 8086 模式 (ICW4)
    outb(PIC1_DATA, ICW1_ICW4);
    io_wait();
    outb(PIC2_DATA, ICW1_ICW4);
    io_wait();

    // 6. 恢复屏蔽位（或者全屏蔽，等驱动初始化时再开启）
    // 为了防止未处理的中断，我们暂时屏蔽所有
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}