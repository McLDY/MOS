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

#include "bootloader.h"

EFI_SYSTEM_TABLE *gST = NULL;

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;

    set_image_handle(ImageHandle);

    gST->ConOut->Reset(gST->ConOut, 0);

    gST->ConOut->OutputString(gST->ConOut, L"MWOS UEFI Bootloader\r\n");
    gST->ConOut->OutputString(gST->ConOut, L"=====================\r\n\r\n");

    EFI_STATUS status = load_kernel();
    if (EFI_ERROR(status)) {
        print_error(L"[X] Load kernel", status);
    } else {
        print_string(L"[OK] Load kernel\r\n");
    }

    status = boot_kernel();

    return EFI_SUCCESS;
}
