#!/bin/bash
# Script para testar o kernel

echo "=== Teste do Kernel Bare-Metal x86_64 ==="
echo ""

# Verificar se os arquivos existem
if [ ! -f "boot.bin" ]; then
    echo "Erro: boot.bin não encontrado!"
    exit 1
fi

if [ ! -f "kernel.bin" ]; then
    echo "Erro: kernel.bin não encontrado!"
    exit 1
fi

echo "Arquivos encontrados:"
ls -lh boot.bin kernel.bin
echo ""

# Compilar kernel (se necessário)
echo "Compilando kernel..."
make
echo ""

# Testar com QEMU
echo "Iniciando teste com QEMU..."
echo "Pressione Ctrl+A然后X para sair do QEMU"
echo ""

qemu-system-x86_64 -drive file=boot.bin,format=raw -m 128M