; Bootloader simples para x86_64
; Este bootloader carrega o kernel em modo 64-bit

[bits 16]           ; Modo real de 16 bits
[org 0x7c00]        ; Endereço onde o bootloader é carregado

; Ponto de entrada do bootloader
_start:
    ; Configurar segmentos
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Habilitar interrupt timer
    sti

    ; Carregar kernel do disco
    ; Aqui você implementaria a rotina de leitura do disco
    ; Por simplicidade, vamos apenas pular para o kernel

    ; Mudar para modo protegido de 32 bits
    cli                 ; Desabilitar interrupções
    lgdt [gdt_descriptor] ; Carregar GDT

    ; Habilitar modo protegido
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Pular para modo de 32 bits
    jmp 0x08:protected_mode

; Tabela Global de Descritores (GDT)
gdt_start:
    ; Descritor nulo
    dd 0x0
    dd 0x0

; Código segmento (0x08)
gdt_code:
    dw 0xFFFF       ; Limite (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Bandeiras de acesso
    db 11001111b    ; Bandeiras de granularidade
    db 0x0          ; Base (bits 24-31)

; Dados segmento (0x10)
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
    dd gdt_start                 ; Endereço da GDT

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

    ; Carregar kernel em 0x100000 (1MB)
    ; Aqui você implementaria a rotina de carregamento do kernel
    ; Por simplicidade, vamos apenas pular para o kernel

    ; Pular para o kernel
    jmp 0x100000

; Preencher com zeros até 510 bytes
times 510-($-$$) db 0

; Assinatura de boot
dw 0xaa55