#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <list.h>
#include "threads/thread.h"
void syscall_init (void);
void sys_exit (int);
void sys_close_all_open_file (struct thread*);

/* This structure stores some information of the files opened by processes. 
   It is allocated when process opens a file, and deallocated when process exits. */
struct proc_file
{
  int fd;                     /* File descriptor. */
  char *name;                 /* Used for debugging. */
  bool deny_write;            /* Whether process and write to file. */
  struct file *fptr;          /* Pointer to struct file. */
  struct list_elem elem;      /* List element for process's open_files list. */
};
#endif /* userprog/syscall.h */
