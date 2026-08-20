#!/bin/bash
# Teste final do projeto

echo "=== Teste Final do Projeto Kernel Bare-Metal x86_64 ==="
echo ""

# Verificar se os arquivos existem
echo "1. Verificando arquivos..."
files=("boot.asm" "boot.bin" "kernel.c" "kernel_example.c" "kernel.ld" "Makefile")
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
generated=("kernel.bin" "kernel_example.bin")
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
echo "5. Arquivos do projeto:"
ls -lh
echo ""

echo "=== Teste concluído ==="
echo ""
echo "Para testar com QEMU:"
echo "  ./run_qemu.sh        # Kernel simples"
echo "  ./run_example.sh     # Kernel exemplo"
echo "  ./test_kernel.sh     # Teste completo"