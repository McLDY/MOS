ENTRY(_start)

SECTIONS
{
    . = 0x100000;  /*内核加载到1MB地址*/
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
        *(.rodata)
    }
    
    .bss : {
        *(.bss)
    }
}