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
BUILDDIR ?= $(SRCDIR)/build
EFIBUILDDIR = $(BUILDDIR)/efi
KERNELBUILDDIR = $(BUILDDIR)/kernel

# UEFI源文件
EFI_SOURCES = \
	$(EFIDIR)/boot.c \
	$(EFIDIR)/efi_utils.c

EFI_OBJECTS = $(patsubst $(EFIDIR)/%.c, $(EFIBUILDDIR)/%.o, $(EFI_SOURCES))

.PHONY: all clean uefi kernel disk run

# 默认目标
all: uefi kernel disk

# UEFI目标
uefi: $(EFIBUILDDIR)/bootx64.efi

KERNEL_SOURCES = \
	$(KERNELDIR)/kernel.asm \
	$(KERNELDIR)/kmain.c

KERNEL_OBJECTS = \
	$(KERNELBUILDDIR)/kmain.o

KERNEL_CFLAGS = -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -Wall -Wextra -O2

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

$(KERNELBUILDDIR)/kernel.bin: $(KERNELBUILDDIR)/kernel.elf
	objcopy -O binary $< $@

$(KERNELBUILDDIR)/kernel.elf: $(KERNEL_OBJECTS) $(KERNELDIR)/kernel.ld
	@mkdir -p $(KERNELBUILDDIR)
	ld.lld -nostdlib -T $(KERNELDIR)/kernel.ld -o $@ $(KERNEL_OBJECTS)

$(KERNELBUILDDIR)/kernel.o: $(KERNELDIR)/kernel.asm
	@mkdir -p $(KERNELBUILDDIR)
	nasm -f elf64 -o $@ $<

$(KERNELBUILDDIR)/kmain.o: $(KERNELDIR)/kmain.c
	@mkdir -p $(KERNELBUILDDIR)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@
	
# 磁盘映像目标
disk: $(BUILDDIR)/disk.img

$(BUILDDIR)/disk.img: uefi kernel
	@mkdir -p $(BUILDDIR)/EFI/BOOT
	dd if=/dev/zero of=$@ bs=1M count=64
	mkfs.fat -F 32 $@
	mmd -i $@ ::EFI
	mmd -i $@ ::EFI/BOOT
	mcopy -i $@ $(EFIBUILDDIR)/bootx64.efi ::EFI/BOOT/bootx64.efi
	mcopy -i $@ $(KERNELBUILDDIR)/kernel.bin ::kernel.bin

# 运行
run: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio

# 调试运行（使用GDB）
debug: disk
	qemu-system-x86_64 -bios OVMF.fd -drive file=$(BUILDDIR)/disk.img,format=raw -serial stdio -s -S

clean:
	rm -rf $(BUILDDIR)
