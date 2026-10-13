#include "shell.h"
#include "kbd_buffer.h"
#include "vga.h"
#include "printf.h"
#include "string.h"
#include "kernel.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "kheap.h"
#include "vmm.h"
#include "paging.h"
#include "ata.h"
#include "blockdev.h"
#include "fat32.h"
#include "banner.h"
#include "process.h"

#define PROMPT        "prootos> "
#define LINE_MAX      128
#define HISTORY_SIZE  16

static int preferred_pager_size = 20;

/* ─── Утилиты строк ─────────────────────────────────── */

static void strncat_unsafe(char* dst, const char* src, u32 max) {
    u32 dl = 0;
    while (dl < max - 1 && dst[dl]) dl++;
    while (dl < max - 1 && *src) dst[dl++] = *src++;
    dst[dl] = '\0';
}

static int str_contains(const char* haystack, const char* needle) {
    if (!*needle) return 1;
    for (const char* h = haystack; *h; h++) {
        const char* hp = h;
        const char* np = needle;
        while (*hp && *np && *hp == *np) { hp++; np++; }
        if (!*np) return 1;
    }
    return 0;
}

static u32 parse_hex(const char* s) {
    u32 v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        u32 d;
        char c = *s;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        v = (v << 4) | d;
        s++;
    }
    return v;
}

static int parse_uint(const char* s, u32* out) {
    u32 v = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (u32)(*s - '0');
        s++;
        any = 1;
    }
    *out = v;
    return any;
}

/* ─── История и текущая директория ──────────────────── */

static char current_dir[FAT32_MAX_PATH] = "/";

static char history[HISTORY_SIZE][LINE_MAX];
static int  history_count = 0;

static void history_add(const char* line) {
    if (*line == '\0') return;
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_SIZE;
        if (strcmp(history[last], line) == 0) return;
    }
    int idx = history_count % HISTORY_SIZE;
    strncpy(history[idx], line, LINE_MAX - 1);
    history[idx][LINE_MAX - 1] = '\0';
    history_count++;
}

static const char* history_get(int logical) {
    if (logical < 0 || logical >= history_count) return NULL;
    return history[logical % HISTORY_SIZE];
}

/* ─── Пути ─────────────────────────────────────────── */

static void normalize_path(const char* input, const char* cwd, char* out) {
    char combined[FAT32_MAX_PATH];

    if (input[0] == '/') {
        strncpy(combined, input, FAT32_MAX_PATH - 1);
        combined[FAT32_MAX_PATH - 1] = '\0';
    } else {
        strncpy(combined, cwd, FAT32_MAX_PATH - 1);
        combined[FAT32_MAX_PATH - 1] = '\0';
        u32 cl = (u32) strlen(combined);
        if (cl > 0 && combined[cl - 1] != '/') {
            strncat_unsafe(combined, "/", FAT32_MAX_PATH);
        }
        strncat_unsafe(combined, input, FAT32_MAX_PATH);
    }

    char work[FAT32_MAX_PATH];
    strncpy(work, combined, FAT32_MAX_PATH - 1);
    work[FAT32_MAX_PATH - 1] = '\0';

    char* parts[64];
    int   nparts = 0;

    char* p = work;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char* start = p;
        while (*p && *p != '/') p++;
        char save = *p;
        *p = '\0';

        if (strcmp(start, ".") == 0) {
            /* skip */
        } else if (strcmp(start, "..") == 0) {
            if (nparts > 0) nparts--;
        } else {
            if (nparts < 64) parts[nparts++] = start;
        }
        if (save == '\0') break;
        p++;
    }

    if (nparts == 0) {
        strcpy(out, "/");
    } else {
        out[0] = '\0';
        for (int i = 0; i < nparts; i++) {
            u32 ol = (u32) strlen(out);
            u32 pl = (u32) strlen(parts[i]);
            if (ol + pl + 2 < FAT32_MAX_PATH) {
                out[ol]     = '/';
                out[ol + 1] = '\0';
                strncat_unsafe(out, parts[i], FAT32_MAX_PATH);
            }
        }
    }
}

