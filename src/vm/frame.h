#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "threads/synch.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"


// THe list of frames, designed for clock eviction, first is the list, sec is the pointer
static struct list frame_list; 
static struct list_elem *clk_ptr;
static struct list_elem *clk_ptr2;
// Here we define the lock for synchronizaion of frame operations
static struct lock frame_lock;
// This is the mapping from physical address to the frame table entry
static struct hash frame_map;
// This structure is the fram table entry
struct frame_table_entry{
    // The thread contained in the frame table entry
    struct thread *t;
    // The bool value to avoid the page being swapped away
    bool pinned;            
	// This is the kernel page, which maps to physical address
    void *kernel_page;           
	// The virtual memory address, ptr to page
    void *virtual_address_page;               
    // The frame mapping
    struct hash_elem helem;   
    // The fram list
    struct list_elem lelem; 
};

// THe following are the functions for Frame
void virtual_memory_frame_unpin (void* kernel_page);
void virtual_memory_frame_init (void);
void* virtual_memory_frame_alloc(enum palloc_flags flags, void *virtual_address_page);
void virtual_memory_frame_entry_remove(void*);
void virtual_memory_frame_pin(void* kernel_page);
void virtual_memory_frame_free(void*);
//  pintos -q run mmap-zero
#endif
