#ifndef IDE_H
#define IDE_H

#include <stdint.h>

#define IDE_DATA        0x1F0
#define IDE_ERROR       0x1F1
#define IDE_FEATURES    0x1F1
#define IDE_SECTOR_CNT  0x1F2
#define IDE_LBA_LOW     0x1F3
#define IDE_LBA_MID     0x1F4
#define IDE_LBA_HIGH    0x1F5
#define IDE_DRIVE_HEAD  0x1F6
#define IDE_STATUS      0x1F7
#define IDE_COMMAND     0x1F7
#define IDE_ALT_STATUS  0x3F6
#define IDE_DEV_CTRL    0x3F6

#define IDE_STATUS_ERR  0x01
#define IDE_STATUS_DRQ  0x08
#define IDE_STATUS_SRV  0x10
#define IDE_STATUS_DF   0x20
#define IDE_STATUS_RDY  0x40
#define IDE_STATUS_BSY  0x80

#define IDE_CMD_READ    0x20
#define IDE_CMD_WRITE   0x30
#define IDE_CMD_IDENT   0xEC
#define IDE_CMD_FLUSH   0xE7

#define IDE_MASTER      0xE0
#define IDE_SLAVE       0xF0
#define IDE_LBA_MODE    0x40

#define defult_device   0xF0

void ide_init(void);
int ide_read_sectors(uint32_t lba, uint8_t num_sectors, void* buffer);
int ide_write_sectors(uint32_t lba, uint8_t num_sectors, void* buffer);
void ide_identify(void);
uint8_t ide_polling(uint8_t check_error);
void ide_wait(void);
uint8_t ide_check_drive(void);
int ide_wait_ready(void);
int ide_wait_drq(void);

#endif
