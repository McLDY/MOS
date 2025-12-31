#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

// FAT16分区类型
#define FAT16_PARTITION_TYPE 0x06
#define FAT16B_PARTITION_TYPE 0x0E

// 文件属性
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F

// 特殊目录项值
#define DIR_ENTRY_FREE  0xE5
#define DIR_ENTRY_END   0x00

// FAT特殊簇值
#define FAT_CLUSTER_FREE     0x0000
#define FAT_CLUSTER_RESERVED 0x0001
#define FAT_CLUSTER_BAD      0xFFF7
#define FAT_CLUSTER_LAST_MIN 0xFFF8
#define FAT_CLUSTER_LAST_MAX 0xFFFF
#define FAT_CLUSTER_MASK     0x0FFF  // FAT12掩码
#define FAT_CLUSTER_MASK16   0xFFFF  // FAT16掩码

// 检查是否为簇链结束
#define IS_LAST_CLUSTER(cluster) ((cluster) >= FAT_CLUSTER_LAST_MIN && (cluster) <= FAT_CLUSTER_LAST_MAX)

// 最大文件名长度
#define MAX_FILENAME_LEN 256

// 分区表项结构 (MBR偏移0x1BE)
typedef struct {
    uint8_t  status;
    uint8_t  chs_start[3];
    uint8_t  type;
    uint8_t  chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) PartitionEntry;

// MBR结构
typedef struct {
    uint8_t boot_code[446];
    PartitionEntry partitions[4];
    uint16_t signature;  // 0x55AA
} __attribute__((packed)) MBR;

// FAT16引导扇区 (BPB)
// 在fat16.h中，修改Fat16BootSector结构体
typedef struct {
    // BIOS参数块 (BPB)
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
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    // 扩展BPB (FAT16)
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
    // 确保整个结构体是512字节
    uint8_t  boot_code[448];
    uint16_t boot_sign;  // 应该在偏移0x1FE处
} __attribute__((packed, aligned(1))) Fat16BootSector;  // 添加aligned(1)

// FAT目录项 (32字节)
typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) DirEntry;

// 文件句柄
typedef struct {
    uint32_t first_cluster;     // 起始簇号
    uint32_t current_cluster;   // 当前簇号
    uint32_t file_size;         // 文件大小
    uint32_t position;          // 当前读写位置
    uint32_t sector_offset;     // 在当前簇内的扇区偏移
    uint8_t  is_directory;      // 是否为目录
    uint8_t  mode;              // 打开模式
} Fat16File;

// 文件系统句柄
typedef struct {
    // 分区信息
    uint32_t partition_lba;     // 分区起始LBA
    
    // 从BPB读取的信息
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint32_t total_sectors;
    uint16_t sectors_per_fat;
    
    // 计算得到的关键偏移
    uint32_t fat_lba;           // FAT表起始LBA
    uint32_t root_lba;          // 根目录起始LBA
    uint32_t root_sectors;      // 根目录占用扇区数
    uint32_t data_lba;          // 数据区起始LBA
    uint32_t total_clusters;    // 总簇数
    
    // 状态信息
    uint8_t  initialized;
    char     volume_label[12];
    char     fs_type[9];
} Fat16FS;

// 查找结果
typedef struct {
    uint32_t cluster;      // 找到的簇号
    uint32_t dir_sector;   // 目录项所在扇区
    uint16_t dir_index;    // 目录项在扇区内的索引
    uint8_t  found;        // 是否找到
} FindResult;

// 目录项信息（便于使用）
typedef struct {
    char     name[13];     // 8.3格式文件名
    char     long_name[MAX_FILENAME_LEN]; // 长文件名（暂不支持）
    uint32_t size;
    uint32_t cluster;
    uint16_t attr;
    uint16_t create_date;
    uint16_t create_time;
    uint16_t modify_date;
    uint16_t modify_time;
    uint8_t  is_dir;
    uint8_t  is_hidden;
    uint8_t  is_system;
    uint8_t  is_readonly;
} FileInfo;

// 函数声明
int fat16_init(void);
int fat16_mount(uint8_t drive);
int fat16_unmount(void);
int fat16_open(const char* path, Fat16File* file, uint8_t mode);
int fat16_close(Fat16File* file);
int fat16_read(Fat16File* file, void* buffer, uint32_t size);
int fat16_write(Fat16File* file, const void* buffer, uint32_t size);
int fat16_seek(Fat16File* file, uint32_t offset);
int fat16_list(const char* path, FileInfo* list, uint32_t max_count);
int fat16_create(const char* path, uint8_t attr);
int fat16_delete(const char* path);
int fat16_mkdir(const char* path);
int fat16_rmdir(const char* path);
uint32_t fat16_get_free_space(void);
int fat16_get_info(const char* path, FileInfo* info);
void fat16_print_info(void);
// 在drivers/fs/fat16.h中添加
int fat16_seek(Fat16File *file, uint32_t offset);

// 内部函数
int _fat16_parse_boot_sector(Fat16FS* fs);
uint32_t _fat16_cluster_to_lba(Fat16FS* fs, uint32_t cluster);
uint16_t _fat16_get_fat_entry(Fat16FS* fs, uint32_t cluster);
int _fat16_set_fat_entry(Fat16FS* fs, uint32_t cluster, uint16_t value);
int _fat16_find_file(Fat16FS* fs, const char* path, FindResult* result);
int _fat16_read_sector(uint32_t lba, void* buffer);
int _fat16_write_sector(uint32_t lba, const void* buffer);
uint32_t _fat16_find_free_cluster(Fat16FS* fs);
int _fat16_allocate_cluster_chain(Fat16FS* fs, uint32_t start_cluster, uint32_t count);
int _fat16_free_cluster_chain(Fat16FS* fs, uint32_t start_cluster);
int _fat16_add_dir_entry(Fat16FS* fs, uint32_t dir_cluster, DirEntry* entry);
int _fat16_remove_dir_entry(Fat16FS* fs, uint32_t dir_cluster, const char* name);

#endif // FAT16_H