# Makefile para kernel bare-metal x86

# Compilador e flags
CC = gcc
CFLAGS = -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
         -m32 -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra -c
LDFLAGS = -m elf_i386 -T linker/kernel.ld -nostdlib

NASM = nasm
NASMFLAGS = -f bin      # bootloader (binário puro)
NASM_FLAT = -f elf32    # assembly do kernel (objeto ELF)

DISK_SIZE = 1474560  # 1.44 MB (imagem de disquete)

# Fontes do kernel (entry point, arquitetura, drivers e inicialização)
SOURCES = kernel/main.c \
          kernel/arch/x86/gdt.c \
          kernel/arch/x86/idt.c \
          kernel/arch/x86/pic.c \
          kernel/drivers/vga.c \
          kernel/drivers/pit.c \
          kernel/drivers/console.c \
          kernel/drivers/keyboard.c \
          kernel/init/bss.c \
          kernel/syscall/syscall.c \
          kernel/scheduler/scheduler.c \
          kernel/fs/fs.c
ASM_SOURCES = kernel/arch/x86/idt_stubs.asm

OBJECTS = $(patsubst kernel/%.c,build/%.o,$(SOURCES)) \
          build/user.o \
          $(patsubst kernel/%.asm,build/%.o,$(ASM_SOURCES))
SUBDIRS = $(sort $(dir $(OBJECTS)))

# Regra principal: monta a imagem de disco completa (boot + kernel)
all: build/os.img

build:
	mkdir -p $(SUBDIRS)

# Compilar bootloader com NASM
build/boot.bin: boot/boot.asm | build
	$(NASM) $(NASMFLAGS) $< -o $@

# Compilar cada fonte do kernel (C)
build/%.o: kernel/%.c | build
	$(CC) $(CFLAGS) $< -o $@

# Compilar a aplicação do usuário (roda em ring 3, própria seção)
build/user.o: user/user.c | build
	$(CC) $(CFLAGS) $< -o $@

# Compilar cada fonte do kernel (Assembly ELF)
build/%.o: kernel/%.asm | build
	$(NASM) $(NASM_FLAT) $< -o $@

# Linkar os objetos usando o linker script e converter para binário puro
build/kernel.elf: $(OBJECTS) linker/kernel.ld
	ld $(LDFLAGS) $(OBJECTS) -o $@

build/kernel.bin: build/kernel.elf
	objcopy -O binary $< $@

# Montar imagem de disco: bootloader no setor 0, kernel a partir do setor 1
build/os.img: build/boot.bin build/kernel.bin
	cp build/boot.bin $@
	truncate -s $(DISK_SIZE) $@
	dd if=build/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=none

# Rodar no QEMU
run: build/os.img
	qemu-system-x86_64 -drive file=build/os.img,format=raw -m 128M

# Limpar arquivos gerados
clean:
	rm -rf build

.PHONY: all clean run