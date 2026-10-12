#ifndef PROOTOS_PROCESS_H
#define PROOTOS_PROCESS_H

#include "types.h"

#define PROCESS_NAME_MAX  32
#define MAX_PROCESSES     64
#define KERNEL_STACK_SIZE 8192
#define SCHED_QUANTUM     5

typedef enum {
    PROC_UNUSED = 0,
    PROC_RUNNING,
    PROC_READY,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct process {
    u32          pid;
    char         name[PROCESS_NAME_MAX];
    proc_state_t state;
    u8*          kstack;
    u32          kstack_top;
    u32          esp;              /* offset 48 — важно! */
    u32          cr3;
    u64          wake_tick;
} process_t;

extern process_t* current;
extern volatile int need_resched;

void process_init(void);

u32  process_create(const char* name, void (*fn)(void*), void* arg);

process_t* process_current(void);

void scheduler_tick(void);      /* вызывается из PIT — ставит флаг */
void schedule(void);             /* вызывается вне прерывания — переключает */
void process_exit(void);
void process_dump(void);

void switch_to(process_t* next);

/* Удобная обёртка для точек, где можно отдать CPU. */
static inline void yield(void) {
    if (need_resched) {
        need_resched = 0;
        schedule();
    }
}

#endif