/*
                   _ooOoo_
                  o8888888o
                  88" . "88
                  (| -_- |)
                  O\  =  /O
               ____/`---'\____
             .'  \\|     |//  `.
            /  \\|||  :  |||//  \
           /  _||||| -:- |||||-  \
           |   | \\\  -  /// |   |
           | \_|  ''\---/''  |   |
           \  .-\__  `-`  ___/-. /
         ___`. .'  /--.--\  `. . __
      ."" '<  `.___\_<|>_/___.'  >'"".
     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
     \  \ `-.   \_ __\ /__ _/   .-` /  /
======`-.____`-.___\_____/___.-`____.-'======
                   `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            佛祖保佑       永无BUG
*/

#include "drivers/fs/fat16.h"
#include "drivers/ide.h"
#include "serial.h"
#include "string.h"
#include "memory.h"

static Fat16FS g_fs;
static uint8_t g_mounted = 0;
static uint8_t* g_sector_buffer = NULL;

// 辅助函数：读取扇区
int _fat16_read_sector(uint32_t lba, void* buffer) {
    return ide_read_sectors(lba, 1, buffer);
}

// 辅助函数：写入扇区  
int _fat16_write_sector(uint32_t lba, const void* buffer) {
    return ide_write_sectors(lba, 1, (void*)buffer);
}

// 初始化FAT16模块
int fat16_init(void) {
    serial_puts("Initializing FAT16 filesystem module...\n");
    
    // 分配扇区缓冲区（4KB对齐）
    g_sector_buffer = (uint8_t*)kmalloc(4096);
    if (!g_sector_buffer) {
        serial_puts("Failed to allocate sector buffer\n");
        return -1;
    }
    
    memset(&g_fs, 0, sizeof(Fat16FS));
    g_mounted = 0;
    
    serial_puts("FAT16 module initialized\n");
    return 0;
}

// 挂载FAT16分区
int fat16_mount(uint8_t drive) {
    if (g_mounted) {
        serial_puts("Already mounted, unmount first\n");
        return -1;
    }
    
    serial_puts("Mounting FAT16 filesystem...\n");
    
    // 读取MBR
    MBR mbr;
    if (_fat16_read_sector(0, &mbr) != 0) {
        serial_puts("Failed to read MBR\n");
        return -1;
    }
    
    // 检查MBR签名
    if (mbr.signature != 0xAA55 && mbr.signature != 0x55AA) {
        serial_puts("Invalid MBR signature\n");
        return -1;
    }
    
    // 查找FAT16分区
    int found = 0;
    for (int i = 0; i < 4; i++) {
        if (mbr.partitions[i].type == FAT16_PARTITION_TYPE || 
            mbr.partitions[i].type == FAT16B_PARTITION_TYPE) {
            g_fs.partition_lba = mbr.partitions[i].lba_start;
            serial_puts("Found FAT16 partition at LBA: ");
            serial_putdec64(g_fs.partition_lba);
            serial_puts("\n");
            found = 1;
            break;
        }
    }
    
    if (!found) {
        serial_puts("No FAT16 partition found\n");
        return -1;
    }
    
    // 读取引导扇区
    Fat16BootSector bs;
    if (_fat16_read_sector(g_fs.partition_lba, &bs) != 0) {
        serial_puts("Failed to read boot sector\n");
        return -1;
    }
    
    // 检查引导扇区签名
    // 修改签名检查，更灵活一些
    uint8_t* bs_bytes = (uint8_t*)&bs;
    uint16_t signature = (bs_bytes[511] << 8) | bs_bytes[510];  // 手动组合
    
    if (signature != 0xAA55) {
        // 尝试另一种字节序
        signature = (bs_bytes[510] << 8) | bs_bytes[511];
        if (signature != 0xAA55) {
            serial_puts("Invalid boot sector signature: 0x");
            char hex[5];
            hex[0] = "0123456789ABCDEF"[(signature >> 12) & 0xF];
            hex[1] = "0123456789ABCDEF"[(signature >> 8) & 0xF];
            hex[2] = "0123456789ABCDEF"[(signature >> 4) & 0xF];
            hex[3] = "0123456789ABCDEF"[signature & 0xF];
            hex[4] = 0;
            serial_puts(hex);
            serial_puts("\n");
            return -1;
        }
    }
    
    // 填充文件系统信息
    g_fs.bytes_per_sector = bs.bytes_per_sector;
    g_fs.sectors_per_cluster = bs.sectors_per_cluster;
    g_fs.reserved_sectors = bs.reserved_sectors;
    g_fs.fat_count = bs.fat_count;
    g_fs.root_entries = bs.root_entries;
    g_fs.sectors_per_fat = bs.sectors_per_fat_16;
    
    // 计算总扇区数
    if (bs.total_sectors_16 != 0) {
        g_fs.total_sectors = bs.total_sectors_16;
    } else {
        g_fs.total_sectors = bs.total_sectors_32;
    }
    
    // 计算关键偏移
    g_fs.fat_lba = g_fs.partition_lba + g_fs.reserved_sectors;
    g_fs.root_lba = g_fs.fat_lba + (g_fs.fat_count * g_fs.sectors_per_fat);
    g_fs.root_sectors = (g_fs.root_entries * 32 + g_fs.bytes_per_sector - 1) / g_fs.bytes_per_sector;
    g_fs.data_lba = g_fs.root_lba + g_fs.root_sectors;
    
    // 计算总簇数
    uint32_t data_sectors = g_fs.total_sectors - g_fs.reserved_sectors - 
                           (g_fs.fat_count * g_fs.sectors_per_fat) - g_fs.root_sectors;
    g_fs.total_clusters = data_sectors / g_fs.sectors_per_cluster;
    
    // 复制卷标和文件系统类型
    memcpy(g_fs.volume_label, bs.volume_label, 11);
    g_fs.volume_label[11] = '\0';
    memcpy(g_fs.fs_type, bs.fs_type, 8);
    g_fs.fs_type[8] = '\0';
    
    // 打印文件系统信息
    fat16_print_info();
    
    g_fs.initialized = 1;
    g_mounted = 1;
    
    serial_puts("FAT16 filesystem mounted successfully\n");
    return 0;
}

