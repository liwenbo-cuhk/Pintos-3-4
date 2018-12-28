#include <hash.h>
#include "vm/page.h"

// THe hash function for frame mapping, the key is kaddr
static unsigned hash_function(const struct hash_elem *elem, void *aux UNUSED){
  struct page_table_entry *entry = hash_entry(elem, struct page_table_entry, elem);
  return hash_int( (int)entry->virtual_address_page );
}

static bool less_check(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
  struct page_table_entry *first_entry = hash_entry(a, struct page_table_entry, elem);
  struct page_table_entry *sec_entry = hash_entry(b, struct page_table_entry, elem);
  return first_entry->virtual_address_page < sec_entry->virtual_address_page;
}

static void destroy_the_entry(struct hash_elem *elem, void *aux UNUSED){
  struct page_table_entry *entry = hash_entry(elem, struct page_table_entry, elem);
  if(SWAP == entry->status){
    virtual_memory_swap(entry->index_to_swap);
  }else if(entry->kernel_page){
    virtual_memory_frame_entry_remove(entry->kernel_page);
  }
  free (entry);
}


static bool virtual_memory_page_loading(struct page_table_entry *target_page_table_entry, void *kernel_page){
  int n_read = file_read (target_page_table_entry->file, kernel_page, target_page_table_entry->bytes_to_read);
  file_seek (target_page_table_entry->file, target_page_table_entry->offset_of_file);
  if(n_read == (int)target_page_table_entry->bytes_to_read){
    memset (kernel_page + n_read, 0, target_page_table_entry->no_bytes);
    return true;
  }else{
    return false;
  }
}

void virtual_memory_destroy(struct page_table *supplement_table){
  hash_destroy (&supplement_table->page_map, destroy_the_entry);
  free (supplement_table);
}

struct page_table* virtual_memroy_table_create(void){
  struct page_table *supplement_table = (struct page_table*) malloc(sizeof(struct page_table));
  hash_init (&supplement_table->page_map, hash_function, less_check, NULL);
  return supplement_table;
}

bool virtual_memory_frame_install(struct page_table *supplement_table, void *virtual_address_page, void *kernel_page){
  struct page_table_entry *target_page_table_entry;
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &target_page_table_entry->elem);
  target_page_table_entry = (struct page_table_entry *) malloc(sizeof(struct page_table_entry));
  target_page_table_entry->index_to_swap = -1;
  target_page_table_entry->kernel_page = kernel_page;
  target_page_table_entry->status = FRAME;
  target_page_table_entry->virtual_address_page = virtual_address_page;
  target_page_table_entry->dirty = false;
  if (prev_elem){
    free (target_page_table_entry);
    return false;
  }else {
    return true;
  }
}

void virtual_memory_page_pin(struct page_table *supplement_table, void *page){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  if(target_page_table_entry){
    virtual_memory_frame_pin(target_page_table_entry->kernel_page);
  }else{
    return;
  }
}

void virtual_memory_page_unpin(struct page_table *supplement_table, void *page){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  if (FRAME == target_page_table_entry->status) virtual_memory_frame_unpin (target_page_table_entry->kernel_page);
}

bool virtual_memroy_page_install(struct page_table *supplement_table, void *virtual_address_page){
  struct page_table_entry *target_page_table_entry = (struct page_table_entry *) malloc(sizeof(struct page_table_entry));
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &target_page_table_entry->elem);
  target_page_table_entry->virtual_address_page = virtual_address_page;
  target_page_table_entry->dirty = false;
  target_page_table_entry->kernel_page = NULL;
  target_page_table_entry->status = ZEROS;
  if (prev_elem){
    return false;
  }else{
    return true;
  }
}


bool virtual_memory_file_system_install(struct page_table *supplement_table, void *virtual_address_page, struct file * file, off_t offset, int bytes_to_read, int no_bytes, bool writable){
  struct page_table_entry *target_page_table_entry = (struct page_table_entry *) malloc(sizeof(struct page_table_entry));
  target_page_table_entry->status = FILESYSTEM;
  target_page_table_entry->dirty = false;
  target_page_table_entry->file = file;
  target_page_table_entry->no_bytes = no_bytes;
  target_page_table_entry->writable = writable;
  target_page_table_entry->offset_of_file = offset;
  target_page_table_entry->kernel_page = NULL;
  target_page_table_entry->bytes_to_read = bytes_to_read;
  target_page_table_entry->virtual_address_page = virtual_address_page;
  struct hash_elem *prev_elem = hash_insert (&supplement_table->page_map, &target_page_table_entry->elem);
  if (!prev_elem){ 
    return true;
  }else{
    return false;
  }
}

