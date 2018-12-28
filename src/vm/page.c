#include <hash.h>
#include "vm/page.h"

// THe hash function for frame mapping, the key is kaddr
static unsigned hash_function(const struct hash_elem *elem, void *aux UNUSED){
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  return hash_int( (int)entry->virtual_address_page );
}

static bool less_check(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
  struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
  return a_entry->virtual_address_page < b_entry->virtual_address_page;
}

static void destroy_the_entry(struct hash_elem *elem, void *aux UNUSED){
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  if(ON_SWAP == entry->status){
    vm_swap_free (entry->index_to_swap);
  }else if(entry->kernel_page){
    vm_frame_remove_entry (entry->kernel_page);
  }
  free (entry);
}

static bool vm_load_page_from_filesys(struct supplemental_page_table_entry *spte, void *kernel_page){
  int n_read = file_read (spte->file, kernel_page, spte->bytes_to_read);
  file_seek (spte->file, spte->offset_of_file);
  if(n_read == (int)spte->bytes_to_read){
    memset (kernel_page + n_read, 0, spte->no_bytes);
    return true;
  }else{
    return false;
  }
}

void vm_supt_destroy (struct supplemental_page_table *supplement_table){
  hash_destroy (&supplement_table->page_map, destroy_the_entry);
  free (supplement_table);
}

struct supplemental_page_table* vm_supt_create(void){
  struct supplemental_page_table *supplement_table = (struct supplemental_page_table*) malloc(sizeof(struct supplemental_page_table));
  hash_init (&supplement_table->page_map, hash_function, less_check, NULL);
  return supplement_table;
}

bool vm_supt_install_frame (struct supplemental_page_table *supplement_table, void *virtual_address_page, void *kernel_page){
  struct supplemental_page_table_entry *spte;
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &spte->elem);
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  spte->index_to_swap = -1;
  spte->kernel_page = kernel_page;
  spte->status = ON_FRAME;
  spte->virtual_address_page = virtual_address_page;
  spte->dirty = false;
  if (prev_elem){
    free (spte);
    return false;
  }else {
    return true;
  }
}

void vm_pin_page(struct supplemental_page_table *supplement_table, void *page){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  if(spte){
    vm_frame_pin (spte->kernel_page);
  }else{
    return;
  }
}

void vm_unpin_page(struct supplemental_page_table *supplement_table, void *page){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  if (ON_FRAME == spte->status) vm_frame_unpin (spte->kernel_page);
}

bool vm_supt_install_zeropage (struct supplemental_page_table *supplement_table, void *virtual_address_page){
  struct supplemental_page_table_entry *spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &spte->elem);
  spte->virtual_address_page = virtual_address_page;
  spte->dirty = false;
  spte->kernel_page = NULL;
  spte->status = ALL_ZERO;
  if (prev_elem){
    return false;
  }else{
    return true;
  }
}


bool vm_supt_install_filesys (struct supplemental_page_table *supplement_table, void *virtual_address_page, struct file * file, off_t offset, int bytes_to_read, int no_bytes, bool writable){
  struct supplemental_page_table_entry *spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));
  spte->status = FROM_FILESYS;
  spte->dirty = false;
  spte->file = file;
  spte->no_bytes = no_bytes;
  spte->writable = writable;
  spte->offset_of_file = offset;
  spte->kernel_page = NULL;
  spte->bytes_to_read = bytes_to_read;
  spte->virtual_address_page = virtual_address_page;
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &spte->elem);
  if (!prev_elem){ 
    return true;
  }else{
    return false;
  }
}

bool vm_supt_set_swap (struct supplemental_page_table *supplement_table, void *page, int index_to_swap){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  if(spte){
    spte->status = ON_SWAP;
    spte->kernel_page = NULL;
    spte->index_to_swap = index_to_swap;
    return true;
  }else{
    return false;
  }
}



bool vm_supt_has_entry(struct supplemental_page_table *supplement_table, void *page){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  if(spte){
    return true;
  }else{
    return false;
  }
}

bool vm_supt_set_dirty (struct supplemental_page_table *supplement_table, void *page, bool value){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  spte->dirty = spte->dirty || value;
  return true;
}



struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supplement_table, void *page){
  struct supplemental_page_table_entry spte_temp;
  spte_temp.virtual_address_page = page;
  struct hash_elem *elem = hash_find (&supplement_table->page_map, &spte_temp.elem);
  if(elem){
    return hash_entry(elem, struct supplemental_page_table_entry, elem);
  }else{
    return NULL;
  }
}

bool vm_load_page(struct supplemental_page_table *supplement_table, int *pagedir, void *virtual_address_page){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, virtual_address_page);
  bool writable = true;
  void *frame_page = vm_frame_allocate(PAL_USER, virtual_address_page);
  if(!spte){
    return false;
  }else if(ON_FRAME == spte->status) {
    return true;
  }
  if(!frame_page) return false;
  switch (spte->status){
  case ON_FRAME:
    break;
  case ALL_ZERO:
    memset (frame_page, 0, PGSIZE);
    break;
  case FROM_FILESYS:
    if(!vm_load_page_from_filesys(spte, frame_page)){
      vm_frame_free(frame_page);
      return false;
    }
    writable = spte->writable;
    break;
  case ON_SWAP:
    vm_swap_in (spte->index_to_swap, frame_page);
    break;
  default:
    PANIC ("THE STATE IS UNREACHABLE");
  }
  if(pagedir_set_page(pagedir, virtual_address_page, frame_page, writable)){
    spte->status = ON_FRAME;
    spte->kernel_page = frame_page;
    pagedir_set_dirty (pagedir, frame_page, false);
    vm_frame_unpin(frame_page);
    return true;
  }else{
    vm_frame_free(frame_page);
    return false;
  }
}

bool vm_supt_mm_unmap(struct supplemental_page_table *supplement_table, int *pagedir, void *page, struct file *f, off_t offset, size_t bytes){
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supplement_table, page);
  if (spte->status == ON_FRAME) vm_frame_pin (spte->kernel_page);
  switch (spte->status){
  case ON_SWAP:
    {
      bool is_dirty = spte->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->virtual_address_page);
      if(!is_dirty){
        vm_swap_free (spte->index_to_swap);
      }
      else{
        void *tmp_page = palloc_get_page(0); 
        vm_swap_in (spte->index_to_swap, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
      }
    break;
    }
  case FROM_FILESYS:
    break;
  case ON_FRAME:
    {
      bool is_dirty = spte->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->virtual_address_page);
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->kernel_page);
      if(is_dirty) file_write_at (f, spte->virtual_address_page, bytes, offset);
      vm_frame_free (spte->kernel_page);
      pagedir_clear_page (pagedir, spte->virtual_address_page);
      break;
    }
  default:
    PANIC ("THIS IS IMPOSSIBLE!! ALL ZEROS");
  }
  hash_delete(& supplement_table->page_map, &spte->elem);
  return true;
}

