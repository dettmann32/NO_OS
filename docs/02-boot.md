# 02 — O bootloader: do reset à RAM

> Arquivo: `boot/boot.asm`
>
> O bootloader é o **primeiro código nosso** que a máquina executa. Tem
> apenas 512 bytes (um setor) e trabalho de curta duração: ler o kernel do
> disco, mudar para o modo protegido e pular para o kernel.

## 2.1 Por que assembly?

Porque no momento do boot ainda **não existe** nenhum runtime. Não há como
chamar `main()`, não há como alocar memória, e as rotinas que precisamos
(chamar o BIOS) só são acessíveis por instruções específicas que o C não
tem. O bootloader é pequeno e feio por natureza — é sempre a parte em
assembly de qualquer SO.

## 2.2 O contrato do BIOS com o bootloader

Quando o BIOS termina o POST e acha um disco com a assinatura `0xAA55` no
final do primeiro setor, ele:

1. Carrega os **512 bytes** desse setor no endereço físico **`0x7C00`**;
2. Coloca o número do drive de boot no registrador **`DL`**;
3. **Salta para `0x7C00`** executando o bootloader.

> Por isso o arquivo tem na primeira linha:
> ```asm
> [bits 16]        ; CPU ainda está em modo real (16 bits)
> [org 0x7c00]     ; avisa o montador: meus offsets são relativos a 0x7C00
> ```

## 2.3 Passo a passo do código

### (a) Preparar registradores e pilha

```asm
xor ax, ax        ; ax = 0
mov ds, ax        ; segmentos de dados = 0
mov es, ax
mov ss, ax        ; stack segment = 0
mov sp, 0x7c00    ; pilha logo abaixo do nosso código (cresce para baixo)
mov [boot_drive], dl   ; salva o número do drive que o BIOS nos deu
```

Em modo real a memória se endereça por `segmento:offseto`. Zerar os segmentos
deixa tudo no "segmento 0". A pilha é configurada para **crescer para baixo**
a partir de `0x7C00`, numa área livre (não sobrescreve nem o BIOS nem o
bootloader).

### (b) Mensagem de texto via rotina do BIOS

```asm
mov si, msg_boot
call print_bios
```

`print_bios` usa `int 0x10` (interrupção do BIOS para vídeo), modo `0x0E`
(teletype = imprime 1 char e avança). Essa é a **única** forma fácil de
escrever texto em modo real sem conhecer o hardware de vídeo:

```asm
print_bios:
    lodsb            ; AL = byte apontado por SI, SI++
    test al, al      ; é o '\0' final?
    jz .done
    mov ah, 0x0e     ; int 0x10, função 0x0E (print teletype)
    int 0x10
    jmp print_bios
.done:
    ret
```

### (c) Ler o kernel do disco — INT 13h AH=42h

Um disquete tem **setores de 512 bytes**. Como o bootloader ocupa o setor 0,
o kernel é gravado a partir do **setor 1** (definido no Makefile com
`bs=512 seek=1`). O BIOS oferece leitura "LBA estendida" via `int 0x13 AH=42h`:

```asm
mov si, dap          ; DAP = bloco de parâmetros (que setor, para onde)
mov dl, [boot_drive] ; drive (o BIOS nos passou em DL)
mov ah, 0x42
int 0x13             ; chama o BIOS
jc disk_error        ; ds: CF=1 -> erro
```

O **DAP** (Disk Address Packet) é uma estrutura de 16 bytes:

```asm
dap:
    db 0x10                 ; tamanho da estrutura (16 bytes)
    db 0                    ; reservado
    dw KERNEL_SECTORS       ; 64 setores = 32 KiB a ler
    dw 0x0000               ; offset de destino
    dw 0x1000               ; segmento de destino (1000h:0000h = 0x10000)
    dq 1                    ; LBA inicial = setor 1 (logo após o bootloader)
```

O kernel é carregado em **`0x10000`** (64 KiB) — esse valor é a constante
`KERNEL_OFFSET` e precisa ser **exatamente igual ao `KERNEL_BASE`** definido
no linker script (`linker/kernel.ld`). Sem essa correspondência, o bootloader carregaria o kernel num lugar e o
kernel esperaria estar em outro.

