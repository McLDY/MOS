# 工具链配置
CC = clang
CFLAGS = -target x86_64-pc-win32-coff -mno-red-zone -fno-stack-protector -fshort-wchar -Wall -Wextra
AS = nasm
LD = lld-link

# 目录配置
SRCDIR = .
BOOTDIR = $(SRCDIR)/boot
EFIDIR = $(BOOTDIR)
KERNELDIR = $(SRCDIR)/kernel
BUILDDIR = $(SRCDIR)/build
EFIBUILDDIR = $(BUILDDIR)/efi
KERNELBUILDDIR = $(BUILDDIR)/kernel

# UEFI源文件
EFI_SOURCES = \
	$(EFIDIR)/boot.c \
	$(EFIDIR)/efi_utils.c

EFI_OBJECTS = $(patsubst $(EFIDIR)/%.c, $(EFIBUILDDIR)/%.o, $(EFI_SOURCES))

.PHONY: all clean uefi kernel run_uefi

# 默认目标
all: uefi kernel

# UEFI目标
uefi: $(EFIBUILDDIR)/bootx64.efi

# 内核目标
kernel: $(KERNELBUILDDIR)/kernel.bin

# UEFI引导程序
$(EFIBUILDDIR)/bootx64.efi: $(EFI_OBJECTS)
	@mkdir -p $(EFIBUILDDIR)
	$(LD) /subsystem:efi_application /entry:efi_main /out:$@ $^

# UEFI对象文件
$(EFIBUILDDIR)/%.o: $(EFIDIR)/%.c
	@mkdir -p $(EFIBUILDDIR)
	$(CC) $(CFLAGS) -I$(EFIDIR) -c $< -o $@

# 内核二进制文件 - 使用NASM编译
$(KERNELBUILDDIR)/kernel.bin: kernel/kernel.asm
	@mkdir -p $(KERNELBUILDDIR)
	nasm -f bin -o $@ $<
	
# 运行
run: uefi kernel
	@mkdir -p $(BUILDDIR)/EFI/BOOT
	cp $(EFIBUILDDIR)/bootx64.efi $(BUILDDIR)/EFI/BOOT/
	cp $(KERNELBUILDDIR)/kernel.bin $(BUILDDIR)/
	qemu-system-x86_64 -bios OVMF.fd -hda fat:rw:$(BUILDDIR) -serial stdio


clean:
	rm -rf $(BUILDDIR)
