#include "process.h"
#include "pmm.h"
#include "kheap.h"
#include "vmm.h"
#include "paging.h"
#include "elf_loader.h"
#include "tss.h"
#include "printf.h"
#include "string.h"
#include "panic.h"

extern u64 pit_get_ticks(void);
extern void thread_entry(void);
extern void user_entry(void);

static process_t processes[MAX_PROCESSES];
static u32       next_pid     = 1;
static u32       quantum_left = SCHED_QUANTUM;

process_t* current = NULL;
volatile int need_resched = 0;

process_t* process_current(void) { return current; }

process_t* process_find(u32 pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state != PROC_UNUSED && processes[i].pid == pid) {
            return &processes[i];
        }
    }
    return NULL;
}

void process_init(void) {
    memset(processes, 0, sizeof(processes));
    next_pid = 1;
    current  = NULL;
    need_resched = 0;
    quantum_left = SCHED_QUANTUM;
}

static process_t* alloc_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) return &processes[i];
    }
    return NULL;
}

/* Закрыть все fd (кроме 0/1/2). */
static void close_all_fds(process_t* p) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!p->fds[i].used) continue;
        if (p->fds[i].wmode && p->fds[i].wlen > 0) {
            fat32_write_file(p->fds[i].path, p->fds[i].wbuf, p->fds[i].wlen);
        }
        fat32_close(&p->fds[i].file);
        p->fds[i].used  = 0;
        p->fds[i].wmode = 0;
        p->fds[i].wlen  = 0;
    }
}

/* ─── Kernel thread ──────────────────────────────── */

u32 process_create(const char* name, void (*fn)(void*), void* arg) {
    process_t* p = alloc_slot();
    if (!p) { printf("[proc] no free slot\n"); return 0; }

    u8* stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
    if (!stack) { printf("[proc] OOM\n"); return 0; }

    memset(p, 0, sizeof(*p));
    p->pid        = next_pid++;
    p->state      = PROC_READY;
    strncpy(p->name, name, PROCESS_NAME_MAX - 1);
    p->kstack     = (u32) stack;
    p->kstack_top = (u32)(stack + KERNEL_STACK_SIZE);
    p->cr3        = vmm_get_current_pd();
    p->is_user    = 0;

    u32* sp = (u32*) p->kstack_top;
    *--sp = (u32) fn;
    *--sp = (u32) arg;
    *--sp = (u32) thread_entry;
    *--sp = 0;  *--sp = 0;  *--sp = 0;  *--sp = 0;
    *--sp = 0;  *--sp = 0;  *--sp = 0;  *--sp = 0;
    *--sp = 0x10; *--sp = 0x10; *--sp = 0x10; *--sp = 0x10;

    p->esp = (u32) sp;
    printf("[proc] created '%s' pid=%u esp=0x%08x (kernel)\n",
           p->name, p->pid, p->esp);
    return p->pid;
}

/* ─── user-stack с argc/argv ─────────────────────── */

