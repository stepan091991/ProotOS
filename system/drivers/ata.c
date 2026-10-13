#include "ata.h"
#include "printf.h"
#include "string.h"
#include "panic.h"
#include "pmm.h"
#include "blockdev.h"

/* Порты primary канала */
#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6

#define ATA_REG_DATA        0
#define ATA_REG_ERROR       1
#define ATA_REG_FEATURES    1
#define ATA_REG_SECCOUNT    2
#define ATA_REG_LBA_LOW     3
#define ATA_REG_LBA_MID     4
#define ATA_REG_LBA_HIGH    5
#define ATA_REG_DRIVE       6
#define ATA_REG_STATUS      7
#define ATA_REG_COMMAND     7

#define ATA_SR_BSY   0x80
#define ATA_SR_DRDY  0x40
#define ATA_SR_DF    0x20
#define ATA_SR_DRQ   0x08
#define ATA_SR_ERR   0x01

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30
#define ATA_CMD_FLUSH    0xE7
#define ATA_CMD_IDENTIFY 0xEC

/* ─── Порты ввода-вывода ─────────────────────────────── */

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}
static inline u16 inw(u16 port) {
    u16 v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void ata_io_delay(void) {
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
    inb(ATA_PRIMARY_CTRL);
}

static int ata_wait_not_busy(void) {
    for (u32 i = 0; i < 1000000; i++) {
        u8 status = inb(ATA_PRIMARY_BASE + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return 1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    for (u32 i = 0; i < 1000000; i++) {
        u8 status = inb(ATA_PRIMARY_BASE + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return 0;
        if (status & ATA_SR_DF)  return 0;
        if (status & ATA_SR_DRQ) return 1;
    }
    return 0;
}

static u8 drive_present[2] = { 0, 0 };

int ata_init(void) {
    outb(ATA_PRIMARY_CTRL, 0x04);
    ata_io_delay();
    outb(ATA_PRIMARY_CTRL, 0x00);
    ata_io_delay();

    for (u8 drive = 0; drive < 2; drive++) {
        outb(ATA_PRIMARY_BASE + ATA_REG_DRIVE, (u8)(0xA0 | (drive << 4)));
        ata_io_delay();

        outb(ATA_PRIMARY_BASE + ATA_REG_SECCOUNT, 0);
        outb(ATA_PRIMARY_BASE + ATA_REG_LBA_LOW,  0);
        outb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID,  0);
        outb(ATA_PRIMARY_BASE + ATA_REG_LBA_HIGH, 0);

        outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
        ata_io_delay();

        u8 status = inb(ATA_PRIMARY_BASE + ATA_REG_STATUS);
        if (status == 0) continue;

        if (!ata_wait_not_busy()) continue;

        u8 lba_mid  = inb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID);
        u8 lba_high = inb(ATA_PRIMARY_BASE + ATA_REG_LBA_HIGH);
        if (lba_mid != 0 || lba_high != 0) continue;

        if (!ata_wait_drq()) continue;

        for (int i = 0; i < 256; i++) {
            (void) inw(ATA_PRIMARY_BASE + ATA_REG_DATA);
        }

        drive_present[drive] = 1;
    }

    if (!drive_present[0] && !drive_present[1]) {
        printf("[ata] no drives detected on primary channel\n");
        return 0;
    }

    printf("[ata] primary channel: master=%s slave=%s\n",
           drive_present[0] ? "yes" : "no",
           drive_present[1] ? "yes" : "no");

    ata_register_block_devices();   /* ← ДОБАВИТЬ ЭТУ СТРОКУ */

    return 1;
}

int ata_read_sectors(u8 drive, u32 lba, u8 count, void* buffer) {
    if (drive > 1 || !drive_present[drive]) return 0;
    if (count == 0) return 1;

    if (!ata_wait_not_busy()) return 0;

    u8 drive_head = (u8)(0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_BASE + ATA_REG_DRIVE, drive_head);
    ata_io_delay();

    outb(ATA_PRIMARY_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_LOW,  (u8)(lba & 0xFF));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID,  (u8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_HIGH, (u8)((lba >> 16) & 0xFF));

    outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND, ATA_CMD_READ);
    ata_io_delay();

    u16* out = (u16*) buffer;
    u32 total_words = (u32) count * 256;

    for (u32 i = 0; i < total_words; i++) {
        if (!ata_wait_drq()) return 0;
        out[i] = inw(ATA_PRIMARY_BASE + ATA_REG_DATA);
    }

    return 1;
}

int ata_write_sectors(u8 drive, u32 lba, u8 count, const void* buffer) {
    if (drive > 1 || !drive_present[drive]) return 0;
    if (count == 0) return 1;

    if (!ata_wait_not_busy()) return 0;

    u8 drive_head = (u8)(0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_BASE + ATA_REG_DRIVE, drive_head);
    ata_io_delay();

    outb(ATA_PRIMARY_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_LOW,  (u8)(lba & 0xFF));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID,  (u8)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_HIGH, (u8)((lba >> 16) & 0xFF));

    outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND, ATA_CMD_WRITE);
    ata_io_delay();

    const u16* in = (const u16*) buffer;
    u32 total_words = (u32) count * 256;

    for (u32 i = 0; i < total_words; i++) {
        if (!ata_wait_drq()) return 0;
        outw(ATA_PRIMARY_BASE + ATA_REG_DATA, in[i]);
    }

    outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND, ATA_CMD_FLUSH);
    ata_io_delay();
    ata_wait_not_busy();

    return 1;
}

