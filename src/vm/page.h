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

struct supplemental_page_table{struct hash page_map;};
// THe stage of the page
enum page_status {ALL_ZERO, ON_FRAME, ON_SWAP, FROM_FILESYS};

struct supplemental_page_table_entry{
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


bool vm_load_page(struct supplemental_page_table *supplement_table, int *pagedir, void *virtual_address_page);
void vm_unpin_page(struct supplemental_page_table *supplement_table, void *page);
bool vm_supt_install_frame (struct supplemental_page_table *supplement_table, void *virtual_address_page, void *kernel_page);
bool vm_supt_set_dirty (struct supplemental_page_table *supplement_table, void *, bool);
bool vm_supt_install_filesys (struct supplemental_page_table *supplement_table, void *page, struct file * file, off_t offset, int bytes_to_read, int no_bytes, bool writable);
struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supplement_table, void *);
bool vm_supt_has_entry (struct supplemental_page_table *, void *page);
void vm_pin_page(struct supplemental_page_table *supplement_table, void *page);
struct supplemental_page_table* vm_supt_create (void);
bool vm_supt_mm_unmap(struct supplemental_page_table *supplement_table, int *pagedir, void *page, struct file *f, off_t offset, size_t bytes);
bool vm_supt_install_zeropage (struct supplemental_page_table *supplement_table, void *);
bool vm_supt_set_swap (struct supplemental_page_table *supplement_table, void *, int);
void vm_supt_destroy (struct supplemental_page_table *);

#endif