static const char* resolve_path(const char* path, char* out) {
    if (path[0] == '/') {
        strncpy(out, path, FAT32_MAX_PATH - 1);
        out[FAT32_MAX_PATH - 1] = '\0';
    } else {
        normalize_path(path, current_dir, out);
    }
    return out;
}

/* ─── Line editor ──────────────────────────────────── */

static void redraw_input(const char* buf, u32 len, u32 pos,
                         u32 start_row, u32 start_col) {
    vga_set_cursor(start_row, start_col);
    for (u32 i = 0; i < len; i++) vga_putchar(buf[i]);
    vga_putchar(' ');
    vga_set_cursor(start_row, start_col + pos);
}

static u32 read_line(char* out, u32 max) {
    u32 len = 0, pos = 0;
    out[0] = '\0';

    u32 start_row, start_col;
    vga_get_cursor(&start_row, &start_col);

    if (start_col >= VGA_WIDTH - 1) {
        vga_putchar('\n');
        vga_get_cursor(&start_row, &start_col);
    }
    u32 screen_max = VGA_WIDTH - start_col - 1;
    if (max > screen_max + 1) max = screen_max + 1;
    if (max < 2) max = 2;

    int  hist_pos     = history_count;
    int  draft_active = 0;
    char draft[LINE_MAX];
    draft[0] = '\0';

    for (;;) {
        int c = kbd_buffer_pop();
        if (c < 0) {
            c = kbd_buffer_pop();
            if (c < 0) {
                yield();
                __asm__ volatile ("hlt");
                continue;
            }
        }

        if (c == '\n') {
            vga_set_cursor(start_row, start_col + len);
            vga_putchar('\n');
            out[len] = '\0';
            history_add(out);
            return len;
        }

        if (c == '\b') {
            if (pos == 0) continue;
            for (u32 i = pos - 1; i + 1 < len; i++) out[i] = out[i + 1];
            len--; pos--;
            out[len] = '\0';
            redraw_input(out, len, pos, start_row, start_col);
            continue;
        }

        if (c == KBD_KEY_DEL) {
            if (pos >= len) continue;
            for (u32 i = pos; i + 1 < len; i++) out[i] = out[i + 1];
            len--;
            out[len] = '\0';
            redraw_input(out, len, pos, start_row, start_col);
            continue;
        }

        if (c == KBD_KEY_LEFT) {
            if (pos > 0) { pos--; vga_set_cursor(start_row, start_col + pos); }
            continue;
        }
        if (c == KBD_KEY_RIGHT) {
            if (pos < len) { pos++; vga_set_cursor(start_row, start_col + pos); }
            continue;
        }
        if (c == KBD_KEY_HOME) {
            pos = 0; vga_set_cursor(start_row, start_col); continue;
        }
        if (c == KBD_KEY_END) {
            pos = len; vga_set_cursor(start_row, start_col + pos); continue;
        }

        if (c == KBD_KEY_UP) {
            if (history_count == 0) continue;
            if (!draft_active) {
                strncpy(draft, out, LINE_MAX - 1);
                draft[LINE_MAX - 1] = '\0';
                draft_active = 1;
            }
            if (hist_pos == 0) continue;
            hist_pos--;
            const char* h = history_get(hist_pos);
            if (!h) continue;
            u32 n = (u32) strlen(h);
            if (n >= max) n = max - 1;
            strncpy(out, h, n);
            out[n] = '\0';
            len = n; pos = n;
            redraw_input(out, len, pos, start_row, start_col);
            continue;
        }

        if (c == KBD_KEY_DOWN) {
            if (!draft_active) continue;
            if (hist_pos < history_count - 1) {
                hist_pos++;
                const char* h = history_get(hist_pos);
                if (!h) continue;
                u32 n = (u32) strlen(h);
                if (n >= max) n = max - 1;
                strncpy(out, h, n);
                out[n] = '\0';
                len = n; pos = n;
                redraw_input(out, len, pos, start_row, start_col);
            } else {
                hist_pos = history_count;
                u32 n = (u32) strlen(draft);
                if (n >= max) n = max - 1;
                strncpy(out, draft, n);
                out[n] = '\0';
                len = n; pos = n;
                redraw_input(out, len, pos, start_row, start_col);
                draft_active = 0;
            }
            continue;
        }

        if (c >= 0x80) continue;
        if ((c < 0x20) && (c != '\t')) continue;

        if (len + 1 < max) {
            for (u32 i = len; i > pos; i--) out[i] = out[i - 1];
            out[pos] = (char) c;
            len++; pos++;
            out[len] = '\0';
            redraw_input(out, len, pos, start_row, start_col);
        }
    }
}