### (d) Entrar no modo protegido

A transição tem 4 passos padronizados:

```asm
cli              ; desliga interrupções (BIOS ainda está configurando coisas)
lgdt [gdt_descriptor]   ; carrega a GDT (ver abaixo)
mov eax, cr0     ; CR0 é o registrador de controle da CPU
or eax, 1        ; liga o bit 0 (modo protegido)
mov cr0, eax
jmp 0x08:protected_mode   ; salto longo recarrega CS com o seletor 0x08
```

> O `jmp 0x08:...` é fundamental: muda **CS** para o descritor `0x08` da GDT.
> O privilégio e o limite de CS só são (re)verificados num salto longo;
> segmentos de dados, sim, dá para trocar com `mov`.

A GDT **mínima** do bootloader tem 3 entradas: nula, código, dados.

```
GDT:
  [0] null      — obrigatória (a CPU espera a entrada 0 zerada)
  [1] code 0x08 — anel 0, base 0, limite 4 GiB, executável
  [2] data 0x10 — anel 0, base 0, limite 4 GiB
```

Cada entrada tem 8 bytes e o formato é dividido em 3 partes (limite, base,
flags) por causa de herança histórica do x86. Veremos o formato exato em
`docs/04-gdt-idt-pic.md` — o bootloader usa o mesmo formato que o kernel.

### (e) Já em 32 bits: configurar segmentos e saltar para o kernel

```asm
[bits 32]
protected_mode:
    mov ax, 0x10     ; seletor de dados do kernel (0x10)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000 ; pilha grande do 32 bits (em RAM livre)
    jmp KERNEL_OFFSET ; entrega o controle ao C: _start (kernel/main.c)
```

### (f) Assinatura de boot

```asm
times 510-($-$$) db 0   ; preenche com zeros até o byte 510
dw 0xaa55               ; os últimos 2 bytes: a "marca" de disco bootável
```

Sem `0xAA55` nos bytes 510-511, o BIOS (e o QEMU) **não reconhecem** o disco.

## 2.4 Diagrama do fluxo

```
BIOS (POST)
  │ lê setor 0 → RAM 0x7C00
  ▼
boot.asm [16 bits]
  │ ds/es/ss/sp = 0x7C00
  │ imprime "Booting..."
  │ int 13h AH=42h ← lê 64 setores → RAM 0x10000 (KERNEL_OFFSET)
  │ imprime "Kernel carregado!"
  ▼
cli → lgdt → liga PE (bit 0 do CR0)
  ▼
jmp 0x08:protected_mode        (CS = seletor kernel code)
  ▼
[32 bits] mov ds/es/fs/gs/ss = 0x10; esp = 0x90000
  ▼
jmp 0x10000  →  _start no kernel C
```

## 2.5 Por que o kernel tem que ser pequeno?

O bootloader só lê `KERNEL_SECTORS` (64 setores = 32 KiB). Se o `kernel.bin`
cresce mais, o excedente não é lido e a máquina quebra. Para conferir o
tamanho: `ls -l build/kernel.bin` (ele deve estar folgado abaixo de 32 KiB).

## Nota sobre "x86_64" no título

O processador físico pode ser x86-64, mas aqui trabalhamos em **modo
protegido 32 bits** (o bootloader nunca chega a ativar o modo longo/64).
Isso simplifica tudo: registradores, endereços, e o compilador usando `-m32`.

## Próximo passo

Veja como o `kernel.bin` é montado (Makefile) e organizado na memória
(linker script): `docs/03-build.md`.

## Código-fonte correspondente

| Arquivo | O que está relacionado |
|---------|------------------------|
| `boot/boot.asm` | todo o bootloader explicado acima |
| `linker/kernel.ld` | `KERNEL_BASE = 0x10000` (deve bater com `KERNEL_OFFSET`) |
| `Makefile` | `bs=512 seek=1` grava o kernel a partir do setor 1 |