#!/bin/bash
# Script para testar o kernel

echo "=== Teste do Kernel Bare-Metal ==="
echo ""

# Compilar tudo (boot.bin, kernel.bin e os.img)
echo "Compilando..."
make clean >/dev/null
if ! make; then
    echo "Erro na compilação!"
    exit 1
fi
echo ""

# Verificar se os arquivos existem
for file in boot.bin kernel.bin os.img; do
    if [ ! -f "$file" ]; then
        echo "Erro: $file não encontrado!"
        exit 1
    fi
done

echo "Arquivos gerados:"
ls -lh boot.bin kernel.bin os.img
echo ""

# Testar com QEMU
echo "Iniciando teste com QEMU..."
echo "Pressione Ctrl+A X para sair do QEMU"
echo ""

qemu-system-x86_64 -drive file=os.img,format=raw -m 128M
