#!/bin/bash
# Script para testar o kernel com QEMU

echo "Testando kernel com QEMU..."
echo "Pressione Ctrl+A X para sair"

# Testar com QEMU usando a imagem completa (bootloader + kernel)
qemu-system-x86_64 -drive file=os.img,format=raw -m 128M
