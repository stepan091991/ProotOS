#include "vmm.h"
#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "printf.h"
#include "panic.h"

#define ENTRIES_PER_TABLE 1024

/* ─── CR3: чтение/запись ─────────────────────────────── */

u32 vmm_get_current_pd(void) {
    u32 cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void vmm_switch_address_space(u32 pd_phys) {
    /* mov в CR3 автоматически сбрасывает TLB. */
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));
}

/* ─── Создание нового адресного пространства ─────────── */

u32 vmm_create_address_space(void) {
    u32 current_pd = vmm_get_current_pd();

    /* Аллоцируем PD (одна страница). */
    u32 new_pd_phys = (u32) pmm_alloc_page();
    if (!new_pd_phys) return 0;

    u32* new_pd = (u32*) new_pd_phys;
    u32* old_pd = (u32*) current_pd;

    /* Обнуляем новый PD. */
    memset(new_pd, 0, PAGE_SIZE);

    /* Копируем kernel-часть (0..VMM_KERNEL_LIMIT).
     * PD-записи указывают на те же PT — общая память. */
    for (u32 i = 0; i < VMM_KERNEL_PD_ENTRIES; i++) {
        new_pd[i] = old_pd[i];
    }

    return new_pd_phys;
}

/* ─── Разбор виртуального адреса ─────────────────────── */

/*
 * Возвращает указатель на PTE для адреса virt.
 * create=1: если PT отсутствует — аллоцирует и подключает.
 * create=0: возвращает NULL, если PT или PD-запись отсутствует.
 */
static u32* vmm_get_pte(u32 virt, int create) {
    u32 pd_idx = virt >> 22;
    u32 pt_idx = (virt >> 12) & 0x3FF;

    u32 pd_phys = vmm_get_current_pd();
    u32* pd = (u32*) pd_phys;

    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        if (!create) return NULL;

        /* Нужно создать PT. */
        u32 pt_phys = (u32) pmm_alloc_page();
        if (!pt_phys) return NULL;

        memset((void*) pt_phys, 0, PAGE_SIZE);

        /* Флаги: PRESENT | RW | USER.
         * USER здесь нужен, потому что user-space страницы должны
         * быть доступны из ring 3. Для kernel-области это ничего
         * не портит (ring 0 и так имеет доступ). */
        pd[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    u32 pt_phys = pd[pd_idx] & PAGE_ADDR_MASK;
    u32* pt = (u32*) pt_phys;

    return &pt[pt_idx];
}

/* ─── Публичный API ──────────────────────────────────── */

void vmm_map(u32 virt, u32 phys, u32 flags) {
    virt &= PAGE_ADDR_MASK;
    phys &= PAGE_ADDR_MASK;

    u32* pte = vmm_get_pte(virt, 1);
    if (!pte) {
        panic("vmm_map: cannot allocate page table");
    }

    *pte = phys | (flags & 0xFFF) | PAGE_PRESENT;

    /* Если ставим USER на PTE, надо установить USER и на PDE.
     * Иначе процессор блокирует доступ: оба уровня должны разрешать. */
    if (flags & PAGE_USER) {
        u32 pd_phys = vmm_get_current_pd();
        u32* pd = (u32*) pd_phys;
        u32 pd_idx = virt >> 22;
        pd[pd_idx] |= PAGE_USER;
    }

    __asm__ volatile ("invlpg (%0)" : : "r"(virt));
}

void vmm_unmap(u32 virt) {
    virt &= PAGE_ADDR_MASK;

    u32* pte = vmm_get_pte(virt, 0);
    if (!pte || !(*pte & PAGE_PRESENT)) return;

    *pte = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt));
}

u32 vmm_get_physical(u32 virt) {
    u32* pte = vmm_get_pte(virt, 0);
    if (!pte || !(*pte & PAGE_PRESENT)) return 0;
    return (*pte & PAGE_ADDR_MASK) | (virt & 0xFFF);
}

int vmm_is_mapped(u32 virt) {
    u32* pte = vmm_get_pte(virt, 0);
    return pte && (*pte & PAGE_PRESENT);
}

/* ─── Диагностика ────────────────────────────────────── */

void vmm_dump(u32 virt) {
    u32 pd_idx = virt >> 22;
    u32 pt_idx = (virt >> 12) & 0x3FF;
    u32 offset = virt & 0xFFF;

    u32 pd_phys = vmm_get_current_pd();
    u32* pd = (u32*) pd_phys;
    u32 pde = pd[pd_idx];

    printf("[vmm] virt = %p\n", (void*) virt);
    printf("  pd_idx=%u pt_idx=%u offset=%u\n", pd_idx, pt_idx, offset);
    printf("  PD[%u] = %p (P=%u R/W=%u U/S=%u)\n",
           pd_idx, (void*) pde,
           (pde >> 0) & 1, (pde >> 1) & 1, (pde >> 2) & 1);

    if (!(pde & PAGE_PRESENT)) {
        printf("  -> NOT MAPPED (PD entry absent)\n");
        return;
    }

    u32* pt = (u32*)(pde & PAGE_ADDR_MASK);
    u32 pte = pt[pt_idx];

    printf("  PT[%u] = %p (P=%u R/W=%u U/S=%u)\n",
           pt_idx, (void*) pte,
           (pte >> 0) & 1, (pte >> 1) & 1, (pte >> 2) & 1);

    if (!(pte & PAGE_PRESENT)) {
        printf("  -> NOT MAPPED (PTE absent)\n");
        return;
    }

    u32 phys = (pte & PAGE_ADDR_MASK) | offset;
    printf("  -> physical = %p\n", (void*) phys);
}

/* ─── Самопроверка ───────────────────────────────────── */

void vmm_test(void) {
    printf("[vmm] starting self-test\n");

    /* Возьмём физическую страницу из PMM. */
    u32 phys = (u32) pmm_alloc_page();
    if (!phys) {
        printf("  FAIL: cannot alloc physical page\n");
        return;
    }
    printf("  allocated physical page at %p\n", (void*) phys);

    /* Заполним её через identity-map (phys == virt для нижних 1 ГБ). */
    u8* direct = (u8*) phys;
    for (u32 i = 0; i < 32; i++) {
        direct[i] = (u8)(0xA0 + i);
    }
    printf("  wrote pattern via identity map\n");

    /* Возьмём заведомо свободный виртуальный адрес в user-части. */
    u32 virt = 0xD0000000;
    printf("  mapping %p -> %p\n", (void*) virt, (void*) phys);

    vmm_map(virt, phys, PAGE_RW);

    /* Читаем через новый virt — должен быть тот же паттерн. */
    u8* via_virt = (u8*) virt;
    int ok = 1;
    for (u32 i = 0; i < 32; i++) {
        if (via_virt[i] != (u8)(0xA0 + i)) {
            ok = 0;
            break;
        }
    }
    printf("  read via new virt: %s\n", ok ? "MATCH" : "MISMATCH");

    /* Пишем через virt, читаем через identity. */
    via_virt[0] = 0xFF;
    printf("  wrote 0xFF via virt, read via phys = 0x%x\n", direct[0]);

    /* Смотрим разрешение адреса. */
    vmm_dump(virt);

    /* Снимаем маппинг. */
    vmm_unmap(virt);
    printf("  unmapped %p\n", (void*) virt);

    /* Физическую страницу можно вернуть PMM. */
    pmm_free_page((void*) phys);

    printf("[vmm] self-test complete\n");
}
