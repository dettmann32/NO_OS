#!/bin/bash
# Script para testar o kernel com QEMU

echo "Testando kernel com QEMU..."
echo "Pressione Ctrl+A然后X para sair"

# Testar com QEMU
qemu-system-x86_64 -drive file=boot.bin,format=raw -m 128M