// 卸载文件系统
int fat16_unmount(void) {
    if (!g_mounted) {
        return 0;
    }
    
    // 刷新所有缓存（如果有的话）
    memset(&g_fs, 0, sizeof(Fat16FS));
    g_mounted = 0;
    
    serial_puts("FAT16 filesystem unmounted\n");
    return 0;
}

// 打印文件系统信息
void fat16_print_info(void) {
    serial_puts("\n=== FAT16 Filesystem Info ===\n");
    serial_puts("Volume Label: ");
    serial_puts(g_fs.volume_label);
    serial_puts("\nFS Type: ");
    serial_puts(g_fs.fs_type);
    serial_puts("\nBytes per Sector: ");
    serial_putdec64(g_fs.bytes_per_sector);
    serial_puts("\nSectors per Cluster: ");
    serial_putdec64(g_fs.sectors_per_cluster);
    serial_puts("\nReserved Sectors: ");
    serial_putdec64(g_fs.reserved_sectors);
    serial_puts("\nFAT Count: ");
    serial_putdec64(g_fs.fat_count);
    serial_puts("\nRoot Entries: ");
    serial_putdec64(g_fs.root_entries);
    serial_puts("\nSectors per FAT: ");
    serial_putdec64(g_fs.sectors_per_fat);
    serial_puts("\nTotal Sectors: ");
    serial_putdec64(g_fs.total_sectors);
    serial_puts("\nTotal Clusters: ");
    serial_putdec64(g_fs.total_clusters);
    serial_puts("\nFAT LBA: ");
    serial_putdec64(g_fs.fat_lba);
    serial_puts("\nRoot LBA: ");
    serial_putdec64(g_fs.root_lba);
    serial_puts("\nData LBA: ");
    serial_putdec64(g_fs.data_lba);
    serial_puts("\n============================\n");
}

