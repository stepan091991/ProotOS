#ifndef PROOTOS_PROCESS_H
#define PROOTOS_PROCESS_H

#include "types.h"

#define PROCESS_NAME_MAX  32
#define MAX_PROCESSES     64
#define KERNEL_STACK_SIZE 8192
#define USER_STACK_SIZE   4096
#define SCHED_QUANTUM     5

typedef enum {
    PROC_UNUSED = 0,
    PROC_RUNNING,
    PROC_READY,
    PROC_SLEEPING,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct process {
    u32          pid;                    /* 0 */
    char         name[PROCESS_NAME_MAX]; /* 4..35 */
    proc_state_t state;                  /* 36 */
    u32          kstack;                 /* 40 */
    u32          kstack_top;             /* 44 */
    u32          esp;                    /* 48 — offset важен! */
    u32          cr3;                    /* 52 */
    u32          user_entry;             /* 56 */
    u32          user_stack;             /* 60 */
    u32          wake_tick;              /* 64 */
    u32          is_user;                /* 68 */
    u32          exit_code;              /* 72 */
} process_t;

extern process_t* current;
extern volatile int need_resched;

void process_init(void);

/* Kernel thread. */
u32  process_create(const char* name, void (*fn)(void*), void* arg);

/* User-процесс. Загружает ELF в своё адресное пространство.
 * Возвращает pid или 0. */
u32  process_create_user(const char* name, const char* path);

process_t* process_current(void);
process_t* process_find(u32 pid);

void scheduler_tick(void);
void schedule(void);

void process_sleep(u32 ms);
int  process_wait(u32 pid);
int  process_kill(u32 pid);
void process_exit(void);
void process_dump(void);

void switch_to(process_t* next);

static inline void yield(void) {
    if (need_resched) {
        need_resched = 0;
        schedule();
    }
}

#endif