#include "kernel.h"
#include "cstd.h"
#include "serial.h"

#pragma pack(push, 1)
typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_mem_desc_t;
#pragma pack(pop)

static uint8_t* bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t bitmap_size;

#define SET_BIT(i) (bitmap[(i)/8] |= (1 << ((i)%8)))
#define CLEAR_BIT(i) (bitmap[(i)/8] &= ~(1 << ((i)%8)))
#define TEST_BIT(i) (bitmap[(i)/8] & (1 << ((i)%8)))

static bool pmm_is_address_valid(uint64_t addr) {
    if (addr == 0 || addr == 0xFFFFFFFFFFFFFFFF) return false;
    if (addr > 0x0000008000000000) return false; 
    return true;
}

void pmm_init(void* mmap, size_t mmap_size, size_t desc_size) {
    if (mmap == NULL || mmap_size == 0 || desc_size < sizeof(efi_mem_desc_t)) {
        serial_puts("FATAL: PMM received invalid MMap parameters!\n");
        while(1);
    }

    uint64_t max_addr = 0;
    size_t desc_count = mmap_size / desc_size;
    uint8_t* mmap_ptr = (uint8_t*)mmap;

    for (size_t i = 0; i < desc_count; i++) {
        efi_mem_desc_t* d = (efi_mem_desc_t*)(mmap_ptr + (i * desc_size));
        
        if (d->number_of_pages == 0) continue;
        if (d->physical_start == 0xFFFFFFFFFFFFFFFF) continue;

        uint64_t end = d->physical_start + (d->number_of_pages * 4096);
        if (end > max_addr) max_addr = end;
    }

    if (max_addr == 0) {
        serial_puts("FATAL: Could not determine max RAM address.\n");
        while(1);
    }

    total_pages = max_addr / 4096;
    bitmap_size = (total_pages + 7) / 8;

    bitmap = NULL;
    for (size_t i = 0; i < desc_count; i++) {
        efi_mem_desc_t* d = (efi_mem_desc_t*)(mmap_ptr + (i * desc_size));
        if (d->type == 7 && (d->number_of_pages * 4096) >= bitmap_size) {
            if (d->physical_start >= 0x1000000 && d->physical_start < max_addr) {
                bitmap = (uint8_t*)d->physical_start;
                break;
            }
        }
    }

    if (bitmap == NULL) {
        serial_puts("PMM Error: Failed to find safe memory for bitmap (>=16MB)!\n");
        while(1);
    }

    memset(bitmap, 0xFF, bitmap_size);

    free_pages = 0;
    for (size_t i = 0; i < desc_count; i++) {
        efi_mem_desc_t* d = (efi_mem_desc_t*)(mmap_ptr + (i * desc_size));
        if (d->type == 7) { 
            uint64_t start_page = d->physical_start / 4096;
            for (uint64_t j = 0; j < d->number_of_pages; j++) {
                if (start_page + j < total_pages) {
                    CLEAR_BIT(start_page + j);
                    free_pages++;
                }
            }
        }
    }

    uint64_t kernel_safety_pages = 0x1000000 / 4096; 
    for (uint64_t i = 0; i < kernel_safety_pages && i < total_pages; i++) {
        if (!TEST_BIT(i)) {
            SET_BIT(i);
            free_pages--;
        }
    }

    uint64_t bitmap_start_page = (uint64_t)bitmap / 4096;
    uint64_t bitmap_pages = (bitmap_size + 4095) / 4096;
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        if (bitmap_start_page + i < total_pages) {
            if (!TEST_BIT(bitmap_start_page + i)) {
                SET_BIT(bitmap_start_page + i);
                free_pages--;
            }
        }
    }

    serial_puts("PMM Initialized.\nBitmap at: 0x");
    serial_puthex64((uint64_t)bitmap);
    serial_puts("\nFree pages: ");
    serial_putdec64(free_pages);
    serial_puts("\n");
}

void* pmm_alloc_page() {
    uint64_t start_search = 0x1000000 / 4096;
    for (uint64_t i = start_search; i < total_pages; i++) {
        if (!TEST_BIT(i)) {
            uint64_t addr = i * 4096;
            if (!pmm_is_address_valid(addr)) continue;

            SET_BIT(i);
            free_pages--;
            return (void*)addr;
        }
    }
    return NULL;
}

void pmm_free_page(void* addr) {
    uint64_t page_index = (uint64_t)addr / 4096;
    if (page_index >= (0x1000000 / 4096) && page_index < total_pages) {
        if (TEST_BIT(page_index)) {
            CLEAR_BIT(page_index);
            free_pages++;
        }
    }
}