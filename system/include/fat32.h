#ifndef PROOTOS_FAT32_H
#define PROOTOS_FAT32_H

#include "types.h"
#include "blockdev.h"

#define FAT32_MAX_NAME 64
#define FAT32_MAX_PATH 256

typedef struct {
    u32 bytes_per_sector;
    u32 sectors_per_cluster;
    u32 reserved_sectors;
    u32 num_fats;
    u32 sectors_per_fat;
    u32 root_cluster;
    u32 total_sectors;
    u32 fat_start_lba;
    u32 data_start_lba;
    u32 cluster_size;
    u32 total_clusters;
} fat32_info_t;

typedef struct {
    u32 start_cluster;
    u32 size;
    u32 current_cluster;
    u32 current_offset;
    u32 position;
    int is_open;
} fat32_file_t;

typedef struct {
    char name[FAT32_MAX_NAME];
    u32  size;
    u32  first_cluster;
    u8   attributes;
    int  is_directory;
} fat32_dirent_t;

/* ─── Read API ──────────────────────────────────────── */
int  fat32_mount(block_device_t* dev);
int  fat32_open(const char* path, fat32_file_t* file);
u32  fat32_read(fat32_file_t* file, void* buffer, u32 size);
void fat32_close(fat32_file_t* file);
int  fat32_list(const char* path, fat32_dirent_t* out, int max_entries);
int  fat32_is_dir(const char* path);
void fat32_dump_info(void);
void fat32_test(void);

/* ─── Write API ─────────────────────────────────────── */
/* Создать пустой файл. 1 = успех. */
int  fat32_create(const char* path);

/* Удалить файл или пустую директорию. */
int  fat32_delete(const char* path);

/* Создать директорию. */
int  fat32_mkdir(const char* path);

/* Записать файл целиком (создаёт или перезаписывает). */
int  fat32_write_file(const char* path, const void* data, u32 size);

int  fat32_seek(fat32_file_t* file, u32 offset);

#endif
