#!/bin/bash
# Script para testar o kernel exemplo

echo "=== Teste do Kernel Exemplo ==="
echo ""

# Compilar (gera os.img e kernel_example.bin)
make || exit 1

# Verificar se os arquivos existem
if [ ! -f "os.img" ]; then
    echo "Erro: os.img não encontrado!"
    exit 1
fi

echo "Arquivos encontrados:"
ls -lh os.img kernel_example.bin
echo ""

# Testar com QEMU
echo "Iniciando teste com QEMU..."
echo "Pressione Ctrl+A X para sair do QEMU"
echo ""

qemu-system-x86_64 -drive file=os.img,format=raw -m 128M
