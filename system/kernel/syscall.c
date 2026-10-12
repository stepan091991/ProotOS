#include "syscall.h"
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "vga.h"
#include "printf.h"
#include "usermode.h"

/* Обработчик int 0x80. Регистры: eax = номер, ebx/ecx/edx — аргументы. */
void syscall_handler(registers_t* regs) {
    u32 nr = regs->eax;

    switch (nr) {
        case SYS_EXIT: {
            /* Аргумент: код возврата (игнорируем). */
            usermode_return_to_kernel();
            /* Не вернёмся. */
            break;
        }
        case SYS_WRITE: {
            /* write(fd, buf, len)
             *   ebx = fd (пока только 1 = stdout)
             *   ecx = user pointer
             *   edx = length
             */
            u32 fd  = regs->ebx;
            const char* buf = (const char*) regs->ecx;
            u32 len = regs->edx;

            if (fd == 1 && buf) {
                for (u32 i = 0; i < len; i++) {
                    vga_putchar(buf[i]);
                }
            }
            regs->eax = len;
            break;
        }
        default:
            printf("[syscall] unknown nr=%u\n", nr);
            regs->eax = (u32)-1;
            break;
    }
}

void syscall_init(void) {
    /* Вектор 0x80, DPL=3 — чтобы вызывать из ring 3. */
    extern void syscall_stub(void);
    idt_set_gate(0x80, (u32) syscall_stub, GDT_KERNEL_CODE,
                 IDT_INT_GATE_RING3);
    printf("[syscall] int 0x80 registered (DPL=3)\n");
}
