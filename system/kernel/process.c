#include "process.h"
#include "pmm.h"
#include "kheap.h"
#include "printf.h"
#include "string.h"
#include "panic.h"

#define OFFSETOF(t, f) ((u32)&((t*)0)->f)
typedef char proc_esp_offset_check[(OFFSETOF(process_t, esp) == 48) ? 1 : -1];

static process_t processes[MAX_PROCESSES];
static u32       next_pid     = 1;
static u32       quantum_left = SCHED_QUANTUM;

process_t* current = NULL;
volatile int need_resched = 0;

process_t* process_current(void) { return current; }

/* Точка входа нового потока — в switch.S. */
extern void thread_entry(void);

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

u32 process_create(const char* name, void (*fn)(void*), void* arg) {
    process_t* p = alloc_slot();
    if (!p) { printf("[proc] no free slot\n"); return 0; }

    u8* stack = (u8*) kmalloc(KERNEL_STACK_SIZE);
    if (!stack) { printf("[proc] OOM\n"); return 0; }

    memset(p, 0, sizeof(*p));
    p->pid        = next_pid++;
    p->state      = PROC_READY;
    strncpy(p->name, name, PROCESS_NAME_MAX - 1);
    p->kstack     = stack;
    p->kstack_top = (u32)(stack + KERNEL_STACK_SIZE);
    p->cr3        = 0;

    u32* sp = (u32*) p->kstack_top;

    /* Порядок на стеке (снизу вверх):
     *   [esp]        gs
     *                fs
     *                es
     *                ds
     *                edi..eax (pusha)
     *                eip = thread_entry     ← ret снимает
     *                arg                    ← thread_entry: pop eax
     *                fn                     ← thread_entry: pop ebx
     */

    /* 1. fn и arg — сначала fn (глубже), потом arg (на вершине). */
    *--sp = (u32) fn;
    *--sp = (u32) arg;
    *--sp = (u32) thread_entry;

    /* 2. pusha: порядок сохранения eax,ecx,edx,ebx,esp,ebp,esi,edi.
     *    При popa порядок обратный — edi первым.
     *    На стеке (сверху вниз) должен быть edi. */
    *--sp = 0;   /* edi */
    *--sp = 0;   /* esi */
    *--sp = 0;   /* ebp */
    *--sp = 0;   /* esp_dummy */
    *--sp = 0;   /* ebx */
    *--sp = 0;   /* edx */
    *--sp = 0;   /* ecx */
    *--sp = 0;   /* eax */

    /* 3. Сегменты: сверху gs, ниже fs, es, ds. */
    *--sp = 0x10;   /* gs */
    *--sp = 0x10;   /* fs */
    *--sp = 0x10;   /* es */
    *--sp = 0x10;   /* ds */

    p->esp = (u32) sp;

    printf("[proc] created '%s' pid=%u esp=0x%08x\n",
           p->name, p->pid, p->esp);
    return p->pid;
}

void process_exit(void) {
    if (!current) panic("process_exit without current");

    printf("[proc] pid=%u '%s' exiting\n", current->pid, current->name);
    current->state = PROC_ZOMBIE;

    for (;;) {
        schedule();
        __asm__ volatile ("hlt");
    }
}

void process_dump(void) {
    printf("[proc] PID  STATE     NAME\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].state == PROC_UNUSED) continue;
        const char* st = "?";
        switch (processes[i].state) {
            case PROC_RUNNING: st = "RUNNING"; break;
            case PROC_READY:   st = "READY";   break;
            case PROC_ZOMBIE:  st = "ZOMBIE";  break;
            default: break;
        }
        printf("       %-4u %-9s %s\n",
               processes[i].pid, st, processes[i].name);
    }
}

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
    if (!current) return;
    if (quantum_left > 0) { quantum_left--; return; }
    quantum_left = SCHED_QUANTUM;
    need_resched = 1;
}

void schedule(void) {
    __asm__ volatile ("cli");

    process_t* next = pick_next();
    if (!next) {
        __asm__ volatile ("sti");
        return;
    }

    next->state = PROC_RUNNING;
    if (current && current->state == PROC_RUNNING) current->state = PROC_READY;

    switch_to(next);
    /* switch_to уводит на новый стек.
     * Если сюда попали — вернулись в этот же контекст. */
    __asm__ volatile ("sti");
}