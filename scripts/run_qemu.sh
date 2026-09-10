#!/bin/bash
# Script para testar o kernel com QEMU

# Ir para a raiz do projeto (caminho relativo ao script, não ao CWD)
cd "$(dirname "$0")/.." || exit 1

echo "Testando kernel com QEMU..."
echo "Pressione Ctrl+A X para sair"

# Testar com QEMU usando a imagem completa (bootloader + kernel)
qemu-system-x86_64 -drive file=build/os.img,format=raw -m 128M