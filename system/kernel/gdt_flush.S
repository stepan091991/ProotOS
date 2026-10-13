; system/kernel/gdt_flush.S
; void gdt_flush(u32 gdt_ptr_addr)
; Аргумент приходит в стеке, по соглашению cdecl: [esp+4].

[BITS 32]

section .text
global gdt_flush
extern gdt_flush_done

gdt_flush:
    mov eax, [esp + 4]      ; адрес структуры gdt_ptr
    lgdt [eax]              ; загружаем GDTR

    ; Перезагружаем сегментные регистры данных: cs перезагрузить нельзя
    ; обычным mov, только через far jump или far return.
    mov ax, 0x10            ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Перезагружаем CS через far jump на метку в этом же коде.
    ; 0x08 — селектор кода ядра.
    jmp 0x08:.reload_cs
.reload_cs:
    ret
