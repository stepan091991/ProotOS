// system/drivers/usb/osal_port.c
//
// OSAL (Operating System Abstraction Layer) для CherryUSB v1.6.1
// поверх примитивов ProotOS: кооперативные kernel threads,
// PIT-тики (100 Гц), kmalloc/kfree.

#include "usb_osal.h"
#include "usb_errno.h"
#include "usb_config.h"
#include "usb_log.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────
// Функции ядра ProotOS.
// Forward-декларации, чтобы не тянуть system/include/*.h
// (там typedef u32 uintptr_t / size_t, конфликтует с stdint.h).
// ─────────────────────────────────────────────────────────
extern void      *kmalloc(size_t size);
extern void       kfree(void *ptr);
extern uint32_t   process_create(const char *name,
                                 void (*fn)(void *),
                                 void *arg);
extern void       process_sleep(uint32_t ms);
extern void       process_exit(void);
extern int        process_kill(uint32_t pid);
extern uint64_t   pit_get_ticks(void);
extern void       yield(void);

// PIT = 100 Гц → 1 тик ≈ 10 мс
#define OSAL_MS_TO_TICKS(ms)  (((ms) + 9u) / 10u)

// Fallback, если usb_osal.h не определяет
#ifndef USB_OSAL_WAITING_FOREVER
#define USB_OSAL_WAITING_FOREVER  0xFFFFFFFFu
#endif

// ═══════════════════════════════════════════════════════════
//                          Потоки
// ═══════════════════════════════════════════════════════════

usb_osal_thread_t usb_osal_thread_create(const char *name,
                                         uint32_t stack_size,
                                         uint32_t prio,
                                         usb_thread_entry_t entry,
                                         void *args)
{
    (void)stack_size;   // стек фиксирован (KERNEL_STACK_SIZE)
    (void)prio;         // round-robin, приоритетов нет

    uint32_t pid = process_create(name, entry, args);
    if (pid == 0) {
        USB_LOG_ERR("osal: process_create('%s') failed\r\n", name);
        return NULL;
    }
    return (usb_osal_thread_t)(uintptr_t)pid;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    if (thread == NULL) {
        // Самоудаление: помечаемся ZOMBIE, уходим в планировщик.
        // process_exit не возвращается.
        process_exit();
        return;
    }
    process_kill((uint32_t)(uintptr_t)thread);
}

void usb_osal_thread_schedule_other(void)
{
    yield();
}

// ═══════════════════════════════════════════════════════════
//                         Семафоры
// ═══════════════════════════════════════════════════════════

typedef struct {
    volatile int32_t count;
    volatile int32_t max;
} osal_sem_t;

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    osal_sem_t *s = (osal_sem_t *)kmalloc(sizeof(osal_sem_t));
    if (!s) {
        USB_LOG_ERR("osal: sem create OOM\r\n");
        return NULL;
    }
    s->count = (int32_t)initial_count;
    s->max   = 0x7FFFFFFF;
    return (usb_osal_sem_t)s;
}

