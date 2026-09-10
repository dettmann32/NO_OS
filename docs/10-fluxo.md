# 10 — Linha do tempo completa: do power-on ao prompt `/$`

Agora que cada pedaço foi explicado (boot, GDT/IDT/PIC, drivers,
escalonador, syscalls, FS, terminal), vamos à sequência inteira numa
narrativa única.

## 0. Power-on (QEMU)

```
QEMU liga um PC virtual com um disquete (os.img de 1.44 MB)
CPU começa o reset: executa o BIOS de fábrica (em ROM)
BIOS: POST → prepara memória → procura disco com assinatura 0xAA55
vê o setor 0 do nosso disquete → carrega em 0x7C00 → DL = drive → salta
```

## 1. Bootloader (16 bits)

```
boot.asm [modo real 16 bits, em 0x7C00]
 ├─ ds/es/ss = 0; sp = 0x7C00 (pilha pronta)
 ├─ imprime "Booting..."
 ├─ INT 13h AH=42h (LBA): lê 64 setores → RAM em 0x10000
 ├─ imprime "Kernel carregado!"
 ├─ cli → lgdt → liga PE
 ├─ jmp 0x08:protected_mode  (CS = seletor 0x08)
 └─ [32 bits] ds/es/fs/gs/ss = 0x10; esp = 0x90000
    jmp 0x10000  ──────────────►  _start (kernel C!)
```

## 2. Kernel C — fase de configuração (`kernel/main.c`)

```
_start:
 init_bss()          // 👉 zera o .bss (arrays globais)
 vga_clear()         // 👉 tela limpa
 vga_puts(...)       // 👉 banner "Kernel Bare-Metal x86_64"
 gdt_init()          // 👉 GDT com 6 entradas (anel 0/3 + TSS)
 tss_init()          // 👉 TSS esp0 = pilha do kernel
 idt_init()          // 👉 IDT (48 stubs apontados: exceções + IRQs)
 pic_remap()         // 👉 IRQ0-15 → vetores 32-47; mascara quase tudo
 keyboard_init()     // 👉 registra handler de IRQ1 (teclado)
 syscall_init()      // 👉 IDT[0x80] com DPL 3 (ring 3 pode chamar)
 idt_set_gate(32,irq0_timer_stub,0x8E) // 👉 IRQ0 vai direto p/ escalonador
 pit_init(100)       // 👉 PIT dispara IRQ0 a 100 Hz (a partir daqui tem
                     //    "concorrência": qualquer printf durante a
                     //    inicialização dos próximos passos já pode ser
                     //    interrompida pelo timer)
 fs_init()           // 👉 monta a árvore: docs/, usr/, leiame.txt, ...
 task_init()         // 👉 zera a tabela de TCBs
 task_create(prog_terminal,1) // 👉 tarefa 1 = terminal (ring 3)
 scheduler_add_idle()         // 👉 tarefa 2 = idle (hlt)
 console_init(); console_set_cursor(0, 12) // 👉 posiciona o cursor
 scheduler_begin()   // 👉 PRIMEIRA TROCA (nunca retorna!)
```

## 3. O kernel não "segue" — ele entrega a CPU

`scheduler_begin()` chama `task_frame_enter(saved_esp)` que faz
`mov esp, saved_esp; popa; add esp,8; iret`. O frame da tarefa 1 foi
montado com `USER_DS/USER_CS/USER_ESP/EFLAGS(IF=1)/entry`. O `iret`
pula para `prog_terminal`, agora no **anel 3**, com IF ligado.

**Pronto. O kernel "morreu" (ficou ocioso na CPU): a partir daqui quem
roda é o terminal; o kernel só reage a interrupções e syscalls.**

## 4. Terminal (ring 3) imprimindo o prompt

```
prog_terminal:
  println("Kernel Bare-Metal x86_64 - Terminal")
  println("Digite 'help' para a lista de comandos.")

loop:
  print_prompt() →  sys_pwd (nos dá "/")  → sys_write "/"  + "$ "
  read_line(): loop consumindo sys_read() (tecla a tecla)
```

Cada `sys_write`/`sys_read`/`sys_pwd` faz `int 0x80`:

