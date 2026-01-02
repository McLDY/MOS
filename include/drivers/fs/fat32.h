// include/drivers/fs/fat32.h
#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FS_FAT32 0x0C

// FAT32 BIOS Parameter Block (BPB)
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved_ebpb[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat32_bpb_t;



typedef struct __attribute__((packed)) {
    char        name[11];
    uint8_t     attributes;
    uint8_t     reserved;
    uint8_t     creation_time_tenths;
    uint16_t    creation_time;
    uint16_t    creation_date;
    uint16_t    last_access_date;
    uint16_t    cluster_high;
    uint16_t    last_write_time;
    uint16_t    last_write_date;
    uint16_t    cluster_low;
    uint32_t    file_size;
} fat32_dir_entry_t;

// FAT32 File/Directory Handle
typedef struct {
    uint32_t    first_cluster;         // First cluster of file/directory
    uint32_t    current_cluster;
    uint32_t    current_offset;
    uint32_t    file_size;
    uint32_t    position;              // Current read/write position
    bool        is_directory;          // True if this is a directory
    bool        is_open;               // True if handle is open
    uint8_t     buffer[512];
    uint32_t    buffer_sector;
    bool        buffer_dirty;

    uint32_t    dir_sector;
    uint32_t    dir_offset;
} fat32_handle_t;

typedef struct {
    uint32_t    sectors_per_cluster;
    uint32_t    bytes_per_sector;
    uint32_t    total_sectors;
    uint32_t    fat_start_sector;
    uint32_t    fat_sectors;
    uint32_t    data_start_sector;
    uint32_t    root_dir_cluster;
    uint32_t    total_clusters;
    uint32_t    free_clusters;
    uint8_t     fat_number;            // Which FAT to use (usually 0)
} fat32_info_t;

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F

#define FAT32_FREE_CLUSTER     0x00000000
#define FAT32_RESERVED_CLUSTER 0x00000001
#define FAT32_BAD_CLUSTER      0x0FFFFFF7
#define FAT32_LAST_CLUSTER     0x0FFFFFF8
#define FAT32_CLUSTER_MASK     0x0FFFFFFF

typedef enum {
    FILE_READ,
    FILE_WRITE,
    FILE_APPEND,
    FILE_CREATE
} file_mode_t;

bool fat32_init(uint32_t partition_start);
bool fat32_mount(uint32_t partition_start);
void fat32_umount(void);
bool fat32_format(uint32_t partition_start, const char* volume_label);
bool fat32_check(void);
uint32_t fat32_get_free_space(void);
uint32_t fat32_get_total_space(void);
const char* fat32_get_volume_label(void);
bool fat32_set_volume_label(const char* label);

bool fat32_open(const char* path, fat32_handle_t* handle, file_mode_t mode);
bool fat32_open_root(fat32_handle_t* handle);
bool fat32_read(fat32_handle_t* handle, void* buffer, uint32_t size);
bool fat32_write(fat32_handle_t* handle, const void* buffer, uint32_t size);
bool fat32_seek(fat32_handle_t* handle, uint32_t position);
bool fat32_truncate(fat32_handle_t* handle, uint32_t new_size);
void fat32_close(fat32_handle_t* handle);

bool fat32_create_dir(const char* path);
bool fat32_remove_dir(const char* path);
bool fat32_read_dir(fat32_handle_t* dir_handle, fat32_dir_entry_t* entry);
bool fat32_find_file(const char* path, fat32_dir_entry_t* entry);

bool fat32_create_file(const char* path);
bool fat32_delete_file(const char* path);
bool fat32_rename(const char* old_path, const char* new_path);
bool fat32_copy(const char* src_path, const char* dst_path);
bool fat32_move(const char* src_path, const char* dst_path);
bool fat32_file_exists(const char* path);
uint32_t fat32_get_file_size(const char* path);
bool fat32_get_file_info(const char* path, fat32_dir_entry_t* info);
bool fat32_set_file_attributes(const char* path, uint8_t attributes);

const char* fat32_get_error(void);
bool fat32_mounted(void);
bool fat32_format_check(void);
void fat32_print_info(void);

// Disk I/O functions
uint32_t fat32_read_sector(uint32_t sector, void* buffer);
uint32_t fat32_write_sector(uint32_t sector, const void* buffer);

int toupper(int c);

#endif