static u32 build_user_stack(u32 ustack_virt, u32 ustack_phys,
                            int argc, char** argv) {
    u32 vp = ustack_virt + USER_STACK_SIZE;
    u32 pp = ustack_phys + USER_STACK_SIZE;
    u32 argv_user[16];
    int i;

    if (argc < 0)  argc = 0;
    if (argc > 16) argc = 16;

    for (i = argc - 1; i >= 0; i--) {
        u32 len = (u32) strlen(argv[i]) + 1;
        vp -= len; pp -= len;
        memcpy((void*) pp, argv[i], len);
        argv_user[i] = vp;
    }
    while (pp & 3) { pp--; vp--; }
    pp -= 4; vp -= 4; *(u32*) pp = 0;
    for (i = argc - 1; i >= 0; i--) {
        pp -= 4; vp -= 4; *(u32*) pp = argv_user[i];
    }
    pp -= 4; vp -= 4; *(u32*) pp = (u32) argc;
    return vp;
                            }

                            /* ─── User-процесс ───────────────────────────────── */

                            u32 process_create_user(const char* name, const char* path,
                                                    int argc, char** argv,
                                                    const char* stdin_path) {
                                process_t* p = alloc_slot();
                                if (!p) { printf("[proc] no free slot\n"); return 0; }

                                u32 old_cr3 = vmm_get_current_pd();
                                u32 new_pd  = vmm_create_address_space();
                                if (!new_pd) { printf("[proc] cannot create AS\n"); return 0; }

                                vmm_switch_address_space(new_pd);

                                elf_entry_t entry = elf_load(path);
                                if (!entry) {
                                    printf("[proc] elf_load failed: %s\n", path);
                                    vmm_switch_address_space(old_cr3);
                                    return 0;
                                }

                                u32 ustack_phys = (u32) pmm_alloc_page();
                                if (!ustack_phys) {
                                    printf("[proc] OOM user stack\n");
                                    vmm_switch_address_space(old_cr3);
                                    return 0;
                                }
                                memset((void*) ustack_phys, 0, USER_STACK_SIZE);

                                u32 ustack_virt = 0xB0000000;
                                vmm_map(ustack_virt, ustack_phys, PAGE_RW | PAGE_USER);

                                u32 user_esp = build_user_stack(ustack_virt, ustack_phys, argc, argv);

                                vmm_switch_address_space(old_cr3);

                                u8* kstack = (u8*) kmalloc(KERNEL_STACK_SIZE);
                                if (!kstack) { printf("[proc] OOM kstack\n"); return 0; }

                                memset(p, 0, sizeof(*p));
                                p->pid        = next_pid++;
                                p->state      = PROC_READY;
                                strncpy(p->name, name, PROCESS_NAME_MAX - 1);
                                p->kstack     = (u32) kstack;
                                p->kstack_top = (u32)(kstack + KERNEL_STACK_SIZE);
                                p->cr3        = new_pd;
                                p->user_entry = (u32) entry;
                                p->user_stack = user_esp;
                                p->is_user    = 1;

                                /* fd 0 = redirect, если задан. */
                                if (stdin_path) {
                                    if (fat32_open(stdin_path, &p->fds[0].file)) {
                                        p->fds[0].used = 1;
                                        strncpy(p->fds[0].path, stdin_path, FD_PATH_MAX - 1);
                                    } else {
                                        printf("[proc] cannot open stdin: %s\n", stdin_path);
                                    }
                                }

                                u32* sp = (u32*) p->kstack_top;
                                *--sp = 0x23;
                                *--sp = p->user_stack;
                                *--sp = 0x202;
                                *--sp = 0x1B;
                                *--sp = p->user_entry;
                                *--sp = (u32) user_entry;
                                *--sp = 0;  *--sp = 0;  *--sp = 0;  *--sp = 0;
                                *--sp = 0;  *--sp = 0;  *--sp = 0;  *--sp = 0;
                                *--sp = 0x10; *--sp = 0x10; *--sp = 0x10; *--sp = 0x10;

                                p->esp = (u32) sp;

                                printf("[proc] created '%s' pid=%u esp=0x%08x (user, entry=0x%08x)\n",
                                       p->name, p->pid, p->esp, p->user_entry);
                                return p->pid;
                                                    }

                                                    /* ─── Планировщик ────────────────────────────────── */

                                                    static process_t* pick_next(void) {
                                                        int start = 0;
                                                        if (current) {
                                                            for (int i = 0; i < MAX_PROCESSES; i++) {
                                                                if (&processes[i] == current) { start = i; break; }
                                                            }
                                                        }
                                                        for (int i = 1; i <= MAX_PROCESSES; i++) {
                                                            int idx = (start + i) % MAX_PROCESSES;
                                                            if (processes[idx].state == PROC_READY) return &processes[idx];
                                                        }
                                                        return NULL;
                                                    }

                                                    void scheduler_tick(void) {
                                                        u64 now = pit_get_ticks();
                                                        for (int i = 0; i < MAX_PROCESSES; i++) {
                                                            if (processes[i].state == PROC_SLEEPING &&
                                                                processes[i].wake_tick <= now) {
                                                                processes[i].state = PROC_READY;
                                                            processes[i].wake_tick = 0;
                                                                }
                                                        }
                                                        if (!current) return;
                                                        if (quantum_left > 0) { quantum_left--; return; }
                                                        quantum_left = SCHED_QUANTUM;
                                                        need_resched = 1;
                                                    }

                                                    void schedule(void) {
                                                        __asm__ volatile ("cli");
                                                        process_t* next = pick_next();
                                                        if (!next) { __asm__ volatile ("sti"); return; }
                                                        next->state = PROC_RUNNING;
                                                        if (current && current->state == PROC_RUNNING) current->state = PROC_READY;
                                                        tss_set_kernel_stack(next->kstack_top);
                                                        switch_to(next);
                                                        __asm__ volatile ("sti");
                                                    }

                                                    void yield(void) {
                                                        if (current && current->state == PROC_RUNNING) {
                                                            current->state = PROC_READY;
                                                        }
                                                        schedule();
                                                    }

                                                    /* ─── Примитивы ──────────────────────────────────── */

                                                    void process_sleep(u32 ms) {
                                                        process_t* self = current;
                                                        if (!self) return;
                                                        if (ms == 0) { yield(); return; }
                                                        u64 ticks_needed = (ms + 9) / 10;
                                                        if (ticks_needed == 0) ticks_needed = 1;
                                                        self->wake_tick = (u32)(pit_get_ticks() + ticks_needed);
                                                        self->state     = PROC_SLEEPING;
                                                        schedule();
                                                    }

                                                    int process_wait(u32 pid) {
                                                        if (pid == 0) { printf("wait: usage\n"); return -1; }
                                                        while (1) {
                                                            process_t* p = process_find(pid);
                                                            if (!p) return -1;
                                                            if (p == current) { printf("wait: cannot wait for self\n"); return -1; }

                                                            if (p->state == PROC_ZOMBIE) {
                                                                close_all_fds(p);
                                                                if (p->kstack) { kfree((void*) p->kstack); p->kstack = 0; }
                                                                p->state = PROC_UNUSED;
                                                                p->pid   = 0;
                                                                p->name[0] = '\0';
                                                                return 0;
                                                            }
                                                            process_sleep(10);
                                                        }
                                                    }

                                                    int process_kill(u32 pid) {
                                                        process_t* p = process_find(pid);
                                                        if (!p) return -1;
                                                        if (p->state == PROC_ZOMBIE || p->state == PROC_UNUSED) return -1;
                                                        p->state = PROC_ZOMBIE;
                                                        if (p == current) {
                                                            for (;;) { schedule(); __asm__ volatile ("hlt"); }
                                                        }
                                                        return 0;
                                                    }

                                                    void process_exit(void) {
                                                        if (!current) panic("process_exit without current");
                                                        printf("[proc] pid=%u '%s' exiting\n", current->pid, current->name);
                                                        current->state = PROC_ZOMBIE;
                                                        for (;;) { schedule(); __asm__ volatile ("hlt"); }
                                                    }

                                                    void process_dump(void) {
                                                        printf("[proc] PID  STATE     NAME\n");
                                                        for (int i = 0; i < MAX_PROCESSES; i++) {
                                                            if (processes[i].state == PROC_UNUSED) continue;
                                                            const char* st = "?";
                                                            switch (processes[i].state) {
                                                                case PROC_RUNNING:  st = "RUNNING";  break;
                                                                case PROC_READY:    st = "READY";    break;
                                                                case PROC_SLEEPING: st = "SLEEPING"; break;
                                                                case PROC_ZOMBIE:   st = "ZOMBIE";   break;
                                                            }
                                                            printf("       %-4u %-9s %s%s\n",
                                                                   processes[i].pid, st, processes[i].name,
                                                                   processes[i].is_user ? "  [user]" : "");
                                                        }
                                                    }