bool virtual_memory_swap_setup(struct page_table *supplement_table, void *page, int index_to_swap){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  if(target_page_table_entry){
    target_page_table_entry->status = SWAP;
    target_page_table_entry->kernel_page = NULL;
    target_page_table_entry->index_to_swap = index_to_swap;
    return true;
  }else{
    return false;
  }
}



bool virtual_memory_entry_flag(struct page_table *supplement_table, void *page){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  if(target_page_table_entry){
    return true;
  }else{
    return false;
  }
}

bool virtual_memory_flag_setup(struct page_table *supplement_table, void *page, bool value){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  target_page_table_entry->dirty = target_page_table_entry->dirty || value;
  return true;
}



struct page_table_entry* virtual_memory_search (struct page_table *supplement_table, void *page){
  struct page_table_entry target_page_table_entry_temp;
  target_page_table_entry_temp.virtual_address_page = page;
  struct hash_elem *elem = hash_find (&supplement_table->page_map, &target_page_table_entry_temp.elem);
  if(elem){
    return hash_entry(elem, struct page_table_entry, elem);
  }else{
    return NULL;
  }
}

bool virtual_memory_page_load(struct page_table *supplement_table, int *pagedir, void *virtual_address_page){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, virtual_address_page);
  bool writable = true;
  void *frame_page = virtual_memory_frame_alloc(PAL_USER, virtual_address_page);
  if(!target_page_table_entry){
    return false;
  }else if(FRAME == target_page_table_entry->status) {
    return true;
  }
  if(!frame_page) return false;
  switch (target_page_table_entry->status){
  case FRAME:
    break;
  case ZEROS:
    memset (frame_page, 0, PGSIZE);
    break;
  case FILESYSTEM:
    if(!virtual_memory_page_loading(target_page_table_entry, frame_page)){
      virtual_memory_frame_free(frame_page);
      return false;
    }
    writable = target_page_table_entry->writable;
    break;
  case SWAP:
    virtual_memory_swap_in (target_page_table_entry->index_to_swap, frame_page);
    break;
  default:
    PANIC ("THE STATE IS UNREACHABLE");
  }
  if(pagedir_set_page(pagedir, virtual_address_page, frame_page, writable)){
    target_page_table_entry->status = FRAME;
    target_page_table_entry->kernel_page = frame_page;
    pagedir_set_dirty (pagedir, frame_page, false);
    virtual_memory_frame_unpin(frame_page);
    return true;
  }else{
    virtual_memory_frame_free(frame_page);
    return false;
  }
}

bool virtual_memory_unmap(struct page_table *supplement_table, int *pagedir, void *page, struct file *f, off_t offset, size_t bytes){
  struct page_table_entry *target_page_table_entry = virtual_memory_search(supplement_table, page);
  if (target_page_table_entry->status == FRAME) virtual_memory_frame_pin(target_page_table_entry->kernel_page);
  switch (target_page_table_entry->status){
  case SWAP:
    {
      bool is_dirty = target_page_table_entry->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, target_page_table_entry->virtual_address_page);
      if(!is_dirty){
        virtual_memory_swap(target_page_table_entry->index_to_swap);
      }
      else{
        void *tmp_page = palloc_get_page(0); 
        virtual_memory_swap_in (target_page_table_entry->index_to_swap, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
      }
    break;
    }
  case FILESYSTEM:
    break;
  case FRAME:
    {
      bool is_dirty = target_page_table_entry->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, target_page_table_entry->virtual_address_page);
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, target_page_table_entry->kernel_page);
      if(is_dirty) file_write_at (f, target_page_table_entry->virtual_address_page, bytes, offset);
      virtual_memory_frame_free (target_page_table_entry->kernel_page);
      pagedir_clear_page (pagedir, target_page_table_entry->virtual_address_page);
      break;
    }
  default:
    PANIC ("THIS IS IMPOSSIBLE!! ALL ZEROS");
  }
  hash_delete(& supplement_table->page_map, &target_page_table_entry->elem);
  return true;
}

