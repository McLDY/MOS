#include "efi.h"
#include "efi_utils.h"

// 全局系统表
EFI_SYSTEM_TABLE *gST = NULL;

// UEFI 应用程序入口点
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    
    // 设置全局句柄
    set_image_handle(ImageHandle);
    
    // 初始化控制台
    gST->ConOut->Reset(gST->ConOut, 0);
    
    // 显示启动信息
    gST->ConOut->OutputString(gST->ConOut, L"MOS UEFI Bootloader\r\n");
    gST->ConOut->OutputString(gST->ConOut, L"=====================\r\n\r\n");
    
    // 加载内核
    EFI_STATUS status = load_kernel();
    if (EFI_ERROR(status)) {
        print_error(L"[X] Load kernel", status);
    } else {
        print_string(L"[OK] Load kernel\n");
    }
    
    // 退出引导服务并启动内核
    status = boot_kernel();
    if (EFI_ERROR(status)) {
        print_error(L"[X] Boot kernel", status);
    } else {
        print_string(L"[OK] Boot kernel");
    }
    
    // 不应该到达这里
    while (1);


    return EFI_SUCCESS;
}