```
anel 3: int 0x80 (eax = número, ebx/ecx/edx = args)
  CPU: troca p/ anel 0 (TSS.esp0 = pilha do kernel da tarefa de user)
       → IDT[0x80] → syscall80 (assembly)
       → pusha; push esp; call syscall_handler(registers_t*)
       → executa, grava r->eax, retorna
       → popa; add esp,8; iret  → volta pro anel 3 no DEPOIS do int
```

enquanto isso, **100 vezes por segundo**, o PIT interrompe:

```
IRQ0 → PIC → CPU salva contexto na pilha de kernel da tarefa atual
     → IDT[32] → irq0_timer_stub (assembly)
     → pusha; push esp
     → timer_tick(esp):
          current->saved_esp = esp (guarda onde paramos)
          pic_send_eoi(0)             (libera o IRQ)
          pick_next() → próxima READY  (terminal ↔ idle, rodízio)
          tss_set_esp0(...)            (pilha de kernel da próxima)
          return saved_esp da próxima
     → mov esp, eax; popa; add esp,8; iret
     → "acorda" a próxima tarefa onde ela tinha parado
```

Isso faz o terminal "continuar rodando" mesmo quando o scheduler o
interrompe a cada 10 ms.

## 5. O usuário digita: `ls`

```
/$ ls
  read_line(): lê 'l','s','\n' (sys_read por tecla)
  parse_line() → args[0]="ls"
  cmd_ls("."):
     fd = sys_open(".", SYS_OPEN_READ)   → resolve "." para cwd → fd=0
     loop sys_fread(fd, buf, 91):
        syscall SYS_FILE_READ → dir_line() (próxima linha do dir)
            → "docs:D:0"   → imprime "docs/"
            → "usr:D:0"    → imprime "usr/"
            → "inicio.txt:F:52" → imprime "inicio.txt"
        quando n==0 (fim), sys_close(fd)
  loop do prompt: "/$ "
```

## 6. O usuário digita `echo oi > x.txt`, `cat x.txt`

```
echo oi > x.txt
  parse: args = {"echo","oi",">","x.txt"}
  cmd_echo: sys_open("x.txt", SYS_OPEN_WRITE)   → cria/trunca
            sys_fwrite(fd, "oi", 2)             → fs_write_file
            sys_close(fd)
cat x.txt
  fd = sys_open("x.txt", SYS_OPEN_READ)
  sys_fread → fs_read_sys (copia os 2 bytes de nodes[x].data)
  sys_write → tela: "oi"
  sys_close
```

## 7. O usuário digita `exit`

```
else if (str_eq(cmd,"exit")) {
    sys_exit();        // → scheduler_exit_current(): TASK_DONE
}
```

A tarefa do terminal sai da fila. Nas próximas ticks, `pick_next()` ainda
vê o idle (READY) e escala só ele. A CPU parada em `hlt` (idle) — sistema
"vivo" mas ocioso.

## Resumo em ordem cronológica (com arquivos)

```
BIOS ──► boot/boot.asm ──► kernel/main.c (_start)
        │
        ├── init/bss.c      zera o .bss
        ├── arch/gdt.c      GDT + TSS
        ├── arch/idt.c      IDT (48 stubs)
        ├── arch/pic.c      IRQs → vetores 32+
        ├── drivers/*.c     teclado, tela, timer
        ├── fs/fs.c         arquivos em RAM
        ├── scheduler/*     tarefas + troca de contexto
        └── syscall/syscall.c  int 0x80 → dispatcher
│
        │  ▼  scheduler_begin() → iret
        user/user.c  (terminal, ring 3)  →  syscalls → de volta ao kernel
```

## Código-fonte correspondente

Tudo, mas a **leitura recomendada** nessa ordem:

1. `boot/boot.asm`
2. `kernel/main.c`
3. `kernel/arch/x86/gdt.c` e `kernel/arch/x86/idt.c`
4. `kernel/arch/x86/idt_stubs.asm`
5. `kernel/scheduler/scheduler.c`
6. `kernel/syscall/syscall.c`
7. `kernel/fs/fs.c`
8. `user/user.c`