/* ─── Pager ────────────────────────────────────────── */

static void pager_begin(void) { vga_pager_set(preferred_pager_size); }
static void pager_end(void)   { vga_pager_set(0); }

/* ─── Баннер ───────────────────────────────────────── */

static void cmd_banner(void) {
    print_prootos_banner();
    printf("\n ProotOS kernel v0.1.9\n");
    printf(" Booting via Multiboot / GRUB\n\n");
}

/* ─── help ─────────────────────────────────────────── */

static void cmd_help(void) {
    pager_begin();
    printf("Available commands:\n");
    printf("  help                - show this help\n");
    printf("  clear               - clear screen\n");
    printf("  banner              - print ProotOS banner\n");
    printf("  ticks               - show PIT ticks since boot\n");
    printf("  echo TEXT [> FILE]  - print TEXT or write to FILE\n");
    printf("  grep PATTERN FILE   - print matching lines\n");
    printf("  exec FILE.ELF       - load and run an ELF program\n");
    printf("  panic               - trigger a kernel panic (demo)\n");
    printf("  reboot              - reboot via keyboard controller\n");
    printf("  halt                - halt the CPU\n");
    printf("  mem                 - PMM statistics\n");
    printf("  heap                - kernel heap statistics\n");
    printf("  paging              - paging state (CR0/CR3/CR2)\n");
    printf("  ata                 - dump ATA status and read MBR\n");
    printf("  bdev                - list block devices\n");
    printf("  fat32               - walk the filesystem\n");
    printf("  ls [DIR]            - list files\n");
    printf("  cd [DIR]            - change directory\n");
    printf("  pwd                 - print current directory\n");
    printf("  cat FILE            - print file contents\n");
    printf("  touch FILE          - create empty file\n");
    printf("  mkdir DIR           - create directory\n");
    printf("  rm FILE             - delete file\n");
    printf("  write FILE TEXT     - create/overwrite FILE with TEXT\n");
    printf("  pager [N|off]       - set page size, or disable\n");
    printf("  pci                 - list PCI devices\n");
    printf("  --- processes ---\n");
    printf("  ps / threads        - list processes\n");
    printf("  sleep MS            - sleep N milliseconds\n");
    printf("  kill PID            - kill process by pid\n");
    printf("  wait PID            - wait for process to finish\n");
    printf("\nLine editing: arrows, Home, End, Backspace, Delete\n");
    printf("History: Up/Down arrows. Pager: q quits, any key next page\n");
    pager_end();
}

/* ─── Простые команды ──────────────────────────────── */

static void cmd_clear(void)  { vga_clear(); }
static void cmd_ticks(void)  { printf("ticks = %u\n", (u32) pit_get_ticks()); }

static void cmd_reboot(void) {
    u8 status;
    do {
        __asm__ volatile ("inb $0x64, %0" : "=a"(status));
    } while (status & 0x02);
    __asm__ volatile ("outb %0, $0x64" : : "a"((u8) 0xFE));
    halt();
}

