#ifndef PROOTOS_PROCESS_H
#define PROOTOS_PROCESS_H

#include "types.h"
#include "fat32.h"

#define MAX_PROCESSES       32
#define PROCESS_NAME_MAX    32
#define KERNEL_STACK_SIZE   4096
#define USER_STACK_SIZE     4096
#define SCHED_QUANTUM       5
#define MAX_FDS             8
#define FD_PATH_MAX         128
#define FD_WBUF_MAX         512

typedef enum {
    PROC_UNUSED = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct process_fd {
    u8           used;
    u8           wmode;               /* 1 = открыт на запись */
    char         path[FD_PATH_MAX];
    fat32_file_t file;
    u8           wbuf[FD_WBUF_MAX];
    u32          wlen;
} process_fd_t;

typedef struct process {
    u32 pid;
    u32 state;
    char name[PROCESS_NAME_MAX];

    u32 kstack;
    u32 kstack_top;
    u32 esp;
    u32 cr3;

    u32 user_entry;
    u32 user_stack;
    u32 is_user;

    u32 exit_code;
    u32 wake_tick;

    process_fd_t fds[MAX_FDS];
} process_t;

extern process_t* current;
extern volatile int need_resched;

process_t* process_current(void);
process_t* process_find(u32 pid);

void process_init(void);

u32 process_create(const char* name, void (*fn)(void*), void* arg);

u32 process_create_user(const char* name, const char* path,
                        int argc, char** argv,
                        const char* stdin_path);

void scheduler_tick(void);
void schedule(void);
void yield(void);
void switch_to(process_t* next);

void process_sleep(u32 ms);
int  process_wait(u32 pid);
int  process_kill(u32 pid);
void process_exit(void);
void process_dump(void);

#endif
