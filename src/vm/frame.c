#include "userprog/pagedir.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "threads/vaddr.h"
#include <hash.h>
#include <list.h>
#include <stdio.h>
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"


struct frame_table_entry* clock_frame_next(void){
  if (clk_ptr == list_end(&frame_list) || !clk_ptr){
    clk_ptr = list_begin (&frame_list);
  }else{
    clk_ptr = list_next (clk_ptr);
  }
  struct frame_table_entry *the_entry = list_entry(clk_ptr, struct frame_table_entry, lelem);
  return the_entry;
}

static void vm_frame_set_pinned(void *kernel_page, bool new_value){
  lock_acquire (&frame_lock);
  struct frame_table_entry temporary_entry;
  struct hash_elem *h = hash_find (&frame_map, &(temporary_entry.helem));
  struct frame_table_entry *f;
  temporary_entry.kernel_page = kernel_page;
  f = hash_entry(h, struct frame_table_entry, helem);
  f->pinned = new_value;
  lock_release (&frame_lock);
}

// Here is the clock algorithm for paging...
static struct frame_table_entry* choose_frame_to_swap(int *pagedir){
  size_t n = hash_size(&frame_map);
  // NO INFINITE LOOPING ALLOWED, at most twice
  for(size_t looping_var = 0; looping_var <= n*2; ++looping_var){
    struct frame_table_entry *the_entry = clock_frame_next();
    if(pagedir_is_accessed(pagedir, the_entry->virtual_address_page)){
      pagedir_set_accessed(pagedir, the_entry->virtual_address_page, false);
      continue;
    }else if(the_entry->pinned){
      continue;
    }
    return the_entry;
  }
}

// The frame mapping hash table, the key is the page info
static int  frame_hash_func(const struct hash_elem *elem, void *aux UNUSED){
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, helem);
  return hash_bytes( &entry->kernel_page, sizeof entry->kernel_page );
}

static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  struct frame_table_entry *first_entry = hash_entry(a, struct frame_table_entry, helem);
  struct frame_table_entry *sec_entry = hash_entry(b, struct frame_table_entry, helem);
  return first_entry->kernel_page < sec_entry->kernel_page;
}

// THe function here deallocate the frame and the page
void virtual_memory_frame_free(void *kernel_page){
  lock_acquire (&frame_lock);
  vm_frame_do_free (kernel_page, true);
  lock_release (&frame_lock);
}

void virtual_memory_frame_unpin(void* kernel_page){
  vm_frame_set_pinned (kernel_page, false);
}

void virtual_memory_frame_pin(void* kernel_page){
  vm_frame_set_pinned (kernel_page, true);
}


// Remove the table entry, not using palloc free
void virtual_memory_frame_entry_remove(void *kernel_page){
  lock_acquire (&frame_lock);
  vm_frame_do_free (kernel_page, false);
  lock_release (&frame_lock);
}

// The function here deallocates the frame or the page, synchronization held
void vm_frame_do_free(void *kernel_page, bool free_page){
  // We create a temp entry
  struct frame_table_entry temporary_entry;
  struct frame_table_entry *the_frame;

  temporary_entry.kernel_page = kernel_page;
  struct hash_elem *hash_element = hash_find (&frame_map, &(temporary_entry.helem));
  if(!hash_element) PANIC ("THE PAGE IS NOT FOUND");
  the_frame = hash_entry(hash_element, struct frame_table_entry, helem);
  hash_delete (&frame_map, &the_frame->helem);
  list_remove (&the_frame->lelem);
  if(free_page) palloc_free_page(kernel_page);
  free(the_frame);
}

// The function here allocates a new frame, and return the addresses of the pages associated
void* virtual_memory_frame_alloc(enum palloc_flags flags, void *virtual_address_page){
  lock_acquire (&frame_lock);
  void *frame_page = palloc_get_page (PAL_USER | flags);
  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));

  // If the page alloc fails
  if (!frame_page){
    // swap the page first
    struct frame_table_entry *target_frame_swaped = choose_frame_to_swap( thread_current()->pagedir );
    pagedir_clear_page(target_frame_swaped->t->pagedir, target_frame_swaped->virtual_address_page);
    bool flag = false;
    flag = flag || pagedir_is_dirty(target_frame_swaped->t->pagedir, target_frame_swaped->virtual_address_page);
    flag = flag || pagedir_is_dirty(target_frame_swaped->t->pagedir, target_frame_swaped->kernel_page);
    int swap_idx = virtual_memory_swap_out( target_frame_swaped->kernel_page );
    virtual_memory_swap_setup(target_frame_swaped->t->supplement_table, target_frame_swaped->virtual_address_page, swap_idx);
    virtual_memory_flag_setup(target_frame_swaped->t->supplement_table, target_frame_swaped->virtual_address_page, flag);
    vm_frame_do_free(target_frame_swaped->kernel_page, true);
    frame_page = palloc_get_page (PAL_USER | flags);
  }
  // If the frame alloc succeed, insert to hash table
  if(frame){
    frame->kernel_page = frame_page;
    frame->t = thread_current();
    frame->pinned = true;  
    frame->virtual_address_page = virtual_address_page;  
    hash_insert (&frame_map, &frame->helem);
    list_push_back (&frame_list, &frame->lelem);
    lock_release (&frame_lock);
    return frame_page;
  }else{
    lock_release (&frame_lock);
    return NULL;
  }
}


// THe initialization fo the frame
void virtual_memory_frame_init()
{
  lock_init (&frame_lock);
  hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);
  list_init (&frame_list);
  clk_ptr = NULL;
}