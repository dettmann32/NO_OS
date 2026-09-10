; ============================================================
; BOOTLOADER (modo real 16 bits)
; A máquina ainda está no "modo real": sem proteção, memória
; endereçada por segmento:offseto, registradores de 16 bits.
;
; Texto didático completo: docs/02-boot.md
;
; O que este arquivo faz:
;   1. Configura segmentos e pilha
;   2. Lê o kernel (64 setores) do disco para a RAM em 0x10000
;      (INT 13h AH=42h = leitura LBA estendida do BIOS)
;   3. Entra em modo protegido (32 bits) com uma GDT mínima
;   4. Pula para o kernel C (_start) em KERNEL_OFFSET
; ============================================================

[bits 16]
[org 0x7c00]

; KERNEL_OFFSET deve bater EXATAMENTE com KERNEL_BASE no
; linker/kernel.ld. É o endereço onde o kernel.c será linkado
; e onde este bootloader o carrega na RAM.
KERNEL_OFFSET   equ 0x10000     ; Endereço linear onde o kernel é carregado
KERNEL_SECTORS  equ 64          ; Quantidade de setores a carregar (32 KiB)

_start:
    ; -----------------------------------------------------
    ; (1) Configurar segmentos e pilha
    ; Em modo real zero-segmentos práticos e criamos a pilha
    ; logo abaixo de nós. A pilha CRESCE PARA BAIXO; 0x7C00
    ; está logo antes do bootloader, em área livre.
    ; -----------------------------------------------------
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

    ; -----------------------------------------------------
    ; (2) Ler o kernel do disco
    ; INT 13h AH=42h = leitura estendida (LBA) do BIOS.
    ; SI aponta para um "DAP" (Descriptor Address Packet):
    ; estrutura de 16 bytes que descreve quantos setores ler
    ; e para onde escrever na RAM.
    ; -----------------------------------------------------
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error          ; CF=1 → BIOS reportou erro de leitura

    mov si, msg_ok
    call print_bios

    ; -----------------------------------------------------
    ; (3) Entrar em modo protegido (32 bits)
    ;  cli       →  desliga interrupções (BIOS usa vetores 16 bits;
    ;               aqui em diante a IDT/Bios antiga não vale mais)
    ;  lgdt      →  carrega a GDT mínima (null + code + data)
    ;  CR0.PE=1  →  liga o modo protegido (bit 0 de CR0)
    ;  jmp 0x08: →  salto longo: troca CS para o seletor 0x08
    ;               (código do kernel). Só um "far jump" recarrega
    ;               CS com o novo seletor; o resto do segmento pode
    ;               continuar antigo até trocarmos com mov.
    ; -----------------------------------------------------
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
; usando a interrupção INT 0x10 AH=0x0E do BIOS (teletype).
; "print" em modo real = pedir para o BIOS escrever na tela.
print_bios:
    lodsb                      ; AL = byte em [SI]; SI++
    test al, al                ; chegou no '\0'?
    jz .done
    mov ah, 0x0e
    int 0x10                   ; BIOS: imprime o caractere em AL
    jmp print_bios
.done:
    ret

; -----------------------------------------------------
; DAP — Disk Address Packet (INT 13h AH=42h)
; LBA 1 = primeiro setor após o bootloader (que é o setor 0).
; O Makefile grava o kernel com `seek=1` exatamente por isso.
; ES:BX = 0x1000:0x0000 → endereço físico 0x10000.
; -----------------------------------------------------
dap:
    db 0x10                 ; Tamanho do DAP
    db 0                    ; Reservado
    dw KERNEL_SECTORS       ; Número de setores a ler
    dw 0x0000               ; Offset (ES:BX = 1000h:0000h = 0x10000)
    dw 0x1000               ; Segmento
    dq 1                    ; LBA inicial (setor logo após o bootloader)

boot_drive: db 0

msg_boot db "Booting... ", 0
msg_ok   db "Kernel carregado!", 13, 10, 0      ; \r\n para quebrar linha
msg_err  db "Erro ao ler o disco!", 13, 10, 0

; -----------------------------------------------------
; GDT mínima (modo real NÃO usa GDT; a partir do boot a CPU
; passa a consultar estes descritores a cada acesso a memória)
;
;   [0] null  — entrada 0 é obrigatória zerada
;   [1] code  = seletor 0x08 → base 0, limite 4 GiB, executável
;   [2] data  = seletor 0x10 → dados legíveis/graváveis
;
; O formato de 8 bytes por entrada é dividido em "limite/base/
; flags" por herança histórica do x86 (ver docs/04-gdt-idt-pic.md).
; -----------------------------------------------------
gdt_start:
    ; Descritor nulo
    dd 0x0
    dd 0x0

; Código segmento (0x08): base 0, limite 4GB
gdt_code:
    dw 0xFFFF       ; Limite (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Bandeiras de acesso  → presente, anel 0, executável
    db 11001111b    ; Bandeiras de gran   → 4K, 32-bit, limite alto 0xF
    db 0x0          ; Base (bits 24-31)

; Dados segmento (0x10): base 0, limite 4GB
gdt_data:
    dw 0xFFFF       ; Limite (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10010010b    ; Bandeiras de acesso  → presente, anel 0, dados
    db 11001111b    ; Bandeiras de gran   → 4K, 32-bit, limite alto 0xF
    db 0x0          ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Tamanho da GDT (em bytes - 1)
    dd gdt_start                ; Endereço da GDT

; Modo protegido de 32 bits
[bits 32]
protected_mode:
    ; Configurar segmentos para modo protegido
    ; CS já é 0x08 (via far jump). Os demais segmentos de dados
    ; precisam ser recarregados com o seletor de dados 0x10.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Configurar pilha: agora "esp" (32 bits) aponta para uma
    ; área alta de RAM livre. 0x90000 não conflita com o kernel
    ; (que está em 0x10000..) nem com o VGA (0xB8000).
    mov esp, 0x90000

    ; Pular para o kernel já carregado em memória
    jmp KERNEL_OFFSET

; Preencher com zeros até 510 bytes
times 510-($-$$) db 0

; Assinatura de boot: o BIOS só inicia discos cujos bytes 510-511
; sejam AA55. É o "cartão de visita" do disquete bootável.
dw 0xaa55