// 簇号转换为LBA地址
uint32_t _fat16_cluster_to_lba(Fat16FS* fs, uint32_t cluster) {
    if (cluster < 2) {
        return 0; // 簇0和簇1是保留的
    }
    return fs->data_lba + (cluster - 2) * fs->sectors_per_cluster;
}

// 获取FAT表项
uint16_t _fat16_get_fat_entry(Fat16FS* fs, uint32_t cluster) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        return FAT_CLUSTER_BAD;
    }
    
    // 计算FAT表项所在的扇区
    uint32_t fat_offset = cluster * 2; // 每个FAT16表项2字节
    uint32_t fat_sector = fat_offset / fs->bytes_per_sector;
    uint32_t ent_offset = fat_offset % fs->bytes_per_sector;
    
    // 读取FAT扇区
    if (_fat16_read_sector(fs->fat_lba + fat_sector, g_sector_buffer) != 0) {
        return FAT_CLUSTER_BAD;
    }
    
    // 读取16位FAT表项
    uint16_t entry = *(uint16_t*)(g_sector_buffer + ent_offset);
    return entry;
}

// 设置FAT表项
int _fat16_set_fat_entry(Fat16FS* fs, uint32_t cluster, uint16_t value) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        return -1;
    }
    
    // 计算FAT表项所在的扇区
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_offset / fs->bytes_per_sector;
    uint32_t ent_offset = fat_offset % fs->bytes_per_sector;
    
    // 读取FAT扇区
    if (_fat16_read_sector(fs->fat_lba + fat_sector, g_sector_buffer) != 0) {
        return -1;
    }
    
    // 设置FAT表项
    *(uint16_t*)(g_sector_buffer + ent_offset) = value;
    
    // 写回扇区（写入所有FAT副本）
    for (int i = 0; i < fs->fat_count; i++) {
        if (_fat16_write_sector(fs->fat_lba + fat_sector + i * fs->sectors_per_fat, g_sector_buffer) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// 查找空闲簇
uint32_t _fat16_find_free_cluster(Fat16FS* fs) {
    // 从簇2开始搜索
    for (uint32_t cluster = 2; cluster < fs->total_clusters + 2; cluster++) {
        uint16_t entry = _fat16_get_fat_entry(fs, cluster);
        if (entry == FAT_CLUSTER_FREE) {
            return cluster;
        }
    }
    return 0; // 没有空闲簇
}

// 分配簇链
int _fat16_allocate_cluster_chain(Fat16FS* fs, uint32_t start_cluster, uint32_t count) {
    if (count == 0) return 0;
    
    uint32_t current = start_cluster;
    for (uint32_t i = 0; i < count - 1; i++) {
        // 查找下一个空闲簇
        uint32_t next = _fat16_find_free_cluster(fs);
        if (next == 0) {
            // 回滚已分配的簇
            _fat16_free_cluster_chain(fs, start_cluster);
            return -1;
        }
        
        // 设置当前簇指向下一个簇
        if (_fat16_set_fat_entry(fs, current, next) != 0) {
            _fat16_free_cluster_chain(fs, start_cluster);
            return -1;
        }
        
        current = next;
    }
    
    // 设置最后一个簇为结束标志
    if (_fat16_set_fat_entry(fs, current, FAT_CLUSTER_LAST_MIN) != 0) {
        _fat16_free_cluster_chain(fs, start_cluster);
        return -1;
    }
    
    return 0;
}

// 释放簇链
int _fat16_free_cluster_chain(Fat16FS* fs, uint32_t start_cluster) {
    uint32_t current = start_cluster;
    
    while (current >= 2 && current < fs->total_clusters + 2) {
        uint16_t next = _fat16_get_fat_entry(fs, current);
        
        // 标记当前簇为空闲
        if (_fat16_set_fat_entry(fs, current, FAT_CLUSTER_FREE) != 0) {
            return -1;
        }
        
        if (IS_LAST_CLUSTER(next)) {
            break;
        }
        
        current = next;
    }
    
    return 0;
}

// 规范化8.3格式文件名
static void _normalize_83_name(const char* src, char* name, char* ext) {
    memset(name, ' ', 8);
    memset(ext, ' ', 3);
    
    const char* dot = strchr(src, '.');
    if (dot == NULL) {
        // 没有扩展名
        int len = strlen(src);
        if (len > 8) len = 8;
        memcpy(name, src, len);
    } else {
        // 有扩展名
        int name_len = dot - src;
        if (name_len > 8) name_len = 8;
        memcpy(name, src, name_len);
        
        const char* ext_start = dot + 1;
        int ext_len = strlen(ext_start);
        if (ext_len > 3) ext_len = 3;
        memcpy(ext, ext_start, ext_len);
    }
    
    // 转换为大写
    for (int i = 0; i < 8; i++) {
        if (name[i] >= 'a' && name[i] <= 'z') {
            name[i] = name[i] - 'a' + 'A';
        }
    }
    for (int i = 0; i < 3; i++) {
        if (ext[i] >= 'a' && ext[i] <= 'z') {
            ext[i] = ext[i] - 'a' + 'A';
        }
    }
}

// 查找文件或目录
int _fat16_find_file(Fat16FS* fs, const char* path, FindResult* result) {
    memset(result, 0, sizeof(FindResult));
    
    // 分割路径和文件名
    char search_name[13];
    uint32_t dir_cluster = 0; // 0表示根目录
    
    // 如果是根目录下的文件
    const char* slash = strchr(path, '/');
    if (slash == NULL || slash == path) {
        // 直接查找根目录
        if (slash == path) {
            // 路径是"/"
            result->found = 1;
            result->cluster = 0;
            return 0;
        }
        
        // 规范化文件名
        char name83[8], ext83[3];
        _normalize_83_name(path, name83, ext83);
        
        // 在根目录中查找
        uint32_t entries_per_sector = fs->bytes_per_sector / 32;
        
        for (uint32_t sector = 0; sector < fs->root_sectors; sector++) {
            if (_fat16_read_sector(fs->root_lba + sector, g_sector_buffer) != 0) {
                return -1;
            }
            
            DirEntry* entries = (DirEntry*)g_sector_buffer;
            for (uint32_t i = 0; i < entries_per_sector; i++) {
                DirEntry* entry = &entries[i];
                
                // 检查目录项是否有效
                if ((uint8_t)entry->name[0] == DIR_ENTRY_FREE) {
                    continue;
                }
                if (entry->name[0] == DIR_ENTRY_END) {
                    break;
                }
                if (entry->attr == ATTR_LONG_NAME) {
                    continue; // 跳过长文件名条目
                }
                
                // 比较文件名
                if (memcmp(entry->name, name83, 8) == 0 && 
                    memcmp(entry->ext, ext83, 3) == 0) {
                    
                    result->found = 1;
                    result->cluster = entry->cluster_low;
                    result->dir_sector = fs->root_lba + sector;
                    result->dir_index = i;
                    return 0;
                }
            }
        }
    } else {
        // 处理子目录（简化版，暂不支持多级目录）
        // 你可以扩展这个函数来支持多级目录
        return -1;
    }
    
    return 0;
}

// 打开文件
int fat16_open(const char* path, Fat16File* file, uint8_t mode) {
    if (!g_mounted) {
        return -1;
    }
    
    FindResult result;
    if (_fat16_find_file(&g_fs, path, &result) != 0) {
        return -1;
    }
    
    if (!result.found) {
        return -2; // 文件不存在
    }
    
    memset(file, 0, sizeof(Fat16File));
    
    // 获取文件信息
    DirEntry entry;
    uint32_t sector_offset = result.dir_sector - g_fs.partition_lba;
    if (_fat16_read_sector(result.dir_sector, g_sector_buffer) != 0) {
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)g_sector_buffer;
    entry = entries[result.dir_index];
    
    // 填充文件句柄
    file->first_cluster = entry.cluster_low;
    file->current_cluster = entry.cluster_low;
    file->file_size = entry.file_size;
    file->position = 0;
    file->sector_offset = 0;
    file->is_directory = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;
    file->mode = mode;
    
    return 0;
}

// 关闭文件
int fat16_close(Fat16File* file) {
    // 目前没有需要清理的资源
    (void)file;
    return 0;
}

// 读取文件
int fat16_read(Fat16File* file, void* buffer, uint32_t size) {
    if (!g_mounted || !file) {
        return -1;
    }
    
    if (file->position >= file->file_size) {
        return 0; // 已到文件末尾
    }
    
    // 调整读取大小
    uint32_t remaining = file->file_size - file->position;
    if (size > remaining) {
        size = remaining;
    }
    
    uint8_t* dest = (uint8_t*)buffer;
    uint32_t total_read = 0;
    
    while (total_read < size) {
        // 计算当前簇的LBA
        uint32_t cluster_lba = _fat16_cluster_to_lba(&g_fs, file->current_cluster);
        if (cluster_lba == 0) {
            break; // 无效簇
        }
        
        // 计算在当前簇内的偏移
        uint32_t offset_in_cluster = file->sector_offset * g_fs.bytes_per_sector;
        uint32_t cluster_remaining = g_fs.sectors_per_cluster * g_fs.bytes_per_sector - offset_in_cluster;
        
        // 计算本次读取的大小
        uint32_t to_read = size - total_read;
        if (to_read > cluster_remaining) {
            to_read = cluster_remaining;
        }
        
        // 读取一个或多个扇区
        uint32_t sector = cluster_lba + file->sector_offset;
        uint32_t sector_count = (to_read + g_fs.bytes_per_sector - 1) / g_fs.bytes_per_sector;
        
        // 直接读取到目标缓冲区
        if (ide_read_sectors(sector, sector_count, dest + total_read) != 0) {
            return -1;
        }
        
        total_read += to_read;
        file->position += to_read;
        file->sector_offset += sector_count;
        
        // 如果已经读完当前簇，移动到下一个簇
        if (file->sector_offset >= g_fs.sectors_per_cluster) {
            file->sector_offset = 0;
            file->current_cluster = _fat16_get_fat_entry(&g_fs, file->current_cluster);
            
            if (IS_LAST_CLUSTER(file->current_cluster)) {
                break; // 文件结束
            }
        }
    }
    
    return total_read;
}

// 定位文件指针
int fat16_seek(Fat16File* file, uint32_t offset) {
    if (!g_mounted || !file || offset > file->file_size) {
        return -1;
    }
    
    if (offset == file->position) {
        return 0; // 已经在目标位置
    }
    
    // 从文件开始重新定位
    if (offset < file->position) {
        file->current_cluster = file->first_cluster;
        file->position = 0;
        file->sector_offset = 0;
    }
    
    // 计算需要跳过的字节数
    uint32_t to_skip = offset - file->position;
    
    // 跳过簇
    while (to_skip > 0) {
        uint32_t cluster_size = g_fs.sectors_per_cluster * g_fs.bytes_per_sector;
        uint32_t remaining_in_cluster = cluster_size - file->sector_offset * g_fs.bytes_per_sector;
        
        if (to_skip >= remaining_in_cluster) {
            // 跳过整个簇
            to_skip -= remaining_in_cluster;
            file->position += remaining_in_cluster;
            file->sector_offset = 0;
            
            // 获取下一个簇
            file->current_cluster = _fat16_get_fat_entry(&g_fs, file->current_cluster);
            if (IS_LAST_CLUSTER(file->current_cluster) && to_skip > 0) {
                return -1; // 超出了文件大小
            }
        } else {
            // 在当前簇内跳转
            file->sector_offset += to_skip / g_fs.bytes_per_sector;
            file->position += to_skip;
            to_skip = 0;
        }
    }
    
    return 0;
}

// 列出目录
int fat16_list(const char* path, FileInfo* list, uint32_t max_count) {
    if (!g_mounted) {
        return -1;
    }
    
    FindResult result;
    if (_fat16_find_file(&g_fs, path, &result) != 0) {
        return -1;
    }
    
    if (!result.found) {
        return -2;
    }
    
    uint32_t dir_cluster = result.cluster;
    uint32_t count = 0;
    
    // 读取目录内容
    if (dir_cluster == 0) {
        // 根目录
        uint32_t entries_per_sector = g_fs.bytes_per_sector / 32;
        
        for (uint32_t sector = 0; sector < g_fs.root_sectors && count < max_count; sector++) {
            if (_fat16_read_sector(g_fs.root_lba + sector, g_sector_buffer) != 0) {
                return -1;
            }
            
            DirEntry* entries = (DirEntry*)g_sector_buffer;
            for (uint32_t i = 0; i < entries_per_sector && count < max_count; i++) {
                DirEntry* entry = &entries[i];
                
                // 跳过无效条目
                if ((uint8_t)entry->name[0] == DIR_ENTRY_FREE || (uint8_t)entry->name[0] == DIR_ENTRY_END) {    
                    continue;
                }
                if (entry->attr == ATTR_LONG_NAME) {
                    continue;
                }
                if (entry->attr == ATTR_VOLUME_ID) {
                    continue; // 跳过卷标
                }
                
                // 填充FileInfo结构
                FileInfo* info = &list[count];
                
                // 转换8.3格式为普通文件名
                char name83[9], ext83[4];
                memcpy(name83, entry->name, 8);
                name83[8] = '\0';
                memcpy(ext83, entry->ext, 3);
                ext83[3] = '\0';
                
                // 去除尾部空格
                for (int j = 7; j >= 0 && name83[j] == ' '; j--) {
                    name83[j] = '\0';
                }
                for (int j = 2; j >= 0 && ext83[j] == ' '; j--) {
                    ext83[j] = '\0';
                }
                
                // 组合文件名
                if (ext83[0] == '\0') {
                    sprintf(info->name, "%s", name83);
                } else {
                    sprintf(info->name, "%s.%s", name83, ext83);
                }
                
                info->size = entry->file_size;
                info->cluster = entry->cluster_low;
                info->attr = entry->attr;
                info->is_dir = (entry->attr & ATTR_DIRECTORY) ? 1 : 0;
                info->is_hidden = (entry->attr & ATTR_HIDDEN) ? 1 : 0;
                info->is_system = (entry->attr & ATTR_SYSTEM) ? 1 : 0;
                info->is_readonly = (entry->attr & ATTR_READ_ONLY) ? 1 : 0;
                info->create_date = entry->create_date;
                info->create_time = entry->create_time;
                info->modify_date = entry->modify_date;
                info->modify_time = entry->modify_time;
                
                count++;
            }
        }
    } else {
        // 子目录（简化版）
        // 需要读取簇链中的所有扇区
        uint32_t current_cluster = dir_cluster;
        uint32_t entries_per_sector = g_fs.bytes_per_sector / 32;
        
        while (current_cluster >= 2 && count < max_count) {
            uint32_t cluster_lba = _fat16_cluster_to_lba(&g_fs, current_cluster);
            
            // 读取簇中的所有扇区
            for (uint32_t sector = 0; sector < g_fs.sectors_per_cluster && count < max_count; sector++) {
                if (_fat16_read_sector(cluster_lba + sector, g_sector_buffer) != 0) {
                    return -1;
                }
                
                DirEntry* entries = (DirEntry*)g_sector_buffer;
                for (uint32_t i = 0; i < entries_per_sector && count < max_count; i++) {
                    DirEntry* entry = &entries[i];
                    
                    // 跳过无效条目（同上）
                    if ((uint8_t)entry->name[0] == DIR_ENTRY_FREE || (uint8_t)entry->name[0] == DIR_ENTRY_END) {
                        continue;
                    }
                    if (entry->attr == ATTR_LONG_NAME) {
                        continue;
                    }
                    if (entry->attr == ATTR_VOLUME_ID) {
                        continue; // 跳过卷标
                    }
                    
                    // 填充FileInfo结构
                    FileInfo* info = &list[count];
                    
                    // 转换8.3格式为普通文件名
                    char name83[9], ext83[4];
                    memcpy(name83, entry->name, 8);
                    name83[8] = '\0';
                    memcpy(ext83, entry->ext, 3);
                    ext83[3] = '\0';
                    
                    // 去除尾部空格
                    for (int j = 7; j >= 0 && name83[j] == ' '; j--) {
                        name83[j] = '\0';
                    }
                    for (int j = 2; j >= 0 && ext83[j] == ' '; j--) {
                        ext83[j] = '\0';
                    }
                    
                    // 组合文件名
                    if (ext83[0] == '\0') {
                        sprintf(info->name, "%s", name83);
                    } else {
                        sprintf(info->name, "%s.%s", name83, ext83);
                    }
                    
                    info->size = entry->file_size;
                    info->cluster = entry->cluster_low;
                    info->attr = entry->attr;
                    info->is_dir = (entry->attr & ATTR_DIRECTORY) ? 1 : 0;
                    info->is_hidden = (entry->attr & ATTR_HIDDEN) ? 1 : 0;
                    info->is_system = (entry->attr & ATTR_SYSTEM) ? 1 : 0;
                    info->is_readonly = (entry->attr & ATTR_READ_ONLY) ? 1 : 0;
                    info->create_date = entry->create_date;
                    info->create_time = entry->create_time;
                    info->modify_date = entry->modify_date;
                    info->modify_time = entry->modify_time;
                    
                    count++;
                }
            }
            
            // 获取下一个簇
            current_cluster = _fat16_get_fat_entry(&g_fs, current_cluster);
            if (IS_LAST_CLUSTER(current_cluster)) {
                break;
            }
        }
    }
    
    return count;
}

// 获取文件信息
int fat16_get_info(const char* path, FileInfo* info) {
    if (!g_mounted) {
        return -1;
    }
    
    FindResult result;
    if (_fat16_find_file(&g_fs, path, &result) != 0) {
        return -1;
    }
    
    if (!result.found) {
        return -2;
    }
    
    // 读取目录项
    if (_fat16_read_sector(result.dir_sector, g_sector_buffer) != 0) {
        return -1;
    }
    
    DirEntry* entries = (DirEntry*)g_sector_buffer;
    DirEntry* entry = &entries[result.dir_index];
    
    // 转换8.3格式文件名
    char name83[9], ext83[4];
    memcpy(name83, entry->name, 8);
    name83[8] = '\0';
    memcpy(ext83, entry->ext, 3);
    ext83[3] = '\0';
    
    // 去除尾部空格
    for (int j = 7; j >= 0 && name83[j] == ' '; j--) {
        name83[j] = '\0';
    }
    for (int j = 2; j >= 0 && ext83[j] == ' '; j--) {
        ext83[j] = '\0';
    }
    
    // 组合文件名
    if (ext83[0] == '\0') {
        sprintf(info->name, "%s", name83);
    } else {
        sprintf(info->name, "%s.%s", name83, ext83);
    }
    
    info->size = entry->file_size;
    info->cluster = entry->cluster_low;
    info->attr = entry->attr;
    info->is_dir = (entry->attr & ATTR_DIRECTORY) ? 1 : 0;
    info->is_hidden = (entry->attr & ATTR_HIDDEN) ? 1 : 0;
    info->is_system = (entry->attr & ATTR_SYSTEM) ? 1 : 0;
    info->is_readonly = (entry->attr & ATTR_READ_ONLY) ? 1 : 0;
    info->create_date = entry->create_date;
    info->create_time = entry->create_time;
    info->modify_date = entry->modify_date;
    info->modify_time = entry->modify_time;
    
    return 0;
}

// 获取空闲空间
uint32_t fat16_get_free_space(void) {
    if (!g_mounted) {
        return 0;
    }
    
    uint32_t free_clusters = 0;
    
    for (uint32_t cluster = 2; cluster < g_fs.total_clusters + 2; cluster++) {
        uint16_t entry = _fat16_get_fat_entry(&g_fs, cluster);
        if (entry == FAT_CLUSTER_FREE) {
            free_clusters++;
        }
    }
    
    return free_clusters * g_fs.sectors_per_cluster * g_fs.bytes_per_sector;
}