#include <bitmap.h>
#include "vm/swap.h"

void vm_swap_init(void){
  // Initialize the swap disk
  swap_block = block_get_role(BLOCK_SWAP);
  if(!swap_block)  NOT_REACHED ();
  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_available = bitmap_create(swap_size);
  bitmap_set_all(swap_available, true);
}

void vm_swap_free(int index_to_swap){
  bitmap_set(swap_available, index_to_swap, true);
}

int vm_swap_out(void *page){
  // THe aviable region for using
  size_t index_to_swap = bitmap_scan (swap_available, /*start*/0, /*cnt*/1, true);
  for (size_t i = 0; i < SECTORS_PER_PAGE; ++i){
    block_write(swap_block, index_to_swap *SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set(swap_available, index_to_swap, false);
  return index_to_swap;
}


void vm_swap_in(int index_to_swap, void *page){
  for (size_t i = 0; i < SECTORS_PER_PAGE; ++i){
    block_read (swap_block, index_to_swap * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set(swap_available, index_to_swap, true);
}