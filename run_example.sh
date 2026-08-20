#!/bin/bash
# Script para testar o kernel exemplo

echo "=== Teste do Kernel Exemplo ==="
echo ""

# Verificar se os arquivos existem
if [ ! -f "boot.bin" ]; then
    echo "Erro: boot.bin não encontrado!"
    exit 1
fi

if [ ! -f "kernel_example.bin" ]; then
    echo "Erro: kernel_example.bin não encontrado!"
    echo "Execute 'make' primeiro."
    exit 1
fi

echo "Arquivos encontrados:"
ls -lh boot.bin kernel_example.bin
echo ""

# Testar com QEMU
echo "Iniciando teste com QEMU..."
echo "Pressione Ctrl+A然后X para sair do QEMU"
echo ""

qemu-system-x86_64 -drive file=boot.bin,format=raw -m 128M