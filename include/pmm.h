#ifndef PMM_H
#define PMM_H

#include "cstd.h"

#pragma pack(1)
typedef struct
{
    uint32_t type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

void pmm_init(void *mmap, size_t mmap_size, size_t desc_size);
void *pmm_alloc_page();
void pmm_free_page(void *addr);
uint64_t pmm_get_total_memory();

#endif