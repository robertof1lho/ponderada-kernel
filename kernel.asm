bits 32

section .text
    ; =========================
    ; Cabeçalho Multiboot (GRUB)
    ; =========================
    align 4
    dd 0x1BADB002              ; magic
    dd 0x00                    ; flags
    dd -(0x1BADB002 + 0x00)    ; checksum (m+f+c == 0)

global start
global keyboard_handler
global read_port
global write_port
global load_idt

extern kmain                   ; definido em C
extern keyboard_handler_main   ; definido em C

; =========================
; I/O ports
; =========================
read_port:
    mov edx, [esp + 4]         ; edx = port
    in  al, dx                 ; lê 8 bits de dx -> al
    ret

write_port:
    mov edx, [esp + 4]         ; edx = port
    mov al,  [esp + 8]         ; al  = data
    out dx, al
    ret

; =========================
; carregar IDT
; void load_idt(struct IDTPointer* idtp);
; =========================
load_idt:
    mov edx, [esp + 4]         ; edx = ponteiro para struct {limit, base} (packed)
    lidt [edx]                 ; carrega IDT
    sti                        ; habilita interrupções (pode deixar aqui)
    ret

; =========================
; IRQ1 stub -> chama C e retorna com iret
; =========================
keyboard_handler:
    call keyboard_handler_main
    iretd

; =========================
; Entry point
; =========================
start:
    cli                        ; desabilita interrupções durante setup
    mov esp, stack_space       ; configura topo da pilha
    call kmain                 ; entra no kernel (loop principal)
    hlt                        ; segurança (kmain não retorna)

; =========================
; BSS: pilha
; =========================
section .bss
    resb 8192                  ; 8 KiB de stack
stack_space:

; =========================
; Evita warning: .note.GNU-stack
; =========================
section .note.GNU-stack noalloc noexec nowrite align=4
