; system/kernel/idt_flush.S
; void idt_flush(u32 idt_ptr_addr)
; Загружает IDTR из структуры, лежащей по адресу аргумента.

[BITS 32]

section .text
global idt_flush

idt_flush:
    mov eax, [esp + 4]     ; первый аргумент
    lidt [eax]             ; загрузить IDTR
    ret
