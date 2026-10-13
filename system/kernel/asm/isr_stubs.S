; system/kernel/isr_stubs.S
; ProotOS — ISR stubs (обработчики исключений и IRQ)

[BITS 32]

; ─── Макрос: исключение БЕЗ error code ─────────────────────
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push dword 0            ; заглушка error code
    push dword %1           ; номер вектора
    jmp isr_common
%endmacro

; ─── Макрос: исключение С error code ──────────────────────
; Процессор сам положил error code на стек перед переходом на isr%1.
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push dword %1           ; error code уже на стеке, пушим только номер
    jmp isr_common
%endmacro

; ─── Сами стабы исключений (0..31) ────────────────────────
ISR_NOERR 0     ; #DE Divide Error
ISR_NOERR 1     ; #DB Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; #BP Breakpoint
ISR_NOERR 4     ; #OF Overflow
ISR_NOERR 5     ; #BR Bound Range
ISR_NOERR 6     ; #UD Invalid Opcode
ISR_NOERR 7     ; #NM Device Not Available
ISR_ERR   8     ; #DF Double Fault
ISR_NOERR 9     ; Coprocessor Segment Overrun
ISR_ERR   10    ; #TS Invalid TSS
ISR_ERR   11    ; #NP Segment Not Present
ISR_ERR   12    ; #SS Stack-Segment Fault
ISR_ERR   13    ; #GP General Protection
ISR_ERR   14    ; #PF Page Fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; #MF x87 FPU Error
ISR_ERR   17    ; #AC Alignment Check
ISR_NOERR 18    ; #MC Machine Check
ISR_NOERR 19    ; #XM SIMD FP Exception
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ─── Макрос: IRQ (векторы 32..47 после ремапа PIC) ────────
%macro IRQ 1
global irq%1
irq%1:
    cli
    push dword 0            ; заглушка error code
    push dword (32 + %1)    ; номер вектора
    jmp isr_common
%endmacro

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

; ─── Общий обработчик ─────────────────────────────────────
extern isr_handler
global isr_common

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10            ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; указатель на registers_t
    call isr_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8              ; убираем int_no и err_code
    iret

; ─── Таблица адресов ISR-стабов (для C-кода) ──────────────
section .rodata
global isr_stub_table
isr_stub_table:
    dd isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    dd isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    dd isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dd isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

; ─── Таблица адресов IRQ-стабов ───────────────────────────
global irq_stub_table
irq_stub_table:
    dd irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
    dd irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15

; ─── Пустая секция, помечающая стек неисполняемым ─────────
section .note.GNU-stack noalloc noexec nowrite progbits
