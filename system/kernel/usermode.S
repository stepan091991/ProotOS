; system/kernel/usermode.S
[BITS 32]

section .text
global enter_user_mode
global usermode_return_to_kernel

; void enter_user_mode(u32 entry, u32 user_stack)
enter_user_mode:
    ; Сохранить esp ядра в глобальную переменную.
    mov [saved_kernel_esp], esp

    mov eax, [esp + 4]     ; entry
    mov ebx, [esp + 8]     ; user_stack

    cli

    ; Сегменты данных user (RPL=3).
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    ; Строим стек для iret: ss, esp, eflags, cs, eip.
    push dword 0x23        ; ss
    push ebx               ; esp
    pushfd                 ; eflags
    pop ecx
    or ecx, 0x200          ; IF=1 — разрешить прерывания
    push ecx
    push dword 0x1B        ; cs
    push eax               ; eip

    iret

; Восстановить esp ядра и вернуться в elf_exec.
usermode_return_to_kernel:
    mov esp, [saved_kernel_esp]
    ret

section .data
saved_kernel_esp: dd 0

section .note.GNU-stack noalloc noexec nowrite progbits
