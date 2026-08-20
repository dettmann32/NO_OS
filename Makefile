# Makefile para kernel bare-metal x86_64

# Compilador e flags
CC = gcc
CFLAGS = -ffreestanding -O2 -Wall -Wextra -m64 -c
LDFLAGS = -T kernel.ld -nostdlib -nostartfiles -nodefaultlibs

# Arquivos de origem
OBJS = kernel.o
EXAMPLE_OBJS = kernel_example.o

# Regra principal
all: kernel.bin kernel_example.bin

# Compilar kernel.c para kernel.o
kernel.o: kernel.c
	$(CC) $(CFLAGS) kernel.c -o kernel.o

# Compilar kernel_example.c para kernel_example.o
kernel_example.o: kernel_example.c
	$(CC) $(CFLAGS) kernel_example.c -o kernel_example.o

# Linkar kernel.o usando kernel.ld
kernel.bin: kernel.o
	$(CC) $(LDFLAGS) kernel.o -o kernel.bin

# Linkar kernel_example.o usando kernel.ld
kernel_example.bin: kernel_example.o
	$(CC) $(LDFLAGS) kernel_example.o -o kernel_example.bin

# Limpar arquivos gerados
clean:
	rm -f *.o kernel.bin kernel_example.bin

.PHONY: all clean