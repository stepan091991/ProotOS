#include "syscall.h"
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "vga.h"
#include "printf.h"
#include "kbd_buffer.h"
#include "process.h"
#include "fat32.h"

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

            if (!buf || len == 0) {
                regs->eax = (u32)-1;
                break;
            }

            if (fd == 0) {
                u32 i = 0;
                while (i < len) {
                    int c = kbd_buffer_pop();
                    if (c < 0) { regs->eax = i; return; }
                    buf[i++] = (char) c;
                    if (c == '\n') break;
                }
                regs->eax = i;
            } else if (current && fd >= 3 && fd < MAX_FDS &&
                current->fd_open[fd]) {
                u32 n = fat32_read(&current->fds[fd], buf, len);
            regs->eax = n;
                } else {
                    regs->eax = (u32)-1;
                }
                break;
        }

        case SYS_OPEN: {
            const char* path = (const char*) regs->ebx;
            if (!path || !current) { regs->eax = (u32)-1; break; }

            regs->eax = (u32)-1;
            for (int i = 3; i < MAX_FDS; i++) {
                if (!current->fd_open[i]) {
                    if (fat32_open(path, &current->fds[i])) {
                        current->fd_open[i] = 1;
                        regs->eax = (u32) i;
                    }
                    break;
                }
            }
            break;
        }

        case SYS_CLOSE: {
            u32 fd = regs->ebx;
            if (current && fd >= 3 && fd < MAX_FDS && current->fd_open[fd]) {
                fat32_close(&current->fds[fd]);
                current->fd_open[fd] = 0;
                regs->eax = 0;
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
            printf("[syscall] unknown nr=%u  ebx=%u ecx=%u edx=%u eip=%p\n",
                   nr, regs->ebx, regs->ecx, regs->edx, (void*) regs->eip);
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