usb_osal_sem_t usb_osal_sem_create_counting(uint32_t max_count)
{
    osal_sem_t *s = (osal_sem_t *)kmalloc(sizeof(osal_sem_t));
    if (!s) {
        USB_LOG_ERR("osal: counting sem create OOM\r\n");
        return NULL;
    }
    s->count = 0;
    s->max   = (int32_t)max_count;
    return (usb_osal_sem_t)s;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    if (sem) kfree(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    osal_sem_t *s = (osal_sem_t *)sem;
    if (!s) return -USB_ERR_INVAL;

    if (s->count > 0) {
        // Быстрый путь — семафор доступен сразу.
        s->count--;
        return 0;
    }

    // Диагностика: печатаем только первый раз, чтобы видеть,
    // что hub-нить ушла в ожидание.
    static int printed_wait = 0;
    if (!printed_wait) {
        printf("[osal] sem_take waiting...\n");
        printed_wait = 1;
    }

    if (timeout == USB_OSAL_WAITING_FOREVER) {
        while (s->count <= 0) {
            process_sleep(1);
        }
        s->count--;
        return 0;
    }

    uint64_t start         = pit_get_ticks();
    uint64_t timeout_ticks = OSAL_MS_TO_TICKS(timeout);
    while (s->count <= 0) {
        if (pit_get_ticks() - start >= timeout_ticks) {
            return -USB_ERR_TIMEOUT;
        }
        process_sleep(1);
    }
    s->count--;
    return 0;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    osal_sem_t *s = (osal_sem_t *)sem;
    if (!s) return -USB_ERR_INVAL;
    if (s->count < s->max) s->count++;
    return 0;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
    osal_sem_t *s = (osal_sem_t *)sem;
    if (s) s->count = 0;
}

// ═══════════════════════════════════════════════════════════
//                         Мьютексы
// ═══════════════════════════════════════════════════════════

typedef struct {
    volatile int32_t locked;
} osal_mutex_t;

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    osal_mutex_t *m = (osal_mutex_t *)kmalloc(sizeof(osal_mutex_t));
    if (!m) {
        USB_LOG_ERR("osal: mutex create OOM\r\n");
        return NULL;
    }
    m->locked = 0;
    return (usb_osal_mutex_t)m;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    if (mutex) kfree(mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    osal_mutex_t *m = (osal_mutex_t *)mutex;
    if (!m) return -USB_ERR_INVAL;
    while (__sync_lock_test_and_set(&m->locked, 1)) {
        yield();
    }
    return 0;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    osal_mutex_t *m = (osal_mutex_t *)mutex;
    if (!m) return -USB_ERR_INVAL;
    __sync_lock_release(&m->locked);
    return 0;
}

// ═══════════════════════════════════════════════════════════
//                     Очереди сообщений
// ═══════════════════════════════════════════════════════════

#define OSAL_MQ_MAX_MSGS  32

typedef struct {
    uintptr_t          buffer[OSAL_MQ_MAX_MSGS];
    volatile uint32_t  head;
    volatile uint32_t  tail;
    volatile uint32_t  count;
    uint32_t           capacity;
} osal_mq_t;

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    if (max_msgs == 0)               max_msgs = 1;
    if (max_msgs > OSAL_MQ_MAX_MSGS) max_msgs = OSAL_MQ_MAX_MSGS;

    osal_mq_t *q = (osal_mq_t *)kmalloc(sizeof(osal_mq_t));
    if (!q) {
        USB_LOG_ERR("osal: mq create OOM\r\n");
        return NULL;
    }
    memset(q, 0, sizeof(*q));
    q->capacity = max_msgs;
    return (usb_osal_mq_t)q;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    if (mq) kfree(mq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    osal_mq_t *q = (osal_mq_t *)mq;
    if (!q) return -USB_ERR_INVAL;
    if (q->count >= q->capacity) return -USB_ERR_TIMEOUT;
    q->buffer[q->tail] = addr;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return 0;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    osal_mq_t *q = (osal_mq_t *)mq;
    if (!q || !addr) return -USB_ERR_INVAL;

    if (timeout == USB_OSAL_WAITING_FOREVER) {
        while (q->count == 0) {
            process_sleep(1);
        }
    } else {
        uint64_t start         = pit_get_ticks();
        uint64_t timeout_ticks = OSAL_MS_TO_TICKS(timeout);
        while (q->count == 0) {
            if (pit_get_ticks() - start >= timeout_ticks) {
                return -USB_ERR_TIMEOUT;
            }
            process_sleep(1);
        }
    }
    *addr = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return 0;
}

// ═══════════════════════════════════════════════════════════
//                     Программные таймеры
//
// Фоновый kernel thread просыпается раз в тик (10 мс) и
// обходит связный список таймеров. CherryUSB использует
// таймеры только для длинных интервалов (>=300 мс).
// ═══════════════════════════════════════════════════════════

typedef struct osal_timer_node {
    struct usb_osal_timer   pub;        // обязательно первым полем!
    volatile int            active;
    uint32_t                interval_ticks;
    uint64_t                next_fire;
    uint8_t                 is_period_v;
    struct osal_timer_node *next;
} osal_timer_node_t;

static osal_timer_node_t *g_timer_list           = NULL;
static volatile int       g_timer_thread_started = 0;

static void osal_timer_thread(void *arg)
{
    (void)arg;
    for (;;) {
        uint64_t now = pit_get_ticks();
        osal_timer_node_t *t = g_timer_list;
        while (t) {
            if (t->active && now >= t->next_fire) {
                if (t->pub.handler) {
                    t->pub.handler(t->pub.argument);
                }
                if (t->is_period_v) {
                    t->next_fire = now + t->interval_ticks;
                } else {
                    t->active = 0;
                }
            }
            t = t->next;
        }
        process_sleep(1);
    }
}

static void timer_thread_ensure(void)
{
    if (g_timer_thread_started) return;
    g_timer_thread_started = 1;
    usb_osal_thread_create("usb_tim", 2048, 0, osal_timer_thread, NULL);
}

struct usb_osal_timer *usb_osal_timer_create(const char *name,
                                             uint32_t timeout_ms,
                                             usb_timer_handler_t handler,
                                             void *argument,
                                             bool is_period)
{
    (void)name;
    timer_thread_ensure();

    osal_timer_node_t *t = (osal_timer_node_t *)kmalloc(sizeof(osal_timer_node_t));
    if (!t) {
        USB_LOG_ERR("osal: timer create OOM\r\n");
        return NULL;
    }
    memset(t, 0, sizeof(*t));
    t->pub.handler     = handler;
    t->pub.argument    = argument;
    t->pub.timer       = t;               // ID = указатель на себя
    t->pub.timeout_ms  = timeout_ms;
    t->pub.is_period   = is_period;
    t->interval_ticks  = OSAL_MS_TO_TICKS(timeout_ms);
    if (t->interval_ticks == 0) t->interval_ticks = 1;
    t->is_period_v     = is_period ? 1 : 0;
    t->active          = 0;

    t->next      = g_timer_list;
    g_timer_list = t;

    return &t->pub;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    if (!timer) return;
    osal_timer_node_t *target = (osal_timer_node_t *)timer;

    osal_timer_node_t **pp = &g_timer_list;
    while (*pp) {
        if (*pp == target) {
            *pp = target->next;
            break;
        }
        pp = &(*pp)->next;
    }
    kfree(target);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    if (!timer) return;
    osal_timer_node_t *t = (osal_timer_node_t *)timer;
    t->next_fire = pit_get_ticks() + t->interval_ticks;
    t->active    = 1;
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    if (!timer) return;
    osal_timer_node_t *t = (osal_timer_node_t *)timer;
    t->active = 0;
}

// ═══════════════════════════════════════════════════════════
//                  Критические секции
// ═══════════════════════════════════════════════════════════

size_t usb_osal_enter_critical_section(void)
{
    size_t flags;
    __asm__ volatile ("pushfl; popl %0; cli"
    : "=r"(flags)
    :
    : "memory");
    return flags;
}

void usb_osal_leave_critical_section(size_t flag)
{
    __asm__ volatile ("pushl %0; popfl"
    :
    : "r"(flag)
    : "memory", "cc");
}

// ═══════════════════════════════════════════════════════════
//                      Сон и память
// ═══════════════════════════════════════════════════════════

void usb_osal_msleep(uint32_t delay)
{
    process_sleep(delay);
}

void *usb_osal_malloc(size_t size)
{
    return kmalloc(size);
}

void usb_osal_free(void *ptr)
{
    kfree(ptr);
}
