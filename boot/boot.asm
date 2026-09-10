; Bootloader x86
; Carrega o kernel do disco (LBA 1 em diante) em 0x10000,
; entra em modo protegido e pula para o kernel.

[bits 16]
[org 0x7c00]

KERNEL_OFFSET   equ 0x10000     ; Endereço linear onde o kernel é carregado
KERNEL_SECTORS  equ 64          ; Quantidade de setores a carregar (32 KiB)

_start:
    ; Configurar segmentos e pilha
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Salvar número do drive de boot (BIOS passa em DL)
    mov [boot_drive], dl

    ; Imprimir "Boot..." via BIOS
    mov si, msg_boot
    call print_bios

    ; Carregar kernel do disco usando INT 13h AH=42h (leitura LBA)
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov si, msg_ok
    call print_bios

    ; Entrar em modo protegido
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_mode

disk_error:
    mov si, msg_err
    call print_bios
.halt:
    hlt
    jmp .halt

; print_bios: imprime string terminada em nulo apontada por SI
print_bios:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp print_bios
.done:
    ret

; Bloco de parâmetros para leitura estendida (LBA)
dap:
    db 0x10                 ; Tamanho do DAP
    db 0                    ; Reservado
    dw KERNEL_SECTORS       ; Número de setores a ler
    dw 0x0000               ; Offset (ES:BX = 1000h:0000h = 0x10000)
    dw 0x1000               ; Segmento
    dq 1                    ; LBA inicial (setor logo após o bootloader)

boot_drive: db 0

msg_boot db "Booting... ", 0
msg_ok   db "Kernel carregado!", 13, 10, 0
msg_err  db "Erro ao ler o disco!", 13, 10, 0

; Tabela Global de Descritores (GDT)
gdt_start:
    ; Descritor nulo
    dd 0x0
    dd 0x0

; Código segmento (0x08): base 0, limite 4GB
gdt_code:
    dw 0xFFFF       ; Limite (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Bandeiras de acesso
    db 11001111b    ; Bandeiras de granularidade
    db 0x0          ; Base (bits 24-31)

; Dados segmento (0x10): base 0, limite 4GB
gdt_data:
    dw 0xFFFF       ; Limite (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10010010b    ; Bandeiras de acesso
    db 11001111b    ; Bandeiras de granularidade
    db 0x0          ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Tamanho da GDT
    dd gdt_start                ; Endereço da GDT

; Modo protegido de 32 bits
[bits 32]
protected_mode:
    ; Configurar segmentos para modo protegido
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Configurar pilha
    mov esp, 0x90000

    ; Pular para o kernel já carregado em memória
    jmp KERNEL_OFFSET

; Preencher com zeros até 510 bytes
times 510-($-$$) db 0

; Assinatura de boot
dw 0xaa55