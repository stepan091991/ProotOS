#include "pmm.h"
#include "printf.h"
#include "string.h"

#define PMM_MAX_PAGES   (1024 * 1024)

static u8  bitmap[PMM_MAX_PAGES / 8];
static u32 free_pages  = 0;
static u32 total_pages = 0;

static inline void bitmap_set(u32 page)   { bitmap[page >> 3] |=  (u8)(1u << (page & 7)); }
static inline void bitmap_clear(u32 page) { bitmap[page >> 3] &= (u8)~(1u << (page & 7)); }
static inline int  bitmap_test(u32 page)  { return bitmap[page >> 3] & (1u << (page & 7)); }

static void mark_used(u32 page) {
    if (page >= PMM_MAX_PAGES) return;
    if (!bitmap_test(page)) {
        bitmap_set(page);
        if (free_pages > 0) free_pages--;
    }
}

static void mark_free(u32 page) {
    if (page >= PMM_MAX_PAGES) return;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

void pmm_init(multiboot_info_t* mbi) {
    memset(bitmap, 0xFF, sizeof(bitmap));
    free_pages  = 0;
    total_pages = 0;

    if (!(mbi->flags & MULTIBOOT_INFO_MMAP)) {
        printf("[pmm] ERROR: no memory map provided by bootloader!\n");
        return;
    }

    u32 addr = mbi->mmap_addr;
    u32 end  = mbi->mmap_addr + mbi->mmap_length;

    while (addr < end) {
        multiboot_mmap_entry_t* e = (multiboot_mmap_entry_t*) addr;

        if (e->type == MULTIBOOT_MEMORY_AVAILABLE) {
            u64 start = (e->base_addr + PMM_PAGE_SIZE - 1) & ~((u64) PMM_PAGE_SIZE - 1);
            u64 stop  = (e->base_addr + e->length)     & ~((u64) PMM_PAGE_SIZE - 1);

            for (u64 p = start; p < stop; p += PMM_PAGE_SIZE) {
                u32 page = (u32)(p >> PMM_PAGE_SHIFT);
                mark_free(page);
                total_pages++;
            }
        }
        addr += e->size + 4;
    }

    for (u32 p = 0; p < 0x100000; p += PMM_PAGE_SIZE) {
        mark_used(p >> PMM_PAGE_SHIFT);
    }

    extern u8 kernel_end;
    u32 kstart = 0x100000;
    u32 kend   = (u32) &kernel_end;
    kend = (kend + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);

    for (u32 p = kstart; p < kend; p += PMM_PAGE_SIZE) {
        mark_used(p >> PMM_PAGE_SHIFT);
    }

    printf("[pmm] kernel: 0x%p - 0x%p\n", (void*) kstart, (void*) kend);
    printf("[pmm] total pages: %u (%u MB)\n",
           total_pages, (total_pages * PMM_PAGE_SIZE) / (1024 * 1024));
    printf("[pmm] free pages:  %u (%u MB)\n",
           free_pages, (free_pages * PMM_PAGE_SIZE) / (1024 * 1024));
}

void* pmm_alloc_page(void) {
    for (u32 i = 0; i < PMM_MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            mark_used(i);
            return (void*)(i << PMM_PAGE_SHIFT);
        }
    }
    return NULL;
}

void pmm_free_page(void* page) {
    u32 addr = (u32) page;
    if (addr & (PMM_PAGE_SIZE - 1)) {
        printf("[pmm] WARN: free_page unaligned addr %p\n", page);
        return;
    }
    mark_free(addr >> PMM_PAGE_SHIFT);
}

u32 pmm_get_free_pages(void)  { return free_pages; }
u32 pmm_get_total_pages(void) { return total_pages; }

void pmm_dump(void) {
    printf("[pmm] free = %u / %u pages\n", free_pages, total_pages);
}

void pmm_mark_region_used(u32 start, u32 end) {
    start &= ~(PMM_PAGE_SIZE - 1);
    end = (end + PMM_PAGE_SIZE - 1) & ~(PMM_PAGE_SIZE - 1);
    for (u32 p = start; p < end; p += PMM_PAGE_SIZE) {
        mark_used(p >> PMM_PAGE_SHIFT);
    }
}