void ata_dump_info(void) {
    printf("[ata] primary channel\n");
    printf("  master: %s\n", drive_present[0] ? "present" : "absent");
    printf("  slave:  %s\n", drive_present[1] ? "present" : "absent");
}

void ata_test(void) {
    if (!drive_present[0]) {
        printf("[ata] no master drive, test skipped\n");
        return;
    }

    u8* buf = (u8*) pmm_alloc_page();
    if (!buf) {
        printf("[ata] cannot allocate test buffer\n");
        return;
    }

    printf("[ata] reading LBA 0 (MBR)...\n");
    if (!ata_read_sectors(0, 0, 1, buf)) {
        printf("[ata] read failed\n");
        pmm_free_page(buf);
        return;
    }

    printf("  first 16 bytes: ");
    for (int i = 0; i < 16; i++) printf("%02x ", buf[i]);
    printf("\n");

    printf("  signature bytes: 0x%02x 0x%02x ",
           buf[510], buf[511]);
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        printf("(valid MBR)\n");
    } else {
        printf("(no MBR signature)\n");
    }

    pmm_free_page(buf);
}

/* ─── Экспорт ATA как block_device ───────────────────── */

static int ata_bdev_read(block_device_t* dev, u32 lba, u8 count, void* buf) {
    u8 drive = (u8)(u32) dev->private_data;
    return ata_read_sectors(drive, lba, count, buf);
}

static int ata_bdev_write(block_device_t* dev, u32 lba, u8 count, const void* buf) {
    u8 drive = (u8)(u32) dev->private_data;
    return ata_write_sectors(drive, lba, count, buf);
}

static block_device_t ata_master_dev;
static block_device_t ata_slave_dev;

void ata_register_block_devices(void) {
    if (drive_present[0]) {
        strcpy(ata_master_dev.name, "ata0");
        ata_master_dev.sector_size   = 512;
        ata_master_dev.total_sectors = 0x0FFFFFFF;
        ata_master_dev.read          = ata_bdev_read;
        ata_master_dev.write         = ata_bdev_write;
        ata_master_dev.private_data  = (void*) 0;
        bdev_register(&ata_master_dev);
    }
    if (drive_present[1]) {
        strcpy(ata_slave_dev.name, "ata1");
        ata_slave_dev.sector_size   = 512;
        ata_slave_dev.total_sectors = 0x0FFFFFFF;
        ata_slave_dev.read          = ata_bdev_read;
        ata_slave_dev.write         = ata_bdev_write;
        ata_slave_dev.private_data  = (void*) 1;
        bdev_register(&ata_slave_dev);
    }
}
