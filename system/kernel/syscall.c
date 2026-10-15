#include "syscall.h"
#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "vga.h"
#include "printf.h"
#include "kbd_buffer.h"
#include "process.h"
#include "fat32.h"
#include "pit.h"

static int stat_type(const char* path) {
    if (fat32_is_dir(path)) return STAT_DIR;
    fat32_file_t f;
    if (fat32_open(path, &f)) { fat32_close(&f); return STAT_FILE; }
    return STAT_NOENT;
}

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
            for (;;) { schedule(); __asm__ volatile ("hlt"); }
            break;
        }

        case SYS_WRITE: {
            u32         fd  = regs->ebx;
            const char* buf = (const char*) regs->ecx;
            u32         len = regs->edx;
            if (!buf) { regs->eax = (u32)-1; break; }

            if (fd == 1 || fd == 2) {
                for (u32 i = 0; i < len; i++) vga_putchar(buf[i]);
                regs->eax = len;
            } else if (current && fd >= 3 && fd < MAX_FDS &&
                current->fds[fd].used) {
                process_fd_t* f = &current->fds[fd];
            if (!f->wmode) { regs->eax = (u32)-1; break; }
            if (f->wlen + len > FD_WBUF_MAX) { regs->eax = (u32)-1; break; }
            memcpy(&f->wbuf[f->wlen], buf, len);
            f->wlen += len;
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
            if (!buf || len == 0) { regs->eax = (u32)-1; break; }

            if (fd == 0 && (!current || !current->fds[0].used)) {
                u32 i = 0;
                while (i < len) {
                    int c = kbd_buffer_pop();
                    if (c < 0) {
                        __asm__ volatile ("sti; hlt; cli");
                        continue;
                    }
                    buf[i++] = (char) c;
                    if (c == '\n') break;
                }
                regs->eax = i;
            } else if (current && fd < MAX_FDS && current->fds[fd].used) {
                u32 n = fat32_read(&current->fds[fd].file, buf, len);
                regs->eax = n;
            } else {
                regs->eax = (u32)-1;
            }
            break;
        }

        case SYS_OPEN: {
            const char* path  = (const char*) regs->ebx;
            int         flags = (int) regs->ecx;
            if (!path || !current) { regs->eax = (u32)-1; break; }

            int wmode = (flags & (O_WRONLY | O_CREAT | O_TRUNC)) != 0;

            if ((flags & O_CREAT) && stat_type(path) == STAT_NOENT) {
                fat32_create(path);
            }

            regs->eax = (u32)-1;
            for (int i = 3; i < MAX_FDS; i++) {
                if (current->fds[i].used) continue;

                if (fat32_open(path, &current->fds[i].file)) {
                    current->fds[i].used  = 1;
                    current->fds[i].wmode = (u8) wmode;
                    current->fds[i].wlen  = 0;
                    u32 pl = (u32) strlen(path);
                    if (pl >= FD_PATH_MAX) pl = FD_PATH_MAX - 1;
                    memcpy(current->fds[i].path, path, pl);
                    current->fds[i].path[pl] = '\0';
                    regs->eax = (u32) i;
                }
                break;
            }
            break;
        }

        case SYS_CLOSE: {
            u32 fd = regs->ebx;
            if (current && fd < MAX_FDS && current->fds[fd].used) {
                process_fd_t* f = &current->fds[fd];
                if (f->wmode && f->wlen > 0) {
                    fat32_write_file(f->path, f->wbuf, f->wlen);
                }
                fat32_close(&f->file);
                f->used  = 0;
                f->wmode = 0;
                f->wlen  = 0;
                regs->eax = 0;
            } else {
                regs->eax = (u32)-1;
            }
            break;
        }

        case SYS_GETPID:
            regs->eax = current ? current->pid : 0;
            break;

        case SYS_BRK:
            regs->eax = 0x08000000;
            break;

        case SYS_UPTIME:
            regs->eax = (u32)(pit_get_ticks() * 10);
            break;

        case SYS_STAT: {
            const char* path = (const char*) regs->ebx;
            if (!path) { regs->eax = STAT_NOENT; break; }
            regs->eax = (u32) stat_type(path);
            break;
        }

        case SYS_SLEEP: {
            u32 ms = regs->ebx;
            if (ms > 0) process_sleep(ms);
            regs->eax = 0;
            break;
        }

        case SYS_READDIR: {
            const char* path = (const char*) regs->ebx;
            char*       buf  = (char*) regs->ecx;
            u32         bsz  = regs->edx;
            if (!path || !buf || bsz == 0) { regs->eax = (u32)-1; break; }

            static fat32_dirent_t entries[64];
            int n = fat32_list(path, entries, 64);
            if (n < 0) { regs->eax = (u32)-1; break; }

            u32 off = 0;
            int count = 0;
            for (int i = 0; i < n; i++) {
                u32 nl = (u32) strlen(entries[i].name);
                if (off + nl + 1 >= bsz) break;
                memcpy(buf + off, entries[i].name, nl);
                off += nl;
                buf[off++] = 0;
                count++;
            }
            regs->eax = (u32) count;
            break;
        }

        case SYS_UNLINK: {
            const char* path = (const char*) regs->ebx;
            if (!path || !fat32_delete(path)) { regs->eax = (u32)-1; break; }
            regs->eax = 0;
            break;
        }

        case SYS_MKDIR: {
            const char* path = (const char*) regs->ebx;
            if (!path || !fat32_mkdir(path)) { regs->eax = (u32)-1; break; }
            regs->eax = 0;
            break;
        }

        case SYS_RMDIR: {
            const char* path = (const char*) regs->ebx;
            if (!path || !fat32_delete(path)) { regs->eax = (u32)-1; break; }
            regs->eax = 0;
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
