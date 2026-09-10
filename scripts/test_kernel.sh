#!/bin/bash
# Script para compilar e testar o kernel

# Ir para a raiz do projeto (caminho relativo ao script, não ao CWD)
cd "$(dirname "$0")/.." || exit 1

echo "=== Teste do Kernel Bare-Metal ==="
echo ""

# Compilar tudo (build/boot.bin, build/kernel.bin e build/os.img)
echo "Compilando..."
make clean >/dev/null
if ! make; then
    echo "Erro na compilação!"
    exit 1
fi
echo ""

# Verificar se os arquivos existem
for file in build/boot.bin build/kernel.bin build/os.img; do
    if [ ! -f "$file" ]; then
        echo "Erro: $file não encontrado!"
        exit 1
    fi
done

echo "Arquivos gerados:"
ls -lh build/boot.bin build/kernel.bin build/os.img
echo ""

# Testar com QEMU
echo "Iniciando teste com QEMU..."
echo "Pressione Ctrl+A X para sair do QEMU"
echo ""

qemu-system-x86_64 -drive file=build/os.img,format=raw -m 128M