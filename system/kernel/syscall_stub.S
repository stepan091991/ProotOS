; system/kernel/syscall_stub.S
[BITS 32]

section .text
global syscall_stub
extern isr_common

syscall_stub:
    cli
    push dword 0            ; заглушка error code
    push dword 0x80         ; номер вектора
    jmp isr_common          ; используем общий обработчик

section .note.GNU-stack noalloc noexec nowrite progbits
