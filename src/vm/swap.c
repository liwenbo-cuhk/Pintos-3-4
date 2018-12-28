#include <bitmap.h>
#include "vm/swap.h"

void virtual_memory_init(void){
  // Initialize the swap disk
  block_swap = block_get_role(BLOCK_SWAP);
  if(!block_swap)  NOT_REACHED ();
  swap_size = block_size(block_swap) / SECTORS_PER_PAGE;
  avai_swap = bitmap_create(swap_size);
  bitmap_set_all(avai_swap, true);
}

void virtual_memory_swap(int index_to_swap){
  bitmap_set(avai_swap, index_to_swap, true);
}

int virtual_memory_swap_out(void *page){
  // THe aviable region for using
  size_t index_to_swap = bitmap_scan (avai_swap, /*start*/0, /*cnt*/1, true);
  for (size_t i = 0; i < SECTORS_PER_PAGE; ++i){
    block_write(block_swap, index_to_swap *SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set(avai_swap, index_to_swap, false);
  return index_to_swap;
}


void virtual_memory_swap_in(int index_to_swap, void *page){
  for (size_t i = 0; i < SECTORS_PER_PAGE; ++i){
    block_read (block_swap, index_to_swap * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set(avai_swap, index_to_swap, true);
}