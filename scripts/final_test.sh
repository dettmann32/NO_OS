#!/bin/bash
# Teste final do projeto

# Ir para a raiz do projeto (caminho relativo ao script, não ao CWD)
cd "$(dirname "$0")/.." || exit 1

echo "=== Teste Final do Projeto Kernel Bare-Metal x86_64 ==="
echo ""

# Verificar se os arquivos existem
echo "1. Verificando arquivos..."
files=("boot/boot.asm" "kernel/main.c" "kernel/arch/x86/idt.c" "kernel/arch/x86/pic.c" "kernel/drivers/vga.c" "kernel/drivers/pit.c" "kernel/drivers/console.c" "kernel/drivers/keyboard.c" "kernel/init/bss.c" "linker/kernel.ld" "Makefile")
for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo "   ✓ $file encontrado"
    else
        echo "   ✗ $file NÃO encontrado"
    fi
done
echo ""

# Compilar kernel
echo "2. Compilando kernel..."
make
if [ $? -eq 0 ]; then
    echo "   ✓ Compilação bem-sucedida"
else
    echo "   ✗ Erro na compilação"
    exit 1
fi
echo ""

# Verificar arquivos gerados
echo "3. Verificando arquivos gerados..."
generated=("build/boot.bin" "build/kernel.bin" "build/os.img")
for file in "${generated[@]}"; do
    if [ -f "$file" ]; then
        echo "   ✓ $file gerado ($(ls -lh $file | awk '{print $5}'))"
    else
        echo "   ✗ $file NÃO gerado"
    fi
done
echo ""

# Verificar QEMU
echo "4. Verificando QEMU..."
if command -v qemu-system-x86_64 &> /dev/null; then
    echo "   ✓ QEMU instalado"
else
    echo "   ✗ QEMU NÃO instalado"
fi
echo ""

# Listar arquivos
echo "5. Estrutura do projeto:"
tree -L 3 --dirsfirst 2>/dev/null || ls -R --group-directories-first | head -50
echo ""

echo "=== Teste concluído ==="
echo ""
echo "Para testar com QEMU:"
echo "  ./scripts/run_qemu.sh        # Executa o kernel"
echo "  ./scripts/test_kernel.sh     # Compila e testa"