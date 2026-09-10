; Stubs das interrupções (32 exceções + 16 IRQs)
; Compilado com: nasm -f elf32

[bits 32]

extern isr_handler

; Exceção SEM código de erro na pilha (empilha 0 no lugar)
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

; Exceção COM código de erro já empilhado pelo CPU
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common_stub
%endmacro

; IRQ do PIC: nº do IRQ no primeiro argumento, vetor no segundo
%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp isr_common_stub
%endmacro

; Exceções 0-31
ISR_NOERR 0   ; Divisão por zero
ISR_NOERR 1   ; Debug
ISR_NOERR 2   ; NMI
ISR_NOERR 3   ; Breakpoint
ISR_NOERR 4   ; Overflow
ISR_NOERR 5   ; Fora de faixa
ISR_NOERR 6   ; Opcode inválido
ISR_NOERR 7   ; Dispositivo indisponível
ISR_ERR   8   ; Double fault
ISR_NOERR 9   ; Coprocessador
ISR_ERR   10  ; TSS inválida
ISR_ERR   11  ; Segmento ausente
ISR_ERR   12  ; Falha de pilha
ISR_ERR   13  ; Proteção geral (GPF)
ISR_ERR   14  ; Page fault
ISR_NOERR 15  ; Reservado
ISR_NOERR 16  ; Erro x87 FPU
ISR_ERR   17  ; Alinhamento
ISR_NOERR 18  ; Machine check
ISR_NOERR 19  ; SIMD FPU
ISR_NOERR 20  ; Virtualização
ISR_NOERR 21  ; Controle de proteção
ISR_NOERR 22  ; Reservado
ISR_NOERR 23  ; Reservado
ISR_NOERR 24  ; Reservado
ISR_NOERR 25  ; Reservado
ISR_NOERR 26  ; Reservado
ISR_NOERR 27  ; Reservado
ISR_NOERR 28  ; Reservado
ISR_NOERR 29  ; Reservado
ISR_NOERR 30  ; Reservado
ISR_NOERR 31  ; Reservado

; IRQs do PIC: 0-15 mapeados nos vetores 32-47
IRQ_STUB 0,  32
IRQ_STUB 1,  33
IRQ_STUB 2,  34
IRQ_STUB 3,  35
IRQ_STUB 4,  36
IRQ_STUB 5,  37
IRQ_STUB 6,  38
IRQ_STUB 7,  39
IRQ_STUB 8,  40
IRQ_STUB 9,  41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

; Salva os registradores, chama o handler em C e volta
isr_common_stub:
    pusha
    push esp                    ; ponteiro para registers_t
    call isr_handler
    add esp, 4                  ; remove o ponteiro
    popa
    add esp, 8                  ; remove int_no e err_code
    iret

; ---------------------------------------------------------------
; Stub dedicado do IRQ0 (timer/PIT): é aqui que acontece a troca
; de contexto do escalonador. O frame fica na pilha do kernel da
; tarefa atual; timer_tick() escolhe a próxima e retorna em eax o
; esp do frame dela (o ponto onde o popa deve começar).
; ---------------------------------------------------------------
extern timer_tick

global irq0_timer_stub
irq0_timer_stub:
    push dword 0                ; err_code (dummy)
    push dword 32               ; int_no (vetor do IRQ0)
    jmp irq0_timer_common

irq0_timer_common:
    pusha                       ; salva os 8 registradores na pilha do kernel
    push esp                    ; ponteiro para registers_t
    call timer_tick             ; eax = esp do frame da próxima tarefa
    add esp, 4                  ; remove o ponteiro
    mov esp, eax                ; MUDANÇA DE CONTEXTO: troca de pilha
    popa                        ; restaura os registradores da próxima tarefa
    add esp, 8                  ; remove int_no e err_code
    iret                        ; volta para a próxima tarefa (user ou kernel)

; ---------------------------------------------------------------
; task_frame_enter(saved_esp): entra na primeira tarefa (e em
; qualquer frame já montado), como se ela tivesse sido interrompida.
; ---------------------------------------------------------------
global task_frame_enter
task_frame_enter:               ; void task_frame_enter(uint32_t saved_esp)
    mov esp, [esp + 4]          ; carrega a pilha da tarefa
    popa
    add esp, 8
    iret

; Stub do system call int 0x80 (vetor 128)
extern syscall_handler

global syscall80
syscall80:
    push dword 0
    push dword 0x80
    jmp syscall_common_stub

syscall_common_stub:
    pusha
    push esp
    call syscall_handler
    add esp, 4
    popa
    add esp, 8
    iret

section .data
global isr_stub_table
isr_stub_table:
    dd isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    dd isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    dd isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dd isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    dd irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
    dd irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15