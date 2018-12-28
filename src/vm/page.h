#ifndef VM_PAGE_H
#define VM_PAGE_H
#include "vm/frame.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include <hash.h>
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"

struct page_table{struct hash page_map;};
// THe stage of the page
enum page_status {ZEROS, FRAME, SWAP, FILESYSTEM};

struct page_table_entry{
    void *kernel_page;
    struct file *file;
    bool dirty;     
    int index_to_swap;
    int bytes_to_read; 
    int no_bytes;
    bool writable;
    off_t offset_of_file;
    struct hash_elem elem;
    enum page_status status;
    void *virtual_address_page;              
  };


bool virtual_memory_page_load(struct page_table *supplement_table, int *pagedir, void *virtual_address_page);
void virtual_memory_page_unpin(struct page_table *supplement_table, void *page);
bool virtual_memory_frame_install(struct page_table *supplement_table, void *virtual_address_page, void *kernel_page);
bool virtual_memory_flag_setup(struct page_table *supplement_table, void *, bool);
bool virtual_memory_file_system_install(struct page_table *supplement_table, void *page, struct file * file, off_t offset, int bytes_to_read, int no_bytes, bool writable);
struct page_table_entry* virtual_memory_search (struct page_table *supplement_table, void *);
bool virtual_memory_entry_flag(struct page_table *, void *page);
void virtual_memory_page_pin(struct page_table *supplement_table, void *page);
struct page_table* virtual_memory_table_create(void);
bool virtual_memory_unmap(struct page_table *supplement_table, int *pagedir, void *page, struct file *f, off_t offset, size_t bytes);
bool virtual_memory_page_install(struct page_table *supplement_table, void *);
bool virtual_memory_swap_setup(struct page_table *supplement_table, void *, int);
void virtual_memory_destroy(struct page_table *);

#endif
