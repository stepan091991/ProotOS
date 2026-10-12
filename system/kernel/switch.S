; system/kernel/switch.S

[BITS 32]

section .text

; void switch_to(process_t* next) — next в [esp+4]
; offset 48 = поле esp в process_t.
global switch_to
extern current

switch_to:
    mov eax, [esp + 4]

    mov edx, [current]
    test edx, edx
    jz .load_next

    pusha
    push ds
    push es
    push fs
    push gs

    mov [edx + 48], esp

.load_next:
    mov [current], eax
    mov esp, [eax + 48]

    pop gs
    pop fs
    pop es
    pop ds
    popa

    ret

; Точка входа нового kernel thread.
; На стеке: [esp]=arg, [esp+4]=fn.
global thread_entry
extern process_exit

thread_entry:
    pop eax                 ; eax = arg
    pop ebx                 ; ebx = fn
    sti
    push eax                ; аргумент для fn
    call ebx
    add esp, 4
    call process_exit

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits