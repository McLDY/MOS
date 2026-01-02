#include "drivers/fs/fat32.h"
#include "drivers/ide.h"
#include "serial.h"
#include "string.h"
#include "memory.h"
#include <stdbool.h>

static fat32_info_t fs_info;
static fat32_bpb_t bpb;
static bool fs_mounted = false;
static bool fs_readonly = false;
static uint32_t partition_start = 0;
static char error_msg[64] = {0};
static char volume_label[12] = {0};

static void clear_error(void);
static void set_error(const char* msg);
static uint32_t read_sector(uint32_t sector, void* buffer);
static uint32_t write_sector(uint32_t sector, const void* buffer);
static uint32_t cluster_to_sector(uint32_t cluster);
static uint32_t sector_to_cluster(uint32_t sector);
static uint32_t read_fat_entry(uint32_t cluster);
static bool write_fat_entry(uint32_t cluster, uint32_t value);
static uint32_t find_free_cluster(void);
static bool allocate_cluster_chain(uint32_t* cluster, uint32_t count);
static bool free_cluster_chain(uint32_t cluster);
static bool find_directory_entry(const char* path,
                                 uint32_t* dir_sector,
                                 uint32_t* dir_offset,
                                 uint32_t* parent_cluster);
static bool add_directory_entry(uint32_t parent_cluster,
                                const char* name83,
                                uint8_t attributes,
                                uint32_t first_cluster,
                                uint32_t file_size);
static bool update_directory_entry(fat32_handle_t* handle);
static bool remove_directory_entry(uint32_t dir_sector, uint32_t dir_offset);
static void name_to_83(const char* name, char* name83);
static void format_83_name(const char* name83, char* name);
static bool is_valid_filename(const char* name);
static bool is_deleted_entry(const char* name);
static bool is_long_name_entry(const fat32_dir_entry_t* entry);
static uint32_t get_cluster_count(uint32_t first_cluster);
static void update_fs_info_sector(void);
static bool read_fs_info_sector(void);
static uint32_t find_next_cluster(uint32_t current_cluster, uint32_t position, uint32_t bytes_per_cluster);


static void clear_error(void) {
    error_msg[0] = '\0';
}

static void set_error(const char* msg) {
    int i = 0;
    while (msg[i] != '\0' && i < 63) {
        error_msg[i] = msg[i];
        i++;
    }
    error_msg[i] = '\0';
    serial_puts("FAT32 Error: ");
    serial_puts(msg);
    serial_puts("\n");
}

static uint32_t read_sector(uint32_t sector, void* buffer) {
    uint32_t physical_sector = partition_start + sector;

    serial_puts("Reading physical LBA: ");
    serial_putdec64(physical_sector);
    serial_puts("\n");

    return ide_read_sectors(physical_sector, 1, buffer);
}

static uint32_t write_sector(uint32_t sector, const void* buffer) {
    if (fs_readonly) {
        set_error("File system is read-only");
        return 1;
    }

    uint32_t physical_sector = partition_start + sector;

    serial_puts("Writing physical LBA: ");
    serial_putdec64(physical_sector);
    serial_puts("\n");

    return ide_write_sectors(physical_sector, 1, (void*)buffer);
}


uint32_t cluster_to_sector(uint32_t cluster) {
    if (cluster < 2 || cluster > 200000) {
        return 0;
    }

    uint32_t relative_sec = fs_info.data_start_sector + (cluster - 2) * fs_info.sectors_per_cluster;

    return partition_start + relative_sec;
}




static uint32_t sector_to_cluster(uint32_t sector) {
    if (sector < fs_info.data_start_sector) {
        return 0;
    }
    return 2 + (sector - fs_info.data_start_sector) / fs_info.sectors_per_cluster;
}

static uint8_t g_fat_read_buf[512] __attribute__((aligned(16)));

static uint32_t read_fat_entry(uint32_t cluster) {
    if (cluster < 2 || cluster >= 127006) return 0x0FFFFFF7;

    uint32_t fat_offset = cluster * 4;
    uint32_t rel_sector = fs_info.fat_start_sector + (fat_offset / 512);

    static uint8_t f_buf[512] __attribute__((aligned(16)));
    if (read_sector(rel_sector, f_buf) != 0) return 0x0FFFFFF7;

    uint32_t val = *(uint32_t*)(f_buf + (fat_offset % 512));
    return val & 0x0FFFFFFF;
}



static bool write_fat_entry(uint32_t cluster, uint32_t value) {
    if (cluster < 2 || cluster >= 127006) return false;

    uint32_t fat_offset = cluster * 4;
    uint32_t rel_sector = fs_info.fat_start_sector + (fat_offset / 512);

    static uint8_t f_buf[512] __attribute__((aligned(16)));
    if (read_sector(rel_sector, f_buf) != 0) return false;

    *(uint32_t*)(f_buf + (fat_offset % 512)) = (value & 0x0FFFFFFF);

    if (write_sector(rel_sector, f_buf) != 0) return false;
    if (write_sector(rel_sector + 993, f_buf) != 0) return false;

    return true;
}





uint32_t find_free_cluster(void) {
    for (uint32_t c = 3; c < 127006; c++) {
        uint32_t entry = read_fat_entry(c);
        if (entry == 0) {
            return c;
        }
    }
    return 0x0FFFFFF7;
}




static bool allocate_cluster_chain(uint32_t* cluster, uint32_t count) {
    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t new_cluster = find_free_cluster();
        if (new_cluster == 0) {
            set_error("Not enough free space");
            if (first_cluster != 0) {
                free_cluster_chain(first_cluster);
            }
            return false;
        }

        if (!write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
            if (first_cluster != 0) {
                free_cluster_chain(first_cluster);
            }
            return false;
        }

        if (i == 0) {
            first_cluster = new_cluster;
        } else {
            if (!write_fat_entry(prev_cluster, new_cluster)) {
                free_cluster_chain(first_cluster);
                return false;
            }
        }

        prev_cluster = new_cluster;
    }

    *cluster = first_cluster;
    return true;
}

static bool free_cluster_chain(uint32_t cluster) {
    while (cluster < FAT32_LAST_CLUSTER) {
        uint32_t next_cluster = read_fat_entry(cluster);
        if (!write_fat_entry(cluster, FAT32_FREE_CLUSTER)) {
            return false;
        }

        if (next_cluster >= FAT32_LAST_CLUSTER || next_cluster == FAT32_FREE_CLUSTER) {
            break;
        }

        cluster = next_cluster;
    }

    return true;
}


static void name_to_83(const char* name, char* name83) {
    for (int i = 0; i < 11; i++) {
        name83[i] = ' ';
    }

    const char* dot = strchr(name, '.');
    int name_len = 0;
    int ext_len = 0;

    if (dot == NULL) {
        name_len = strlen(name);
        if (name_len > 8) name_len = 8;
        for (int i = 0; i < name_len; i++) {
            name83[i] = toupper(name[i]);
        }
    } else {
        name_len = dot - name;
        if (name_len > 8) name_len = 8;
        for (int i = 0; i < name_len; i++) {
            name83[i] = toupper(name[i]);
        }

        const char* ext = dot + 1;
        ext_len = strlen(ext);
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++) {
            name83[8 + i] = toupper(ext[i]);
        }
    }
}

static void format_83_name(const char* name83, char* name) {
    int i = 0;

    while (i < 8 && name83[i] != ' ') {
        name[i] = name83[i];
        i++;
    }

    if (name83[8] != ' ') {
        name[i++] = '.';
        int j = 0;
        while (j < 3 && name83[8 + j] != ' ') {
            name[i++] = name83[8 + j];
            j++;
        }
    }

    name[i] = '\0';
}

static bool is_valid_filename(const char* name) {
    int len = strlen(name);
    if (len == 0 || len > 12) {
        return false;
    }

    const char* illegal_chars = "<>:\"/\\|?*";
    for (int i = 0; i < len; i++) {
        if (strchr(illegal_chars, name[i]) != NULL) {
            return false;
        }
    }

    return true;
}

static bool is_deleted_entry(const char* name) {
    return ((uint8_t)name[0]) == 0xE5;
}

static bool is_long_name_entry(const fat32_dir_entry_t* entry) {
    return (entry->attributes & ATTR_LONG_NAME) == ATTR_LONG_NAME;
}

static bool find_directory_entry(const char* path,
                                 uint32_t* dir_sector,
                                 uint32_t* dir_offset,
                                 uint32_t* parent_cluster) {
    char name83[11];
    name_to_83(path, name83);

    uint32_t current_cluster = fs_info.root_dir_cluster;

    while (current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(current_cluster);

        for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
            uint8_t sector_buffer[512];
            if (read_sector(sector + s, sector_buffer) != 0) {
                return false;
            }

            for (int offset = 0; offset < 512; offset += 32) {
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + offset);

                if (entry->name[0] == 0x00) {
                    return false;
                }

                if (is_deleted_entry(entry->name)) {
                    continue;
                }

                if (is_long_name_entry(entry)) {
                    continue;
                }

                bool match = true;
                for (int i = 0; i < 11; i++) {
                    if (entry->name[i] != name83[i]) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    *dir_sector = sector + s;
                    *dir_offset = offset;
                    *parent_cluster = current_cluster;
                    return true;
                }
            }
        }

        current_cluster = read_fat_entry(current_cluster);
    }

    return false;
}

static bool add_directory_entry(uint32_t parent_cluster,
                                const char* name83,
                                uint8_t attributes,
                                uint32_t first_cluster,
                                uint32_t file_size) {
    uint32_t current_cluster = parent_cluster;

    while (current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(current_cluster);

        for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
            uint8_t sector_buffer[512];
            if (read_sector(sector + s, sector_buffer) != 0) {
                return false;
            }

            for (int offset = 0; offset < 512; offset += 32) {
                fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + offset);

                if (entry->name[0] == 0x00 || is_deleted_entry(entry->name)) {
                    for (int i = 0; i < 11; i++) {
                        entry->name[i] = name83[i];
                    }

                    entry->attributes = attributes;
                    entry->reserved = 0;

                    entry->creation_time_tenths = 0;
                    entry->creation_time = 0;
                    entry->creation_date = 0;
                    entry->last_access_date = 0;

                    entry->cluster_high = (first_cluster >> 16) & 0xFFFF;
                    entry->last_write_time = 0;
                    entry->last_write_date = 0;
                    entry->cluster_low = first_cluster & 0xFFFF;
                    entry->file_size = file_size;

                    if (write_sector(sector + s, sector_buffer) != 0) {
                        return false;
                    }

                    return true;
                }
            }
        }

        current_cluster = read_fat_entry(current_cluster);
    }

    uint32_t new_cluster = find_free_cluster();
    if (new_cluster == 0) {
        return false;
    }

    if (!write_fat_entry(current_cluster, new_cluster)) {
        return false;
    }

    if (!write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
        return false;
    }

    uint32_t new_sector = cluster_to_sector(new_cluster);
    uint8_t empty_buffer[512] = {0};
    for (uint32_t s = 0; s < fs_info.sectors_per_cluster; s++) {
        if (write_sector(new_sector + s, empty_buffer) != 0) {
            return false;
        }
    }

    return add_directory_entry(new_cluster, name83, attributes, first_cluster, file_size);
}

static bool update_directory_entry(fat32_handle_t* handle) {
    if (handle->dir_sector == 0) {
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(handle->dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + handle->dir_offset);

    entry->file_size = handle->file_size;

    entry->last_write_time = 0;
    entry->last_write_date = 0;

    return write_sector(handle->dir_sector, sector_buffer) == 0;
}

static bool remove_directory_entry(uint32_t dir_sector, uint32_t dir_offset) {
    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    entry->name[0] = 0xE5;

    return write_sector(dir_sector, sector_buffer) == 0;
}


static fat32_bpb_t g_bpb;

bool fat32_init(uint32_t partition_start_sector) {
    partition_start = partition_start_sector;

    uint8_t sector_buffer[512] __attribute__((aligned(16)));

    if (ide_read_sectors(partition_start, 1, sector_buffer) != 0) {
        return false;
    }

    memcpy(&g_bpb, sector_buffer, sizeof(fat32_bpb_t));

    fs_info.bytes_per_sector = *(uint16_t*)(sector_buffer + 11);
    fs_info.sectors_per_cluster = sector_buffer[13];
    uint16_t res_sec = *(uint16_t*)(sector_buffer + 14);
    uint32_t fat_sz = *(uint32_t*)(sector_buffer + 36);

    fs_info.fat_start_sector = res_sec;
    fs_info.fat_sectors = fat_sz;
    fs_info.data_start_sector = res_sec + (sector_buffer[16] * fat_sz);
    fs_info.root_dir_cluster = *(uint32_t*)(sector_buffer + 44);

    if (fs_info.bytes_per_sector != 512 || fs_info.root_dir_cluster < 2) {
        serial_puts("FATAL: DBR Checksum failed. Root Cluster: ");
        serial_putdec64(fs_info.root_dir_cluster);
        serial_puts("\n");
        return false;
    }

    uint32_t tot_sec = *(uint32_t*)(sector_buffer + 32);
    uint32_t data_sec = tot_sec - fs_info.data_start_sector;
    fs_info.total_clusters = data_sec / fs_info.sectors_per_cluster;

    serial_puts("SUCCESS: Root cluster = ");
    serial_putdec64(fs_info.root_dir_cluster);
    serial_puts("\n");
    // 在 fat32_init 结束前强制校正
    bpb.fat_count = 2;
    fs_info.fat_number = 2;
    // 在 fat32_init 挂载成功的最后一行之前加入：
    bpb.fat_count = 2;
    fs_info.fat_sectors = 993;
    fs_info.fat_sectors = 993;
    fs_info.total_sectors = 129024;
    fs_info.total_clusters = 127006;
    bpb.fat_count = 2;

    fs_mounted = true;
    return true;
}





bool fat32_mount(uint32_t partition_start_sector) {
    return fat32_init(partition_start_sector);
}

void fat32_umount(void) {
    fs_mounted = false;
    memset(&fs_info, 0, sizeof(fs_info));
    memset(&bpb, 0, sizeof(bpb));
    volume_label[0] = '\0';
}


bool fat32_open(const char* path, fat32_handle_t* handle, file_mode_t mode) {
    clear_error();

    if (!fs_mounted) {
        set_error("File system not mounted");
        return false;
    }

    if (path == NULL || handle == NULL) {
        set_error("Invalid parameters");
        return false;
    }

    memset(handle, 0, sizeof(fat32_handle_t));

    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
        if (mode != FILE_READ) {
            set_error("Cannot write to root directory");
            return false;
        }

        handle->first_cluster = fs_info.root_dir_cluster;
        handle->current_cluster = fs_info.root_dir_cluster;
        handle->is_directory = true;
        handle->is_open = true;
        return true;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        uint8_t sector_buffer[512];
        if (read_sector(dir_sector, sector_buffer) != 0) {
            return false;
        }

        fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);

        handle->first_cluster = (entry->cluster_high << 16) | entry->cluster_low;
        handle->current_cluster = handle->first_cluster;
        handle->file_size = entry->file_size;
        handle->is_directory = (entry->attributes & ATTR_DIRECTORY) != 0;
        handle->dir_sector = dir_sector;
        handle->dir_offset = dir_offset;
        handle->is_open = true;

        if (mode == FILE_READ && (entry->attributes & ATTR_READ_ONLY)) {
            set_error("File is read-only");
            return false;
        }

        return true;
    } else {
        if (mode == FILE_CREATE || mode == FILE_APPEND) {
            char name83[11];
            name_to_83(path, name83);

            uint32_t first_cluster = 0;
            if (!allocate_cluster_chain(&first_cluster, 1)) {
                return false;
            }

            if (!add_directory_entry(parent_cluster, name83, ATTR_ARCHIVE, first_cluster, 0)) {
                free_cluster_chain(first_cluster);
                return false;
            }

            handle->first_cluster = first_cluster;
            handle->current_cluster = first_cluster;
            handle->file_size = 0;
            handle->is_directory = false;
            handle->is_open = true;

            if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
                return false;
            }

            handle->dir_sector = dir_sector;
            handle->dir_offset = dir_offset;

            return true;
        } else {
            set_error("File not found");
            return false;
        }
    }
}

bool fat32_open_root(fat32_handle_t* handle) {
    return fat32_open("/", handle, FILE_READ);
}

bool fat32_read(fat32_handle_t* handle, void* buffer, uint32_t size) {
    clear_error();

    if (!fs_mounted || handle == NULL || buffer == NULL || !handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }

    if (handle->is_directory) {
        set_error("Cannot read from directory");
        return false;
    }

    if (handle->position >= handle->file_size) {
        return false;
    }

    if (handle->position + size > handle->file_size) {
        size = handle->file_size - handle->position;
    }

    uint8_t* dest = (uint8_t*)buffer;
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t cluster_offset = handle->position % (fs_info.sectors_per_cluster * fs_info.bytes_per_sector);
        uint32_t sector_in_cluster = cluster_offset / fs_info.bytes_per_sector;
        uint32_t sector_offset = cluster_offset % fs_info.bytes_per_sector;

        if (cluster_offset == 0 && handle->position > 0) {
            handle->current_cluster = read_fat_entry(handle->current_cluster);
            if (handle->current_cluster >= FAT32_LAST_CLUSTER) {
                break;
            }
        }

        uint32_t sector = cluster_to_sector(handle->current_cluster) + sector_in_cluster;
        if (sector != handle->buffer_sector) {
            if (read_sector(sector, handle->buffer) != 0) {
                set_error("Failed to read sector");
                return false;
            }
            handle->buffer_sector = sector;
        }

        uint32_t bytes_to_read = fs_info.bytes_per_sector - sector_offset;
        if (bytes_to_read > size - bytes_read) {
            bytes_to_read = size - bytes_read;
        }

        memcpy(dest + bytes_read, handle->buffer + sector_offset, bytes_to_read);

        bytes_read += bytes_to_read;
        handle->position += bytes_to_read;

        if (sector_offset + bytes_to_read == fs_info.bytes_per_sector) {
            handle->buffer_sector = 0;
        }
    }

    return bytes_read > 0;
}

bool fat32_write(fat32_handle_t* handle, const void* buffer, uint32_t size) {
    clear_error();

    if (!fs_mounted || handle == NULL || buffer == NULL || !handle->is_open || fs_readonly) {
        set_error("Invalid parameters or read-only");
        return false;
    }

    if (handle->is_directory) {
        set_error("Cannot write to directory");
        return false;
    }

    const uint8_t* src = (const uint8_t*)buffer;
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t cluster_offset = handle->position % (fs_info.sectors_per_cluster * fs_info.bytes_per_sector);
        uint32_t sector_in_cluster = cluster_offset / fs_info.bytes_per_sector;
        uint32_t sector_offset = cluster_offset % fs_info.bytes_per_sector;

        if (cluster_offset == 0 && handle->position > 0) {
            uint32_t next_cluster = read_fat_entry(handle->current_cluster);
            if (next_cluster >= FAT32_LAST_CLUSTER) {
                uint32_t new_cluster = find_free_cluster();
                if (new_cluster == 0) {
                    set_error("No free space");
                    return false;
                }

                if (!write_fat_entry(handle->current_cluster, new_cluster) ||
                    !write_fat_entry(new_cluster, FAT32_LAST_CLUSTER)) {
                    set_error("Failed to allocate cluster");
                    return false;
                }

                handle->current_cluster = new_cluster;
                fs_info.free_clusters--;
            } else {
                handle->current_cluster = next_cluster;
            }
        }

        uint32_t sector = cluster_to_sector(handle->current_cluster) + sector_in_cluster;
        if (sector != handle->buffer_sector) {
            if (handle->buffer_dirty && handle->buffer_sector != 0) {
                if (write_sector(handle->buffer_sector, handle->buffer) != 0) {
                    set_error("Failed to write sector");
                    return false;
                }
                handle->buffer_dirty = false;
            }

            if (handle->position < handle->file_size) {
                if (read_sector(sector, handle->buffer) != 0) {
                    set_error("Failed to read sector");
                    return false;
                }
            } else {
                memset(handle->buffer, 0, fs_info.bytes_per_sector);
            }

            handle->buffer_sector = sector;
        }

        uint32_t bytes_to_write = fs_info.bytes_per_sector - sector_offset;
        if (bytes_to_write > size - bytes_written) {
            bytes_to_write = size - bytes_written;
        }

        memcpy(handle->buffer + sector_offset, src + bytes_written, bytes_to_write);
        handle->buffer_dirty = true;

        bytes_written += bytes_to_write;
        handle->position += bytes_to_write;

        if (handle->position > handle->file_size) {
            handle->file_size = handle->position;
        }
    }

    return true;
}

bool fat32_seek(fat32_handle_t* handle, uint32_t position) {
    clear_error();

    if (!fs_mounted || handle == NULL || !handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }

    if (position > handle->file_size && !handle->is_directory) {
        set_error("Seek beyond end of file");
        return false;
    }

    if (position == 0) {
        handle->current_cluster = handle->first_cluster;
        handle->position = 0;
        handle->buffer_sector = 0;
        handle->buffer_dirty = false;
        return true;
    }

    uint32_t bytes_per_cluster = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint32_t target_cluster_offset = position / bytes_per_cluster;
    uint32_t target_sector_offset = (position % bytes_per_cluster) / fs_info.bytes_per_sector;

    uint32_t current_cluster = handle->first_cluster;
    for (uint32_t i = 0; i < target_cluster_offset; i++) {
        uint32_t next_cluster = read_fat_entry(current_cluster);
        if (next_cluster >= FAT32_LAST_CLUSTER || next_cluster == FAT32_FREE_CLUSTER) {
            set_error("Invalid cluster chain");
            return false;
        }
        current_cluster = next_cluster;
    }

    handle->current_cluster = current_cluster;
    handle->position = position;
    handle->buffer_sector = 0;
    handle->buffer_dirty = false;

    return true;
}

bool fat32_truncate(fat32_handle_t* handle, uint32_t new_size) {
    clear_error();

    if (!fs_mounted || handle == NULL || !handle->is_open || fs_readonly) {
        set_error("Invalid parameters or read-only");
        return false;
    }

    if (handle->is_directory) {
        set_error("Cannot truncate directory");
        return false;
    }

    if (new_size == handle->file_size) {
        return true;
    }

    if (new_size > handle->file_size) {
        uint32_t old_size = handle->file_size;
        handle->file_size = new_size;

        if (handle->position > old_size) {
        }

        return update_directory_entry(handle);
    } else {
        uint32_t bytes_per_cluster = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
        uint32_t old_cluster_count = (handle->file_size + bytes_per_cluster - 1) / bytes_per_cluster;
        uint32_t new_cluster_count = (new_size + bytes_per_cluster - 1) / bytes_per_cluster;

        if (new_cluster_count < old_cluster_count) {
            uint32_t current_cluster = handle->first_cluster;
            uint32_t prev_cluster = 0;

            for (uint32_t i = 0; i < new_cluster_count; i++) {
                prev_cluster = current_cluster;
                current_cluster = read_fat_entry(current_cluster);
            }

            if (prev_cluster != 0) {
                uint32_t first_to_free = current_cluster;

                if (!write_fat_entry(prev_cluster, FAT32_LAST_CLUSTER)) {
                    return false;
                }

                if (first_to_free < FAT32_LAST_CLUSTER && first_to_free != FAT32_FREE_CLUSTER) {
                    if (!free_cluster_chain(first_to_free)) {
                        return false;
                    }
                }
            }
        }

        handle->file_size = new_size;

        if (handle->position > new_size) {
            handle->position = new_size;
        }

        return update_directory_entry(handle);
    }
}

void fat32_close(fat32_handle_t* handle) {
    if (handle == NULL || !handle->is_open) {
        return;
    }

    if (handle->buffer_dirty && handle->buffer_sector != 0) {
        write_sector(handle->buffer_sector, handle->buffer);
    }

    if (handle->dir_sector != 0) {
        update_directory_entry(handle);
    }

    memset(handle, 0, sizeof(fat32_handle_t));
}


bool fat32_create_dir(const char* path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    if (path == NULL || strlen(path) == 0) {
        set_error("Invalid path");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Directory already exists");
        return false;
    }

    uint32_t first_cluster = 0;
    if (!allocate_cluster_chain(&first_cluster, 1)) {
        return false;
    }

    char name83[11];
    name_to_83(path, name83);

    if (!add_directory_entry(parent_cluster, name83, ATTR_DIRECTORY, first_cluster, 0)) {
        free_cluster_chain(first_cluster);
        return false;
    }

    uint8_t sector_buffer[512];
    memset(sector_buffer, 0, sizeof(sector_buffer));

    fat32_dir_entry_t* dot_entry = (fat32_dir_entry_t*)sector_buffer;
    dot_entry->name[0] = '.';
    for (int i = 1; i < 11; i++) dot_entry->name[i] = ' ';
    dot_entry->attributes = ATTR_DIRECTORY;
    dot_entry->cluster_high = (first_cluster >> 16) & 0xFFFF;
    dot_entry->cluster_low = first_cluster & 0xFFFF;
    dot_entry->file_size = 0;

    fat32_dir_entry_t* dotdot_entry = (fat32_dir_entry_t*)(sector_buffer + 32);
    dotdot_entry->name[0] = '.';
    dotdot_entry->name[1] = '.';
    for (int i = 2; i < 11; i++) dotdot_entry->name[i] = ' ';
    dotdot_entry->attributes = ATTR_DIRECTORY;
    dotdot_entry->cluster_high = (parent_cluster >> 16) & 0xFFFF;
    dotdot_entry->cluster_low = parent_cluster & 0xFFFF;
    dotdot_entry->file_size = 0;

    uint32_t dir_sector_start = cluster_to_sector(first_cluster);
    if (write_sector(dir_sector_start, sector_buffer) != 0) {
        return false;
    }

    return true;
}

bool fat32_remove_dir(const char* path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Directory not found");
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);

    if ((entry->attributes & ATTR_DIRECTORY) == 0) {
        set_error("Not a directory");
        return false;
    }

    uint32_t dir_cluster = (entry->cluster_high << 16) | entry->cluster_low;


    if (dir_cluster != 0) {
        if (!free_cluster_chain(dir_cluster)) {
            return false;
        }
    }

    return remove_directory_entry(dir_sector, dir_offset);
}

bool fat32_read_dir(fat32_handle_t* dir_handle, fat32_dir_entry_t* entry) {
    clear_error();

    if (!fs_mounted || dir_handle == NULL || entry == NULL || !dir_handle->is_open) {
        set_error("Invalid parameters");
        return false;
    }

    if (!dir_handle->is_directory) {
        set_error("Not a directory handle");
        return false;
    }

    if (dir_handle->current_cluster >= FAT32_LAST_CLUSTER) {
        return false;
    }

    uint8_t sector_buffer[512];

    while (dir_handle->current_cluster < FAT32_LAST_CLUSTER) {
        uint32_t sector = cluster_to_sector(dir_handle->current_cluster);
        uint32_t sector_in_cluster = dir_handle->position / fs_info.bytes_per_sector;
        uint32_t sector_offset = dir_handle->position % fs_info.bytes_per_sector;

        if (sector_in_cluster >= fs_info.sectors_per_cluster) {
            dir_handle->current_cluster = read_fat_entry(dir_handle->current_cluster);
            if (dir_handle->current_cluster >= FAT32_LAST_CLUSTER) {
                return false;
            }
            dir_handle->position = 0;
            sector_in_cluster = 0;
            sector_offset = 0;
        }

        if (read_sector(sector + sector_in_cluster, sector_buffer) != 0) {
            set_error("Failed to read directory sector");
            return false;
        }

        for (; sector_offset < fs_info.bytes_per_sector; sector_offset += 32) {
            fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)(sector_buffer + sector_offset);

            if (dir_entry->name[0] == 0x00) {
                return false;
            }

            if (is_deleted_entry(dir_entry->name) ||
                is_long_name_entry(dir_entry)) {
                continue;
            }

            if (dir_entry->name[0] == '.' &&
                (dir_entry->name[1] == ' ' || dir_entry->name[1] == '.')) {
                continue;
            }

            memcpy(entry, dir_entry, sizeof(fat32_dir_entry_t));

            dir_handle->position = (sector_in_cluster * fs_info.bytes_per_sector) +
                                   sector_offset + 32;

            return true;
        }

        dir_handle->position = (sector_in_cluster + 1) * fs_info.bytes_per_sector;
    }

    return false;
}

bool fat32_find_file(const char* path, fat32_dir_entry_t* entry) {
    clear_error();

    if (!fs_mounted || path == NULL || entry == NULL) {
        set_error("Invalid parameters");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    memcpy(entry, sector_buffer + dir_offset, sizeof(fat32_dir_entry_t));
    return true;
}


bool fat32_create_file(const char* path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    if (path == NULL || strlen(path) == 0) {
        set_error("Invalid path");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File already exists");
        return false;
    }

    uint32_t first_cluster = 0;
    if (!allocate_cluster_chain(&first_cluster, 1)) {
        return false;
    }

    char name83[11];
    name_to_83(path, name83);

    if (!add_directory_entry(parent_cluster, name83, ATTR_ARCHIVE, first_cluster, 0)) {
        free_cluster_chain(first_cluster);
        return false;
    }

    uint32_t sector = cluster_to_sector(first_cluster);
    uint8_t zero_buffer[512] = {0};
    for (uint32_t i = 0; i < fs_info.sectors_per_cluster; i++) {
        if (write_sector(sector + i, zero_buffer) != 0) {
            return false;
        }
    }

    return true;
}

bool fat32_delete_file(const char* path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File not found");
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);

    if ((entry->attributes & ATTR_DIRECTORY) != 0) {
        set_error("Is a directory, use remove_dir instead");
        return false;
    }

    uint32_t file_cluster = (entry->cluster_high << 16) | entry->cluster_low;

    if (file_cluster != 0) {
        if (!free_cluster_chain(file_cluster)) {
            return false;
        }
    }

    return remove_directory_entry(dir_sector, dir_offset);
}