static void cmd_pager(const char* args) {
    if (*args == '\0') {
        if (preferred_pager_size == 0) printf("pager: disabled\n");
        else printf("pager: %d lines per page\n", preferred_pager_size);
        return;
    }
    if (strcmp(args, "off") == 0) {
        preferred_pager_size = 0;
        printf("pager: disabled\n");
        return;
    }
    u32 n = 0;
    if (!parse_uint(args, &n) || n == 0) {
        printf("usage: pager [N|off]\n");
        return;
    }
    if (n > 100) n = 100;
    preferred_pager_size = (int) n;
    printf("pager: %u lines per page\n", n);
}

/* ─── Файловые команды ─────────────────────────────── */

static void cmd_ls(const char* args) {
    char fullpath[FAT32_MAX_PATH];
    const char* path;
    if (*args == '\0') path = current_dir;
    else               path = resolve_path(args, fullpath);

    fat32_dirent_t entries[64];
    int n = fat32_list(path, entries, 64);
    pager_begin();
    printf("%s: %d entries\n", path, n);
    for (int i = 0; i < n; i++) {
        printf("  %-14s  %s  %u bytes\n",
               entries[i].name,
               entries[i].is_directory ? "DIR " : "FILE",
               entries[i].size);
    }
    pager_end();
}

static void cmd_cd(const char* args) {
    char fullpath[FAT32_MAX_PATH];
    const char* target;
    if (*args == '\0') target = "/";
    else               target = resolve_path(args, fullpath);

    if (!fat32_is_dir(target)) {
        printf("cd: not a directory: %s\n", args);
        return;
    }
    strncpy(current_dir, target, FAT32_MAX_PATH - 1);
    current_dir[FAT32_MAX_PATH - 1] = '\0';
}

static void cmd_pwd(void) { printf("%s\n", current_dir); }

