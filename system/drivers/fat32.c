#include "fat32.h"
#include "printf.h"
#include "string.h"
#include "pmm.h"

#define FAT32_EOC_MIN    0x0FFFFFF8
#define FAT32_ATTR_LFN   0x0F
#define FAT32_ATTR_DIR   0x10
#define FAT32_ATTR_LABEL 0x08
#define FAT32_ATTR_ARCH  0x20

static fat32_info_t     fs;
static int              fs_mounted = 0;
static block_device_t*  fs_dev     = NULL;
static u8               boot_sector[512];

/* ═══════════════════════════════════════════════════════
 *  УТИЛИТЫ
 * ═══════════════════════════════════════════════════════ */

static u16 rd16(const u8* p) {
    return (u16) p[0] | ((u16) p[1] << 8);
}
static u32 rd32(const u8* p) {
    return (u32) p[0] | ((u32) p[1] << 8) |
    ((u32) p[2] << 16) | ((u32) p[3] << 24);
}
static void wr16(u8* p, u16 v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}
static void wr32(u8* p, u32 v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static int read_sector(u32 lba, void* buf) {
    return bdev_read(fs_dev, lba, 1, buf);
}
static int write_sector(u32 lba, const void* buf) {
    return bdev_write(fs_dev, lba, 1, buf);
}

static u32 cluster_to_lba(u32 cluster) {
    return fs.data_start_lba + (cluster - 2) * fs.sectors_per_cluster;
}
static int read_cluster(u32 cluster, void* buf) {
    return bdev_read(fs_dev, cluster_to_lba(cluster),
                     (u8) fs.sectors_per_cluster, buf);
}
static int write_cluster(u32 cluster, const void* buf) {
    return bdev_write(fs_dev, cluster_to_lba(cluster),
                      (u8) fs.sectors_per_cluster, buf);
}

/* ─── FAT ──────────────────────────────────────────── */

static u32 fat_get(u32 cluster) {
    u32 fat_offset    = cluster * 4;
    u32 sector_in_fat = fat_offset / fs.bytes_per_sector;
    u32 offset_in_sec = fat_offset % fs.bytes_per_sector;

    u8 buf[512];
    if (!read_sector(fs.fat_start_lba + sector_in_fat, buf)) return 0;

    return rd32(&buf[offset_in_sec]) & 0x0FFFFFFF;
}

/* Записать FAT-запись в ОБЕ копии FAT. */
static int fat_set(u32 cluster, u32 value) {
    u32 fat_offset    = cluster * 4;
    u32 sector_in_fat = fat_offset / fs.bytes_per_sector;
    u32 offset_in_sec = fat_offset % fs.bytes_per_sector;

    u8 buf[512];
    for (u32 fat = 0; fat < fs.num_fats; fat++) {
        u32 lba = fs.fat_start_lba + fat * fs.sectors_per_fat + sector_in_fat;
        if (!read_sector(lba, buf)) return 0;

        u32 old = rd32(&buf[offset_in_sec]);
        u32 nw  = (old & 0xF0000000) | (value & 0x0FFFFFFF);
        wr32(&buf[offset_in_sec], nw);

        if (!write_sector(lba, buf)) return 0;
    }
    return 1;
}

static u32 next_cluster(u32 cluster) {
    u32 n = fat_get(cluster);
    if (n >= FAT32_EOC_MIN) return 0;
    if (n < 2) return 0;
    return n;
}

/* Аллоцировать один кластер: найти свободный, пометить EOC. */
static u32 alloc_cluster(void) {
    for (u32 c = 2; c < fs.total_clusters + 2; c++) {
        if (fat_get(c) == 0) {
            if (!fat_set(c, 0x0FFFFFFF)) return 0;
            return c;
        }
    }
    return 0;
}

/* Освободить всю цепочку кластеров. */
static void free_chain(u32 start) {
    u32 c = start;
    while (c >= 2 && c < FAT32_EOC_MIN) {
        u32 n = fat_get(c);
        fat_set(c, 0);
        if (n >= FAT32_EOC_MIN || n < 2) break;
        c = n;
    }
}

/* ─── Имена ─────────────────────────────────────────── */

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

static void format_name(const u8* raw, char* out) {
    int oi = 0;
    for (int i = 0; i < 8 && oi < FAT32_MAX_NAME - 1; i++) {
        if (raw[i] == ' ') break;
        out[oi++] = (char) raw[i];
    }
    int has_ext = 0;
    for (int i = 8; i < 11; i++) {
        if (raw[i] != ' ') { has_ext = 1; break; }
    }
    if (has_ext && oi < FAT32_MAX_NAME - 1) {
        out[oi++] = '.';
        for (int i = 8; i < 11 && oi < FAT32_MAX_NAME - 1; i++) {
            if (raw[i] == ' ') break;
            out[oi++] = (char) raw[i];
        }
    }
    out[oi] = '\0';
}

static int name_matches(const char* human, const u8* raw) {
    char formatted[FAT32_MAX_NAME];
    format_name(raw, formatted);
    int i = 0;
    while (human[i] && formatted[i]) {
        if (to_upper(human[i]) != to_upper(formatted[i])) return 0;
        i++;
    }
    return human[i] == '\0' && formatted[i] == '\0';
}

/* "hello.txt" → "HELLO   TXT" (11 байт). */
static int encode_short_name(const char* human, u8* out) {
    memset(out, ' ', 11);

    if (*human == '.' || *human == '/' || *human == '\0') return 0;

    int dot_pos = -1;
    int hlen = (int) strlen(human);
    for (int j = 0; j < hlen; j++) {
        if (human[j] == '.') { dot_pos = j; break; }
    }

    int name_len = (dot_pos >= 0) ? dot_pos : hlen;
    if (name_len == 0 || name_len > 8) return 0;

    for (int j = 0; j < name_len; j++) {
        out[j] = (u8) to_upper(human[j]);
    }

    if (dot_pos >= 0) {
        const char* ext = human + dot_pos + 1;
        int ext_len = (int) strlen(ext);
        if (ext_len > 3) return 0;
        for (int j = 0; j < ext_len; j++) {
            out[8 + j] = (u8) to_upper(ext[j]);
        }
    }
    return 1;
}

static int is_dot_entry(const u8* e) {
    if (e[0] != '.') return 0;
    if (e[1] == ' ') return 1;
    if (e[1] == '.') return 1;
    return 0;
}

static void parse_dirent(const u8* e, fat32_dirent_t* d) {
    format_name(e, d->name);
    u32 hi = rd16(&e[20]);
    u32 lo = rd16(&e[26]);
    d->first_cluster = (hi << 16) | lo;
    d->size          = rd32(&e[28]);
    d->attributes    = e[11];
    d->is_directory  = (e[11] & FAT32_ATTR_DIR) ? 1 : 0;
}

/* ─── Поиск ─────────────────────────────────────────── */

/* Ищет запись. Возвращает 1 и копирует 32 байта в entry_out. */
static int find_in_dir(u32 dir_cluster, const char* name, u8* entry_out) {
    u32 cluster = dir_cluster;
    u8* cbuf = (u8*) pmm_alloc_page();
    if (!cbuf) return 0;

    while (cluster != 0) {
        if (fs.cluster_size > 4096) break;
        if (!read_cluster(cluster, cbuf)) break;

        u32 entries = fs.cluster_size / 32;
        for (u32 i = 0; i < entries; i++) {
            const u8* e = &cbuf[i * 32];
            if (e[0] == 0x00) { pmm_free_page(cbuf); return 0; }
            if (e[0] == 0xE5) continue;
            if ((e[11] & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;
            if (e[11] & FAT32_ATTR_LABEL) continue;

            if (name_matches(name, e)) {
                memcpy(entry_out, e, 32);
                pmm_free_page(cbuf);
                return 1;
            }
        }
        cluster = next_cluster(cluster);
    }
    pmm_free_page(cbuf);
    return 0;
}

/* То же, но возвращает и позицию (LBA + offset) для последующего апдейта. */
static int find_in_dir_pos(u32 dir_cluster, const char* name,
                           u32* out_lba, u32* out_offset, u8* entry_out) {
    u32 cluster = dir_cluster;
    u8* cbuf = (u8*) pmm_alloc_page();
    if (!cbuf) return 0;

    while (cluster != 0) {
        if (fs.cluster_size > 4096) break;

        u32 base_lba = cluster_to_lba(cluster);
        for (u32 s = 0; s < fs.sectors_per_cluster; s++) {
            if (!read_sector(base_lba + s, cbuf)) break;

            for (u32 i = 0; i < 16; i++) {
                u32 offset = i * 32;
                const u8* e = &cbuf[offset];
                if (e[0] == 0x00) { pmm_free_page(cbuf); return 0; }
                if (e[0] == 0xE5) continue;
                if ((e[11] & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;
                if (e[11] & FAT32_ATTR_LABEL) continue;

                if (name_matches(name, e)) {
                    *out_lba = base_lba + s;
                    *out_offset = offset;
                    memcpy(entry_out, e, 32);
                    pmm_free_page(cbuf);
                    return 1;
                }
            }
        }
        cluster = next_cluster(cluster);
    }
    pmm_free_page(cbuf);
    return 0;
                           }

                           /* Найти свободный слот (0x00 или 0xE5) в директории. */
                           static int find_free_slot(u32 dir_cluster, u32* out_lba, u32* out_offset) {
                               u32 cluster = dir_cluster;
                               u8* cbuf = (u8*) pmm_alloc_page();
                               if (!cbuf) return 0;

                               while (cluster != 0) {
                                   if (fs.cluster_size > 4096) break;

                                   u32 base_lba = cluster_to_lba(cluster);
                                   for (u32 s = 0; s < fs.sectors_per_cluster; s++) {
                                       if (!read_sector(base_lba + s, cbuf)) break;

                                       for (u32 i = 0; i < 16; i++) {
                                           u32 offset = i * 32;
                                           u8 first = cbuf[offset];
                                           if (first == 0x00 || first == 0xE5) {
                                               *out_lba = base_lba + s;
                                               *out_offset = offset;
                                               pmm_free_page(cbuf);
                                               return 1;
                                           }
                                       }
                                   }
                                   cluster = next_cluster(cluster);
                               }
                               pmm_free_page(cbuf);
                               return 0;
                           }

                           /* Обновить 32-байтную запись в директории. */
                           static int update_dir_entry(u32 lba, u32 offset, const u8* entry) {
                               u8 sec[512];
                               if (!read_sector(lba, sec)) return 0;
                               memcpy(sec + offset, entry, 32);
                               return write_sector(lba, sec);
                           }

                           /* ─── Резолв путей ─────────────────────────────────── */

                           static int resolve_path(const char* path, u8* entry_out) {
                               while (*path == '/') path++;
                               if (*path == '\0') return 0;

                               u32 current_cluster = fs.root_cluster;
                               u8  current_entry[32];
                               char component[FAT32_MAX_NAME];

                               for (;;) {
                                   int ci = 0;
                                   while (*path && *path != '/' && ci < FAT32_MAX_NAME - 1) {
                                       component[ci++] = *path++;
                                   }
                                   component[ci] = '\0';

                                   if (ci == 0) {
                                       if (*path == '/') { path++; continue; }
                                       return 0;
                                   }

                                   if (!find_in_dir(current_cluster, component, current_entry)) return 0;

                                   const char* q = path;
                                   while (*q == '/') q++;
                                   int is_last = (*q == '\0');

                                   if (is_last) {
                                       memcpy(entry_out, current_entry, 32);
                                       return 1;
                                   }

                                   if (!(current_entry[11] & FAT32_ATTR_DIR)) return 0;

                                   u32 hi = rd16(&current_entry[20]);
                                   u32 lo = rd16(&current_entry[26]);
                                   current_cluster = (hi << 16) | lo;
                                   if (current_cluster < 2) return 0;

                                   while (*path == '/') path++;
                               }
                           }

                           static u32 resolve_dir_cluster(const char* path) {
                               while (*path == '/') path++;
                               if (*path == '\0') return fs.root_cluster;

                               u8 entry[32];
                               if (!resolve_path(path, entry)) return 0;
                               if (!(entry[11] & FAT32_ATTR_DIR)) return 0;

                               u32 hi = rd16(&entry[20]);
                               u32 lo = rd16(&entry[26]);
                               u32 cl = (hi << 16) | lo;
                               if (cl == 0) return 0;
                               return cl;
                           }

                           /* Разбирает путь на parent_cluster и leaf.
                            * "DIR/FILE.TXT" → parent=cluster(DIR), leaf="FILE.TXT"
                            * "FILE.TXT"     → parent=root_cluster, leaf="FILE.TXT" */
                           static int resolve_parent(const char* path, u32* parent_cluster, char* leaf) {
                               while (*path == '/') path++;
                               if (*path == '\0') return 0;

                               const char* last_slash = NULL;
                               for (const char* p = path; *p; p++) {
                                   if (*p == '/') last_slash = p;
                               }

                               if (!last_slash) {
                                   *parent_cluster = fs.root_cluster;
                                   if (strlen(path) >= FAT32_MAX_NAME) return 0;
                                   strcpy(leaf, path);
                               } else {
                                   char parent_path[FAT32_MAX_PATH];
                                   u32 plen = (u32)(last_slash - path);
                                   if (plen == 0) {
                                       *parent_cluster = fs.root_cluster;
                                   } else {
                                       if (plen >= FAT32_MAX_PATH) return 0;
                                       memcpy(parent_path, path, plen);
                                       parent_path[plen] = '\0';
                                       *parent_cluster = resolve_dir_cluster(parent_path);
                                       if (*parent_cluster < 2) return 0;
                                   }

                                   const char* lf = last_slash + 1;
                                   if (strlen(lf) >= FAT32_MAX_NAME) return 0;
                                   strcpy(leaf, lf);
                               }
                               return leaf[0] != '\0';
                           }

                           /* ═══════════════════════════════════════════════════════
                            *  MOUNT
                            * ═══════════════════════════════════════════════════════ */

                           int fat32_mount(block_device_t* dev) {
                               if (!dev) return 0;
                               fs_dev = dev;
                               fs_mounted = 0;

                               if (!bdev_read(dev, 0, 1, boot_sector)) {
                                   printf("[fat32] cannot read boot sector from %s\n", dev->name);
                                   return 0;
                               }

                               if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
                                   printf("[fat32] no boot signature on %s\n", dev->name);
                                   return 0;
                               }

                               const char* fs_type = (const char*) &boot_sector[82];
                               if (strncmp(fs_type, "FAT32", 5) != 0) {
                                   printf("[fat32] not FAT32 (type='%.8s')\n", fs_type);
                                   return 0;
                               }

                               fs.bytes_per_sector    = rd16(&boot_sector[11]);
                               fs.sectors_per_cluster = boot_sector[13];
                               fs.reserved_sectors    = rd16(&boot_sector[14]);
                               fs.num_fats            = boot_sector[16];
                               fs.sectors_per_fat     = rd32(&boot_sector[36]);
                               fs.root_cluster        = rd32(&boot_sector[44]);

                               /* Total sectors: 32-bit поле (offset 32) обязательно для FAT32,
                                * но mkfs.fat на маленьких томах (< 33 МБ) его не заполняет.
                                * Каскадный fallback: 32-bit → 16-bit → размер устройства. */
                               fs.total_sectors = rd32(&boot_sector[32]);
                               if (fs.total_sectors == 0) {
                                   fs.total_sectors = rd16(&boot_sector[19]);
                                   printf("[fat32]   total_sectors from 16-bit field: %u\n", fs.total_sectors);
                               }
                               if (fs.total_sectors == 0) {
                                   fs.total_sectors = dev->total_sectors;
                                   printf("[fat32]   total_sectors from device: %u\n", fs.total_sectors);
                               }

                               if (fs.bytes_per_sector != 512) return 0;
                               if (fs.sectors_per_cluster == 0 ||
                                   (fs.sectors_per_cluster & (fs.sectors_per_cluster - 1))) return 0;
                               if (fs.total_sectors <= fs.data_start_lba) {
                                   /* data_start_lba ещё не вычислен, но проверка ниже */
                               }

                               fs.cluster_size   = fs.bytes_per_sector * fs.sectors_per_cluster;
                               fs.fat_start_lba  = fs.reserved_sectors;
                               fs.data_start_lba = fs.reserved_sectors + fs.num_fats * fs.sectors_per_fat;

                               if (fs.total_sectors <= fs.data_start_lba) {
                                   printf("[fat32] bad total_sectors=%u <= data_start=%u\n",
                                          fs.total_sectors, fs.data_start_lba);
                                   return 0;
                               }

                               fs.total_clusters = (fs.total_sectors - fs.data_start_lba) /
                               fs.sectors_per_cluster;

                               fs_mounted = 1;
                               printf("[fat32] mounted %s (cluster=%u bytes, root=%u, total_clusters=%u)\n",
                                      dev->name, fs.cluster_size, fs.root_cluster, fs.total_clusters);
                               return 1;
                           }

                           /* ═══════════════════════════════════════════════════════
                            *  READ
                            * ═══════════════════════════════════════════════════════ */

                           u32 fat32_read(fat32_file_t* f, void* buffer, u32 size) {
                               if (!f->is_open) return 0;
                               if (f->position >= f->size) return 0;

                               u32 remaining = f->size - f->position;
                               if (size > remaining) size = remaining;

                               u8* out = (u8*) buffer;
                               u32 total_read = 0;
                               u32 cluster = f->current_cluster;
                               u32 offset  = f->current_offset;

                               u8* cbuf = (u8*) pmm_alloc_page();
                               if (!cbuf) return 0;

                               while (total_read < size && cluster != 0) {
                                   if (fs.cluster_size > 4096) break;
                                   if (!read_cluster(cluster, cbuf)) break;

                                   while (offset < fs.cluster_size && total_read < size) {
                                       out[total_read++] = cbuf[offset++];
                                   }
                                   if (offset >= fs.cluster_size) {
                                       cluster = next_cluster(cluster);
                                       offset = 0;
                                   }
                               }

                               pmm_free_page(cbuf);
                               f->current_cluster = cluster;
                               f->current_offset  = offset;
                               f->position       += total_read;
                               return total_read;
                           }

                           void fat32_close(fat32_file_t* f) { f->is_open = 0; }

                           int fat32_open(const char* path, fat32_file_t* f) {
                               if (!fs_mounted) return 0;

                               u8 entry[32];
                               if (!resolve_path(path, entry)) return 0;

                               fat32_dirent_t d;
                               parse_dirent(entry, &d);
                               if (d.is_directory) return 0;

                               f->start_cluster   = d.first_cluster;
                               f->current_cluster = d.first_cluster;
                               f->current_offset  = 0;
                               f->position        = 0;
                               f->size            = d.size;
                               f->is_open         = 1;
                               return 1;
                           }

                           int fat32_list(const char* path, fat32_dirent_t* out, int max_entries) {
                               if (!fs_mounted) return 0;

                               u32 start = resolve_dir_cluster(path);
                               if (start < 2) return 0;

                               u32 cluster = start;
                               u8* cbuf = (u8*) pmm_alloc_page();
                               if (!cbuf) return 0;

                               int count = 0;
                               while (cluster != 0 && count < max_entries) {
                                   if (fs.cluster_size > 4096) break;
                                   if (!read_cluster(cluster, cbuf)) break;

                                   u32 entries = fs.cluster_size / 32;
                                   for (u32 i = 0; i < entries && count < max_entries; i++) {
                                       const u8* e = &cbuf[i * 32];
                                       if (e[0] == 0x00) { pmm_free_page(cbuf); return count; }
                                       if (e[0] == 0xE5) continue;
                                       if ((e[11] & FAT32_ATTR_LFN) == FAT32_ATTR_LFN) continue;
                                       if (e[11] & FAT32_ATTR_LABEL) continue;
                                       if (is_dot_entry(e)) continue;
                                       parse_dirent(e, &out[count]);
                                       count++;
                                   }
                                   cluster = next_cluster(cluster);
                               }
                               pmm_free_page(cbuf);
                               return count;
                           }

                           int fat32_is_dir(const char* path) {
                               if (!fs_mounted) return 0;
                               while (*path == '/') path++;
                               if (*path == '\0') return 1;

                               u8 entry[32];
                               if (!resolve_path(path, entry)) return 0;
                               return (entry[11] & FAT32_ATTR_DIR) ? 1 : 0;
                           }

                           /* ═══════════════════════════════════════════════════════
                            *  WRITE API
                            * ═══════════════════════════════════════════════════════ */

                           int fat32_create(const char* path) {
                               if (!fs_mounted) return 0;

                               u32 parent;
                               char leaf[FAT32_MAX_NAME];
                               if (!resolve_parent(path, &parent, leaf)) {
                                   printf("[fat32] invalid path: %s\n", path);
                                   return 0;
                               }

                               u8 tmp[32];
                               if (find_in_dir(parent, leaf, tmp)) {
                                   printf("[fat32] %s already exists\n", path);
                                   return 0;
                               }

                               u8 shortname[11];
                               if (!encode_short_name(leaf, shortname)) {
                                   printf("[fat32] invalid 8.3 name: %s\n", leaf);
                                   return 0;
                               }

                               u32 lba, offset;
                               if (!find_free_slot(parent, &lba, &offset)) {
                                   printf("[fat32] no free slot in directory\n");
                                   return 0;
                               }

                               u8 entry[32];
                               memset(entry, 0, 32);
                               memcpy(entry, shortname, 11);
                               entry[11] = FAT32_ATTR_ARCH;

                               return update_dir_entry(lba, offset, entry);
                           }

                           int fat32_mkdir(const char* path) {
                               if (!fs_mounted) return 0;

                               u32 parent;
                               char leaf[FAT32_MAX_NAME];
                               if (!resolve_parent(path, &parent, leaf)) {
                                   printf("[fat32] invalid path: %s\n", path);
                                   return 0;
                               }

                               u8 tmp[32];
                               if (find_in_dir(parent, leaf, tmp)) {
                                   printf("[fat32] %s already exists\n", path);
                                   return 0;
                               }

                               u8 shortname[11];
                               if (!encode_short_name(leaf, shortname)) {
                                   printf("[fat32] invalid 8.3 name: %s\n", leaf);
                                   return 0;
                               }

                               u32 new_cluster = alloc_cluster();
                               if (!new_cluster) {
                                   printf("[fat32] out of clusters\n");
                                   return 0;
                               }

                               /* Инициализируем кластер: . и .. */
                               u8* cbuf = (u8*) pmm_alloc_page();
                               if (!cbuf) return 0;
                               memset(cbuf, 0, fs.cluster_size);

                               /* Запись "." — указывает на самого себя */
                               memset(cbuf + 0, ' ', 11);
                               cbuf[0]  = '.';
                               cbuf[11] = FAT32_ATTR_DIR;
                               wr16(cbuf + 20, (u16)(new_cluster >> 16));
                               wr16(cbuf + 26, (u16)(new_cluster & 0xFFFF));

                               /* Запись ".." — указывает на родителя */
                               memset(cbuf + 32, ' ', 11);
                               cbuf[32] = '.';
                               cbuf[33] = '.';
                               cbuf[43] = FAT32_ATTR_DIR;
                               wr16(cbuf + 52, (u16)(parent >> 16));
                               wr16(cbuf + 58, (u16)(parent & 0xFFFF));

                               if (!write_cluster(new_cluster, cbuf)) {
                                   pmm_free_page(cbuf);
                                   return 0;
                               }
                               pmm_free_page(cbuf);

                               /* Dir-entry в родителе */
                               u32 lba, offset;
                               if (!find_free_slot(parent, &lba, &offset)) return 0;

                               u8 entry[32];
                               memset(entry, 0, 32);
                               memcpy(entry, shortname, 11);
                               entry[11] = FAT32_ATTR_DIR;
                               wr16(entry + 20, (u16)(new_cluster >> 16));
                               wr16(entry + 26, (u16)(new_cluster & 0xFFFF));

                               return update_dir_entry(lba, offset, entry);
                           }

                           int fat32_delete(const char* path) {
                               if (!fs_mounted) return 0;

                               u32 parent;
                               char leaf[FAT32_MAX_NAME];
                               if (!resolve_parent(path, &parent, leaf)) return 0;

                               u32 lba, offset;
                               u8 entry[32];
                               if (!find_in_dir_pos(parent, leaf, &lba, &offset, entry)) {
                                   printf("[fat32] %s not found\n", path);
                                   return 0;
                               }

                               u32 start = (rd16(&entry[20]) << 16) | rd16(&entry[26]);
                               if (start >= 2) free_chain(start);

                               entry[0] = 0xE5;
                               return update_dir_entry(lba, offset, entry);
                           }

                           int fat32_write_file(const char* path, const void* data, u32 size) {
                               if (!fs_mounted) return 0;

                               u32 parent;
                               char leaf[FAT32_MAX_NAME];
                               if (!resolve_parent(path, &parent, leaf)) return 0;

                               u32 lba = 0, offset = 0;
                               u8  entry[32];

                               if (!find_in_dir_pos(parent, leaf, &lba, &offset, entry)) {
                                   if (!fat32_create(path)) return 0;
                                   if (!find_in_dir_pos(parent, leaf, &lba, &offset, entry)) return 0;
                               }

                               /* Освобождаем старую цепочку */
                               u32 old_start = (rd16(&entry[20]) << 16) | rd16(&entry[26]);
                               if (old_start >= 2) free_chain(old_start);

                               /* Пустой файл */
                               if (size == 0) {
                                   wr16(entry + 20, 0);
                                   wr16(entry + 26, 0);
                                   wr32(entry + 28, 0);
                                   return update_dir_entry(lba, offset, entry);
                               }

                               u32 cluster_bytes = fs.cluster_size;
                               u32 needed = (size + cluster_bytes - 1) / cluster_bytes;

                               u8* cbuf = (u8*) pmm_alloc_page();
                               if (!cbuf) return 0;

                               const u8* src = (const u8*) data;
                               u32 written = 0;
                               u32 first = 0, prev = 0;

                               for (u32 i = 0; i < needed; i++) {
                                   u32 c = alloc_cluster();
                                   if (!c) {
                                       pmm_free_page(cbuf);
                                       if (first) free_chain(first);
                                       return 0;
                                   }

                                   if (i == 0) first = c;
                                   else fat_set(prev, c);

                                   u32 chunk = size - written;
                                   if (chunk > cluster_bytes) chunk = cluster_bytes;

                                   memset(cbuf, 0, cluster_bytes);
                                   memcpy(cbuf, src + written, chunk);

                                   if (!write_cluster(c, cbuf)) {
                                       pmm_free_page(cbuf);
                                       if (first) free_chain(first);
                                       return 0;
                                   }

                                   written += chunk;
                                   prev = c;
                               }

                               if (prev) fat_set(prev, 0x0FFFFFFF);

                               pmm_free_page(cbuf);

                               /* Обновляем dir-entry */
                               wr16(entry + 20, (u16)(first >> 16));
                               wr16(entry + 26, (u16)(first & 0xFFFF));
                               wr32(entry + 28, size);

                               return update_dir_entry(lba, offset, entry);
                           }

                           /* ═══════════════════════════════════════════════════════
                            *  ДИАГНОСТИКА
                            * ═══════════════════════════════════════════════════════ */

                           void fat32_dump_info(void) {
                               if (!fs_mounted) { printf("[fat32] not mounted\n"); return; }
                               printf("[fat32] volume on %s:\n", fs_dev->name);
                               printf("  bytes/sector:     %u\n", fs.bytes_per_sector);
                               printf("  sectors/cluster:  %u\n", fs.sectors_per_cluster);
                               printf("  cluster size:     %u bytes\n", fs.cluster_size);
                               printf("  root cluster:     %u\n", fs.root_cluster);
                               printf("  data start LBA:   %u\n", fs.data_start_lba);
                               printf("  total clusters:   %u\n", fs.total_clusters);
                               printf("  total size:       %u MB\n",
                                      (fs.total_sectors * fs.bytes_per_sector) / (1024 * 1024));
                           }

                           static void test_walk(const char* prefix, u32 depth) {
                               if (depth > 4) return;

                               fat32_dirent_t entries[32];
                               int n = fat32_list(prefix, entries, 32);
                               if (n <= 0) return;

                               for (int i = 0; i < n; i++) {
                                   for (u32 j = 0; j < depth; j++) printf("  ");
                                   printf("  %-14s  %s  %u bytes\n",
                                          entries[i].name,
                                          entries[i].is_directory ? "DIR " : "FILE",
                                          entries[i].size);

                                   if (entries[i].is_directory) {
                                       char subpath[FAT32_MAX_PATH];
                                       int len = (int) strlen(prefix);
                                       if (len > 0 && prefix[len - 1] == '/') {
                                           strcpy(subpath, prefix);
                                           strcat(subpath, entries[i].name);
                                       } else {
                                           strcpy(subpath, prefix);
                                           strcat(subpath, "/");
                                           strcat(subpath, entries[i].name);
                                       }
                                       test_walk(subpath, depth + 1);
                                   }
                               }
                           }

                           void fat32_test(void) {
                               if (!fs_mounted) { printf("[fat32] not mounted\n"); return; }
                               fat32_dump_info();
                               printf("  walking filesystem:\n");
                               test_walk("/", 0);
                           }
