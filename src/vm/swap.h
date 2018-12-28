#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "threads/vaddr.h"
#include "devices/block.h"

// THe biggest swap size
static size_t swap_size;
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static struct block *swap_block;
static struct bitmap *swap_available;

// Here are the following four functions for swap algorithm

// Read the information fo the swapped page, and store into page lastly
void vm_swap_in (int index_to_swap, void *page);
// let the swapped region be freed
void vm_swap_free (int index_to_swap);
// write the swapped page information in the disk, and return the region
// being placed
int vm_swap_out (void *page);
//Init the swap
void vm_swap_init (void);

#endif