static void cmd_cat(const char* args) {
    if (*args == '\0') { printf("usage: cat <path>\n"); return; }
    char fullpath[FAT32_MAX_PATH];
    const char* path = resolve_path(args, fullpath);

    fat32_file_t f;
    if (!fat32_open(path, &f)) { printf("cannot open: %s\n", args); return; }

    pager_begin();
    char buf[256];
    u32 n;
    while ((n = fat32_read(&f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    pager_end();
    printf("\n");
    fat32_close(&f);
}

static void cmd_touch(const char* args) {
    if (*args == '\0') { printf("usage: touch <path>\n"); return; }
    char fullpath[FAT32_MAX_PATH];
    const char* path = resolve_path(args, fullpath);
    if (fat32_create(path)) printf("created %s\n", path);
    else                    printf("failed to create %s\n", path);
}

static void cmd_mkdir(const char* args) {
    if (*args == '\0') { printf("usage: mkdir <path>\n"); return; }
    char fullpath[FAT32_MAX_PATH];
    const char* path = resolve_path(args, fullpath);
    if (fat32_mkdir(path)) printf("created directory %s\n", path);
    else                   printf("failed to create %s\n", path);
}

static void cmd_rm(const char* args) {
    if (*args == '\0') { printf("usage: rm <path>\n"); return; }
    char fullpath[FAT32_MAX_PATH];
    const char* path = resolve_path(args, fullpath);
    if (fat32_delete(path)) printf("removed %s\n", path);
    else                    printf("failed to remove %s\n", path);
}

static void cmd_write(const char* args) {
    char path[FAT32_MAX_NAME];
    u32 pi = 0;
    while (*args && *args != ' ' && pi < FAT32_MAX_NAME - 1) path[pi++] = *args++;
    path[pi] = '\0';
    while (*args == ' ') args++;
    if (pi == 0) { printf("usage: write <path> <text>\n"); return; }

    char fullpath[FAT32_MAX_PATH];
    const char* fp = resolve_path(path, fullpath);

    u32 len = (u32) strlen(args);
    if (fat32_write_file(fp, args, len)) printf("wrote %u bytes to %s\n", len, fp);
    else                                 printf("write failed: %s\n", fp);
}

static void cmd_echo(const char* args) {
    const char* gt = NULL;
    for (const char* p = args; *p; p++) {
        if (*p == '>') { gt = p; break; }
    }
    if (!gt) { printf("%s\n", args); return; }

    char text[256];
    u32 ti = 0;
    for (const char* p = args; p < gt && ti < sizeof(text) - 1; p++) text[ti++] = *p;
    while (ti > 0 && text[ti - 1] == ' ') ti--;
    text[ti] = '\0';

    const char* path = gt + 1;
    while (*path == ' ') path++;

    char pathbuf[FAT32_MAX_PATH];
    u32 pi = 0;
    while (*path && pi < FAT32_MAX_PATH - 1) pathbuf[pi++] = *path++;
    while (pi > 0 && pathbuf[pi - 1] == ' ') pi--;
    pathbuf[pi] = '\0';

    if (pi == 0) { printf("echo: missing file path\n"); return; }

    char fullpath[FAT32_MAX_PATH];
    const char* fp = resolve_path(pathbuf, fullpath);

    char content[260];
    u32 cl = 0;
    for (u32 i = 0; i < ti && cl < sizeof(content) - 2; i++) content[cl++] = text[i];
    content[cl++] = '\n';

    if (fat32_write_file(fp, content, cl)) printf("wrote %u bytes to %s\n", cl, fp);
    else                                   printf("write failed: %s\n", fp);
}

static void cmd_grep(const char* args) {
    char pattern[64];
    u32 pi = 0;
    while (*args && *args != ' ' && pi < 63) pattern[pi++] = *args++;
    pattern[pi] = '\0';
    while (*args == ' ') args++;

    if (pi == 0 || *args == '\0') { printf("usage: grep <pattern> <file>\n"); return; }

    char fullpath[FAT32_MAX_PATH];
    const char* fp = resolve_path(args, fullpath);

    fat32_file_t f;
    if (!fat32_open(fp, &f)) { printf("grep: cannot open %s\n", args); return; }

    pager_begin();
    char line[256];
    u32  li = 0;
    int  matched = 0;
    char c;
    u32 n;
    while ((n = fat32_read(&f, &c, 1)) > 0) {
        if (c == '\n') {
            line[li] = '\0';
            if (str_contains(line, pattern)) { printf("%s\n", line); matched++; }
            li = 0;
        } else if (li < sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    if (li > 0) {
        line[li] = '\0';
        if (str_contains(line, pattern)) { printf("%s\n", line); matched++; }
    }
    pager_end();
    fat32_close(&f);
    if (matched == 0) printf("(no matches)\n");
}

/* ─── Команды процессов ───────────────────────────── */

extern volatile u32 g_work1_count;
extern volatile u32 g_work2_count;

static void cmd_ps(void) {
    process_dump();
    printf("[work] work1=%u work2=%u\n",
           g_work1_count, g_work2_count);
}

static void cmd_sleep(const char* args) {
    u32 ms = 0;
    if (!parse_uint(args, &ms)) {
        printf("usage: sleep <ms>\n");
        return;
    }
    u64 t0 = pit_get_ticks();
    process_sleep(ms);
    u64 t1 = pit_get_ticks();
    printf("slept %u ms (actual %u ms)\n",
           ms, (u32)((t1 - t0) * 10));
}

static void cmd_kill(const char* args) {
    u32 pid = 0;
    if (!parse_uint(args, &pid)) {
        printf("usage: kill <pid>\n");
        return;
    }
    if (process_kill(pid) == 0) printf("killed pid %u\n", pid);
    else                        printf("kill: no such pid\n");
}

static void cmd_wait(const char* args) {
    u32 pid = 0;
    if (!parse_uint(args, &pid)) {
        printf("usage: wait <pid>\n");
        return;
    }
    printf("waiting for pid %u...\n", pid);
    if (process_wait(pid) == 0) printf("pid %u finished\n", pid);
    else                        printf("wait: no such pid\n");
}

/* ─── Диспетчер ───────────────────────────────────── */

static void execute(const char* line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    char cmd[64];
    u32  ci = 0;
    while (*line && *line != ' ' && *line != '\t' && ci < 63) cmd[ci++] = *line++;
    cmd[ci] = '\0';
    while (*line == ' ' || *line == '\t') line++;
    const char* args = line;

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        cmd_clear();
    } else if (strcmp(cmd, "banner") == 0) {
        cmd_banner();
    } else if (strcmp(cmd, "ticks") == 0) {
        cmd_ticks();
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "grep") == 0) {
        cmd_grep(args);
    } else if (strcmp(cmd, "pager") == 0) {
        cmd_pager(args);
    } else if (strcmp(cmd, "panic") == 0) {
        panic("panic command from shell");
    } else if (strcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else if (strcmp(cmd, "halt") == 0) {
        printf("halting...\n");
        halt();
    } else if (strcmp(cmd, "mem") == 0) {
        pmm_dump();
    } else if (strcmp(cmd, "heap") == 0) {
        kheap_dump();
    } else if (strcmp(cmd, "paging") == 0) {
        paging_dump();
    } else if (strcmp(cmd, "ata") == 0) {
        ata_dump_info();
        ata_test();
    } else if (strcmp(cmd, "bdev") == 0) {
        bdev_dump();
    } else if (strcmp(cmd, "fat32") == 0) {
        pager_begin();
        fat32_test();
        pager_end();
    } else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(args);
    } else if (strcmp(cmd, "cd") == 0) {
        cmd_cd(args);
    } else if (strcmp(cmd, "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(cmd, "touch") == 0) {
        cmd_touch(args);
    } else if (strcmp(cmd, "mkdir") == 0) {
        cmd_mkdir(args);
    } else if (strcmp(cmd, "rm") == 0) {
        cmd_rm(args);
    } else if (strcmp(cmd, "write") == 0) {
        cmd_write(args);
    } else if (strcmp(cmd, "vmmtest") == 0) {
        vmm_test();
    } else if (strcmp(cmd, "vmm") == 0) {
        if (*args == '\0') printf("usage: vmm <hex-address>\n");
        else              vmm_dump(parse_hex(args));
    } else if (strcmp(cmd, "pci") == 0) {
        pci_dump();
    }else if (strcmp(cmd, "exec") == 0) {
        if (*args == '\0') {
            printf("usage: exec <path-to-elf>\n");
        } else {
            char fullpath[FAT32_MAX_PATH];
            const char* fp = resolve_path(args, fullpath);

            /* Загрузить имя (без пути) как имя процесса. */
            const char* slash = fp;
            for (const char* q = fp; *q; q++) {
                if (*q == '/') slash = q + 1;
            }

            u32 pid = process_create_user(slash, fp);
            if (pid == 0) {
                printf("exec: failed to load %s\n", fp);
            } else {
                printf("exec: started pid %u, waiting...\n", pid);
                process_wait(pid);
                printf("exec: pid %u finished\n", pid);
            }
        }
    } else if (strcmp(cmd, "ps") == 0 || strcmp(cmd, "threads") == 0) {
        cmd_ps();
    } else if (strcmp(cmd, "sleep") == 0) {
        cmd_sleep(args);
    } else if (strcmp(cmd, "kill") == 0) {
        cmd_kill(args);
    } else if (strcmp(cmd, "wait") == 0) {
        cmd_wait(args);
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for a list.\n");
    }
}

void shell_run(void) {
    char line[LINE_MAX];
    printf("\n");
    for (;;) {
        printf("%s%s ", PROMPT, current_dir);
        read_line(line, LINE_MAX);
        execute(line);
    }
}
