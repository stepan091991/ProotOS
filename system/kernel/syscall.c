#include "syscall.h"
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "vga.h"
#include "printf.h"
#include "kbd_buffer.h"
#include "process.h"

void syscall_handler(registers_t* regs) {
    u32 nr = regs->eax;

    switch (nr) {
        case SYS_EXIT: {
            u32 code = regs->ebx;
            if (current) {
                current->exit_code = code;
                printf("[syscall] pid %u exit(%u)\n", current->pid, code);
                current->state = PROC_ZOMBIE;
            }
            for (;;) {
                schedule();
                __asm__ volatile ("hlt");
            }
            break;
        }
        case SYS_WRITE: {
            u32         fd  = regs->ebx;
            const char* buf = (const char*) regs->ecx;
            u32         len = regs->edx;
            if (fd == 1 || fd == 2) {
                if (buf) {
                    for (u32 i = 0; i < len; i++) vga_putchar(buf[i]);
                }
                regs->eax = len;
            } else {
                regs->eax = (u32)-1;
            }
            break;
        }
        case SYS_READ: {
            u32   fd  = regs->ebx;
            char* buf = (char*) regs->ecx;
            u32   len = regs->edx;
            if (fd == 0 && buf && len > 0) {
                u32 i = 0;
                while (i < len) {
                    int c = kbd_buffer_pop();
                    if (c < 0) {
                        /* Разрешить прерывания — иначе hlt зависнет. */
                        __asm__ volatile ("sti");
                        yield();
                        __asm__ volatile ("hlt");
                        __asm__ volatile ("cli");
                        continue;
                    }
                    buf[i++] = (char) c;
                    //vga_putchar((char) c);
                    if (c == '\n') break;
                }
                regs->eax = i;
            } else {
                regs->eax = (u32)-1;
            }
            break;
        }
        case SYS_GETPID: {
            regs->eax = current ? current->pid : 0;
            break;
        }
        case SYS_BRK: {
            (void) regs->ebx;
            regs->eax = 0x08000000;
            break;
        }
        default:
            printf("[syscall] unknown nr=%u\n", nr);
            regs->eax = (u32)-1;
            break;
    }
}

void syscall_init(void) {
    extern void syscall_stub(void);
    idt_set_gate(0x80, (u32) syscall_stub, GDT_KERNEL_CODE,
                 IDT_INT_GATE_RING3);
    printf("[syscall] int 0x80 registered (DPL=3)\n");
}