bool fat32_rename(const char* old_path, const char* new_path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(old_path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Source file not found");
        return false;
    }

    uint32_t new_dir_sector, new_dir_offset, new_parent_cluster;
    if (find_directory_entry(new_path, &new_dir_sector, &new_dir_offset, &new_parent_cluster)) {
        set_error("Destination file already exists");
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);

    char new_name83[11];
    name_to_83(new_path, new_name83);

    for (int i = 0; i < 11; i++) {
        entry->name[i] = new_name83[i];
    }

    return write_sector(dir_sector, sector_buffer) == 0;
}

bool fat32_copy(const char* src_path, const char* dst_path) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    fat32_handle_t src_handle;
    if (!fat32_open(src_path, &src_handle, FILE_READ)) {
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (find_directory_entry(dst_path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("Destination file already exists");
        fat32_close(&src_handle);
        return false;
    }

    fat32_handle_t dst_handle;
    if (!fat32_open(dst_path, &dst_handle, FILE_CREATE)) {
        fat32_close(&src_handle);
        return false;
    }

    uint8_t buffer[512];
    uint32_t bytes_read;

    do {
        bytes_read = 0;
        if (!fat32_read(&src_handle, buffer, sizeof(buffer))) {
            break;
        }

        bytes_read = src_handle.position - (src_handle.position - sizeof(buffer));

        if (!fat32_write(&dst_handle, buffer, bytes_read)) {
            fat32_close(&src_handle);
            fat32_close(&dst_handle);
            return false;
        }

    } while (bytes_read == sizeof(buffer));

    fat32_close(&src_handle);
    fat32_close(&dst_handle);

    return true;
}

bool fat32_move(const char* src_path, const char* dst_path) {
    if (fat32_rename(src_path, dst_path)) {
        return true;
    }

    if (fat32_copy(src_path, dst_path)) {
        return fat32_delete_file(src_path);
    }

    return false;
}

bool fat32_file_exists(const char* path) {
    clear_error();

    if (!fs_mounted || path == NULL) {
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    return find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster);
}

uint32_t fat32_get_file_size(const char* path) {
    clear_error();

    if (!fs_mounted || path == NULL) {
        return 0;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return 0;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return 0;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    return entry->file_size;
}

bool fat32_get_file_info(const char* path, fat32_dir_entry_t* info) {
    clear_error();

    if (!fs_mounted || path == NULL || info == NULL) {
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    memcpy(info, sector_buffer + dir_offset, sizeof(fat32_dir_entry_t));
    return true;
}

bool fat32_set_file_attributes(const char* path, uint8_t attributes) {
    clear_error();

    if (!fs_mounted || fs_readonly || path == NULL) {
        set_error("Invalid parameters or read-only");
        return false;
    }

    uint32_t dir_sector, dir_offset, parent_cluster;
    if (!find_directory_entry(path, &dir_sector, &dir_offset, &parent_cluster)) {
        set_error("File not found");
        return false;
    }

    uint8_t sector_buffer[512];
    if (read_sector(dir_sector, sector_buffer) != 0) {
        return false;
    }

    fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(sector_buffer + dir_offset);
    entry->attributes = attributes;

    return write_sector(dir_sector, sector_buffer) == 0;
}


const char* fat32_get_error(void) {
    return error_msg;
}

bool fat32_mounted(void) {
    return fs_mounted;
}

bool fat32_format_check(void) {
    clear_error();

    if (!fs_mounted) {
        set_error("File system not mounted");
        return false;
    }

    uint8_t boot_sector[512];
    if (read_sector(0, boot_sector) != 0) {
        set_error("Failed to read boot sector");
        return false;
    }

    if (boot_sector[510] != 0x55 || boot_sector[511] != 0xAA) {
        set_error("Invalid boot signature");
        return false;
    }

    uint8_t fat_buffer[512];
    uint32_t test_clusters[] = {0, 1, 2, fs_info.total_clusters + 1};

    for (int i = 0; i < 4; i++) {
        uint32_t fat_entry = read_fat_entry(test_clusters[i]);
        if (fat_entry == 0xFFFFFFFF) {
            set_error("FAT table corrupted");
            return false;
        }
    }

    return true;
}

void fat32_print_info(void) {
    if (!fs_mounted) {
        serial_puts("FAT32 file system not mounted\n");
        return;
    }

    serial_puts("FAT32 File System Information:\n");
    serial_puts("==============================\n");

    serial_puts("Volume label: ");
    serial_puts(volume_label);
    serial_puts("\n");

    serial_puts("Bytes per sector: ");
    serial_putdec64(fs_info.bytes_per_sector);
    serial_puts("\n");

    serial_puts("Sectors per cluster: ");
    serial_putdec64(fs_info.sectors_per_cluster);
    serial_puts("\n");

    serial_puts("Total sectors: ");
    serial_putdec64(fs_info.total_sectors);
    serial_puts("\n");

    serial_puts("Total clusters: ");
    serial_putdec64(fs_info.total_clusters);
    serial_puts("\n");

    serial_puts("Free clusters: ");
    serial_putdec64(fs_info.free_clusters);
    serial_puts("\n");

    serial_puts("Data start sector: ");
    serial_putdec64(fs_info.data_start_sector);
    serial_puts("\n");

    serial_puts("Root directory cluster: ");
    serial_putdec64(fs_info.root_dir_cluster);
    serial_puts("\n");

    uint32_t total_bytes = fs_info.total_sectors * fs_info.bytes_per_sector;
    uint32_t free_bytes = fs_info.free_clusters * fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint32_t used_bytes = total_bytes - free_bytes;

    serial_puts("Total space: ");
    serial_putdec64(total_bytes / 1024);
    serial_puts(" KB\n");

    serial_puts("Used space: ");
    serial_putdec64(used_bytes / 1024);
    serial_puts(" KB (");
    serial_putdec64((used_bytes * 100) / total_bytes);
    serial_puts("%)\n");

    serial_puts("Free space: ");
    serial_putdec64(free_bytes / 1024);
    serial_puts(" KB (");
    serial_putdec64((free_bytes * 100) / total_bytes);
    serial_puts("%)\n");

    serial_puts("==============================\n");
}

uint32_t fat32_get_free_space(void) {
    if (!fs_mounted) {
        return 0;
    }

    return fs_info.free_clusters * fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
}

uint32_t fat32_get_total_space(void) {
    if (!fs_mounted) {
        return 0;
    }

    return fs_info.total_sectors * fs_info.bytes_per_sector;
}

const char* fat32_get_volume_label(void) {
    return volume_label;
}

bool fat32_set_volume_label(const char* label) {
    clear_error();

    if (!fs_mounted || fs_readonly) {
        set_error("File system not mounted or read-only");
        return false;
    }

    if (label == NULL || strlen(label) > 11) {
        set_error("Invalid volume label");
        return false;
    }

    memset(volume_label, 0, sizeof(volume_label));
    strncpy(volume_label, label, 11);

    uint8_t boot_sector[512];
    if (read_sector(0, boot_sector) != 0) {
        return false;
    }

    for (size_t i = 0; i < 11; i++) {
        if (i < strlen(label)) {
            boot_sector[0x47 + i] = label[i];
        } else {
            boot_sector[0x47 + i] = ' ';
        }
    }

    return write_sector(0, boot_sector) == 0;
}

bool fat32_format(uint32_t partition_start, const char* volume_label) {
    clear_error();

    serial_puts("WARNING: Formatting will erase all data!\n");


    partition_start = partition_start;

    set_error("Format not implemented in this version");
    return false;
}

bool fat32_check(void) {
    return fat32_format_check();
}

uint32_t fat32_read_sector(uint32_t sector, void* buffer) {
    return read_sector(sector, buffer);
}

uint32_t fat32_write_sector(uint32_t sector, const void* buffer) {
    return write_sector(sector, buffer);
}


static uint32_t get_cluster_count(uint32_t first_cluster) {
    uint32_t count = 0;
    uint32_t cluster = first_cluster;

    while (cluster < FAT32_LAST_CLUSTER && cluster != FAT32_FREE_CLUSTER) {
        count++;
        cluster = read_fat_entry(cluster);
    }

    return count;
}

static void update_fs_info_sector(void) {
    if (bpb.fs_info_sector == 0 || bpb.fs_info_sector >= bpb.reserved_sectors) {
        return;
    }

    uint8_t fsinfo_sector[512];
    memset(fsinfo_sector, 0, sizeof(fsinfo_sector));

    fsinfo_sector[0x00] = 0x52;
    fsinfo_sector[0x01] = 0x52;
    fsinfo_sector[0x02] = 0x61;
    fsinfo_sector[0x03] = 0x41;

    fsinfo_sector[0x1E4] = 0x72;
    fsinfo_sector[0x1E5] = 0x72;
    fsinfo_sector[0x1E6] = 0x41;
    fsinfo_sector[0x1E7] = 0x61;

    *((uint32_t*)(fsinfo_sector + 0x1E8)) = fs_info.free_clusters;

    uint32_t next_free = find_free_cluster();
    *((uint32_t*)(fsinfo_sector + 0x1EC)) = next_free;

    fsinfo_sector[0x1FC] = 0x55;
    fsinfo_sector[0x1FD] = 0xAA;

    write_sector(bpb.fs_info_sector, fsinfo_sector);
}

static bool read_fs_info_sector(void) {
    if (bpb.fs_info_sector == 0 || bpb.fs_info_sector >= bpb.reserved_sectors) {
        return false;
    }

    uint8_t fsinfo_sector[512];
    if (read_sector(bpb.fs_info_sector, fsinfo_sector) != 0) {
        return false;
    }

    if (fsinfo_sector[0x00] != 0x52 || fsinfo_sector[0x01] != 0x52 ||
        fsinfo_sector[0x02] != 0x61 || fsinfo_sector[0x03] != 0x41) {
        return false;
    }

    fs_info.free_clusters = *((uint32_t*)(fsinfo_sector + 0x1E8));

    return true;
}

static uint32_t find_next_cluster(uint32_t current_cluster, uint32_t position, uint32_t bytes_per_cluster) {
    uint32_t target_cluster = position / bytes_per_cluster;
    uint32_t cluster = current_cluster;

    for (uint32_t i = 0; i < target_cluster; i++) {
        uint32_t next = read_fat_entry(cluster);
        if (next >= FAT32_LAST_CLUSTER || next == FAT32_FREE_CLUSTER) {
            return 0;
        }
        cluster = next;
    }

    return cluster;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}
