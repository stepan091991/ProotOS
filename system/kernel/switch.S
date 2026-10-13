; system/kernel/switch.S
[BITS 32]
section .text

; void switch_to(process_t* next)
; offset 48 = esp, offset 52 = cr3
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

    ; Переключить CR3, если задан.
    mov ecx, [eax + 52]
    test ecx, ecx
    jz .skip_cr3
    mov cr3, ecx
.skip_cr3:

    pop gs
    pop fs
    pop es
    pop ds
    popa

    ret

; Точка входа kernel thread.
global thread_entry
extern process_exit

thread_entry:
    pop eax                 ; arg
    pop ebx                 ; fn
    sti
    push eax
    call ebx
    add esp, 4
    call process_exit
.hang:
    cli
    hlt
    jmp .hang

; Точка входа user-процесса.
; На стеке: eip, cs, eflags, esp, ss.
global user_entry

user_entry:
    iret

section .note.GNU-stack noalloc noexec nowrite progbits