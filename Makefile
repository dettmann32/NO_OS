# Makefile para kernel bare-metal x86

# Compilador e flags
CC = gcc
CFLAGS = -ffreestanding -fno-pie -fno-pic -fno-stack-protector -fno-builtin \
         -m32 -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra -c
LDFLAGS = -m elf_i386 -T kernel.ld -nostdlib

NASM = nasm
NASMFLAGS = -f bin

DISK_SIZE = 1474560  # 1.44 MB (imagem de disquete)

# Arquivos de origem
OBJS = kernel.o
EXAMPLE_OBJS = kernel_example.o

# Regra principal: monta a imagem de disco completa (boot + kernel)
all: os.img kernel_example.bin

# Montar imagem de disco: bootloader no setor 0, kernel a partir do setor 1
os.img: boot.bin kernel.bin
	cp boot.bin os.img
	truncate -s $(DISK_SIZE) os.img
	dd if=kernel.bin of=os.img bs=512 seek=1 conv=notrunc status=none

# Compilar bootloader com NASM
boot.bin: boot.asm
	$(NASM) $(NASMFLAGS) boot.asm -o boot.bin

# Compilar kernel.c para kernel.o
kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o

# Compilar kernel_example.c para kernel_example.o
kernel_example.o: kernel_example.c
	$(CC) $(CFLAGS) kernel_example.c -o kernel_example.o

# Linkar kernel.o usando kernel.ld e converter para binário puro
kernel.elf: kernel.o kernel.ld
	ld $(LDFLAGS) kernel.o -o kernel.elf

kernel.bin: kernel.elf
	objcopy -O binary kernel.elf $@

# Linkar kernel_example.o usando kernel.ld e converter para binário puro
kernel_example.elf: kernel_example.o kernel.ld
	ld $(LDFLAGS) kernel_example.o -o kernel_example.elf

kernel_example.bin: kernel_example.elf
	objcopy -O binary kernel_example.elf $@

# Limpar arquivos gerados
clean:
	rm -f *.o *.elf *.bin *.tmp os.img

.PHONY: all clean run

run: os.img
	qemu-system-x86_64 -drive file=os.img,format=raw -m 128M
