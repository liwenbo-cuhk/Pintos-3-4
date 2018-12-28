#include "userprog/syscall.h"
#include <list.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/interrupt.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <string.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#define USER_BASE 0x08048000
typedef tid_t pid_t;

static void syscall_handler (struct intr_frame *);
static bool is_good_ptr (int* vaddr);
static struct proc_file* get_pfile (int fd);
void sys_exit (int status);
pid_t sys_exec (const char* file_name);
int sys_write (int fd, const void* buffer, unsigned size);
int sys_read (int fd, const void* buffer, unsigned size);
void sys_close (int fd);
void sys_munmap (mapid_t mapping);
mapid_t sys_mmap (int fd, void *addr);
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  ASSERT(!intr_context ());
  /* First check if f->esp is a valid pointer. */
  if (f->esp == NULL || !is_good_ptr ((int*)f->esp))
  {
  	sys_exit (-1);
  }
  int *p = f->esp;
  /* Cast f->esp into an int*, then dereference it for the SYS_CODE. 
     It is strange that when there are two or three arguments, the
     address of the first argument is no longer p + 1. */
  switch(*p)
  {
  	case SYS_HALT:
  	{
  		shutdown_power_off ();
  		break;
  	}
  	case SYS_EXIT:
  	{
  		int status = is_good_ptr (p + 1) ? *(p + 1) : -1;
  		sys_exit (status);
  		break;
  	}
  	case SYS_EXEC:
  	{
      if (!is_good_ptr (p + 1) || !is_good_ptr ((int*)*(p + 1)))
      {

        sys_exit (-1);
      }
      const char *file_name = (const char*)*(p + 1);
      f->eax = sys_exec (file_name);
  		break;
  	}
  	case SYS_WAIT:
  	{
      if (!is_good_ptr (p + 1))
      {

        sys_exit (-1);
      }
      pid_t pid = (pid_t)*(p + 1);
      f->eax = process_wait (pid);
  		break;
  	}
  	case SYS_CREATE:
  	{
  		if (!is_good_ptr (p + 5) || !is_good_ptr ((int*)*(p + 4)))
  		{
  			sys_exit (-1);
  		}
  		const char *file_name = (const char*)*(p + 4);
        unsigned initial_size = (unsigned)*(p + 5);
        filesys_lock_acquire (&filesys_lock);
        f->eax = filesys_create (file_name, initial_size);
        filesys_lock_release (&filesys_lock);
  		break;
  	}
  	case SYS_REMOVE:
  	{
      if (!is_good_ptr (p + 1) || !is_good_ptr ((int*)*(p + 1)))
      {
        sys_exit (-1);
      }
      const char *file_name = (const char*)*(p + 1);
      filesys_lock_acquire (&filesys_lock);
      f->eax = filesys_remove (file_name);
      filesys_lock_release (&filesys_lock);
  		break;
  	}
  	case SYS_OPEN:
  	{
  		if (!is_good_ptr (p + 1) || !is_good_ptr ((int*)*(p + 1)))
  		{

  			sys_exit (-1);
  		}
  		const char *file_name = (const char*)*(p + 1);
      filesys_lock_acquire (&filesys_lock);
  		struct file *open_file = filesys_open (file_name);
      filesys_lock_release (&filesys_lock);
  		if (open_file == NULL)
  		{
  			f->eax = -1;
  		}
      else
      {
        struct thread *t = thread_current ();
        f->eax = t->fd_next;
        t->fd_next++;
        struct proc_file *pfile = malloc (sizeof (struct proc_file));
        if (pfile != NULL)
        {
          pfile->fd = f->eax;
          pfile->fptr = open_file;
          pfile->name = (char *)file_name;
          list_push_back (&t->open_files, &pfile->elem);
        }
      }
  		break;
  	}
  	case SYS_FILESIZE:
  	{
      if (!is_good_ptr (p + 1))
      {

        sys_exit (-1);
      }
      int fd = *(p + 1);
      for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
      {
        struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
        if (pfile != NULL && pfile->fd == fd)
        {
          filesys_lock_acquire (&filesys_lock);
          f->eax = file_length (pfile->fptr);
          filesys_lock_release (&filesys_lock);
          break;
        }
      }
  		break;
  	}
  	case SYS_READ:
  	{
      if (!is_good_ptr (p + 7) || !is_good_ptr ((int*)*(p + 6)))
      {
        sys_exit (-1);
      }
      int fd = *(p + 5);
      void* buffer = (void*)*(p + 6);
      unsigned size = (unsigned)*(p + 7);
      /* If the file descriptor is invalid, exit -1. */
      if (fd != STDIN_FILENO && fd < 2)
      {
        sys_exit (-1);
      }
      f->eax = sys_read (fd, buffer, size);
  		break;
  	}
  	case SYS_WRITE:
  	{
      if (!is_good_ptr (p + 7) || !is_good_ptr ((int*)*(p + 6)))
      {
        sys_exit (-1);
      }
  		int fd = *(p + 5);
  		void* buffer = (void*)*(p + 6);
  		unsigned size = (unsigned)*(p + 7);

      /* If the file descriptor is invalid, exit -1. */
      if (fd != STDOUT_FILENO && fd < 2)
      {
        sys_exit (-1);
      }
  		f->eax = sys_write (fd, buffer, size);
  		break;
  	}
  	case SYS_SEEK:
  	{
      if (!is_good_ptr (p + 5))
      {
        sys_exit (-1);
      }
      int fd = *(p + 4);
      unsigned position = (unsigned)*(p + 5);
      if (fd >= 0)
      {
        for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
        {
          struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
          if (pfile != NULL && pfile->fd == fd)
          {
            filesys_lock_acquire (&filesys_lock);
            file_seek (pfile->fptr, position);
            filesys_lock_release (&filesys_lock);
            break;
          }
        }
      }
  		break;
  	}
  	case SYS_TELL:
  	{
      if (!is_good_ptr (p + 1))
      {
        sys_exit (-1);
      }
      int fd = *(p + 1);
      if (fd >= 0)
      {
        for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
        {
          struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
          if (pfile != NULL && pfile->fd == fd)
          {
            filesys_lock_acquire (&filesys_lock);
            f->eax = file_tell (pfile->fptr);
            filesys_lock_release (&filesys_lock);
            break;
          }
        }
      }
  		break;
  	}
  	case SYS_CLOSE:
  	{
      if (!is_good_ptr (p + 1))
      {
        sys_exit (-1);
      }
      int fd = *(p + 1);
      /* If the file descriptor is invalid, exit -1. */
      if (fd < 2)
      {
        sys_exit (-1);
      }
      sys_close (fd);
  		break;
  	}
    case SYS_MMAP:
    {
      if (!is_user_vaddr ((const void*)(p + 5)) || !is_user_vaddr ((const void*)*(p + 5)))
      {
        sys_exit (-1);
      }
      int fd = (int)*(p + 4);
      void *addr = (void*)*(p + 5);
      f->eax = sys_mmap (fd, addr);
      break;
    }
    case SYS_MUNMAP:
    {
      if (!is_good_ptr (p + 1))
      {
        sys_exit (-1);
      }
      mapid_t mapping = (mapid_t)*(p + 1);
      sys_munmap (mapping);
      break;
    }
  }  
}

pid_t
sys_exec (const char* file_name)
{
  return process_execute (file_name);
}

int 
sys_write (int fd, const void* buffer, unsigned size)
{
	int ret = 0;
	if (fd == STDOUT_FILENO)
	{
      filesys_lock_acquire (&filesys_lock);
			putbuf (buffer, size);
      filesys_lock_release (&filesys_lock);
      ret = size;
	}
  else
  {
    for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
    {
      struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
      if (pfile != NULL && pfile->fd == fd)
      {
          filesys_lock_acquire (&filesys_lock);
          ret = file_write (pfile->fptr, buffer, size);
          filesys_lock_release (&filesys_lock);
          break;
      }
    }
  }
  return ret;
}

int
sys_read (int fd, const void* buffer, unsigned size)
{
  int ret = 0;
  if (fd == STDIN_FILENO)
  {
    filesys_lock_acquire (&filesys_lock);
    ret = input_getc ();
    filesys_lock_release (&filesys_lock);
  }
  else
  {
    for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
    {
      struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
      if (pfile != NULL && pfile->fd == fd)
      {
        filesys_lock_acquire (&filesys_lock);
        ret = file_read (pfile->fptr, (void*)buffer, size);
        filesys_lock_release (&filesys_lock);
        break;
      }
    }
  }
  return ret;
}
void
sys_close (int fd)
{
  struct thread *t = thread_current ();
  for (struct list_elem *iter = list_begin (&t->open_files); iter != list_end (&t->open_files); iter = list_next (iter))
  {
    struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
    if (pfile != NULL && fd == pfile->fd)
    {
      filesys_lock_acquire (&filesys_lock);
      file_close (pfile->fptr);
      filesys_lock_release (&filesys_lock);
      list_remove (iter);
      free (pfile);
      break;
    }
  }
}
void
sys_close_all_open_file (struct thread *t)
{
  /* Close all open files. 
     This cannot be written the same way as sys_close (), 
     because when we free pfile, the next list_elem cannot
     be found. */
  struct list_elem *iter;
  while (!list_empty (&t->open_files))
  {
    iter = list_pop_front (&t->open_files);
    struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
    file_close (pfile->fptr);
    free (pfile);
  }
}
void 
sys_exit (int status) 
{
    struct thread *t = thread_current ();
    t->tt_ptr->is_finished = true;
    t->tt_ptr->exit_status = status;
    /* It is better to update thread_with_tid_copy before sema up. */
    if (t->parent != NULL && t->parent->wait_child_pid == t->tid)
    {
      sema_up (&t->parent->sema);
    }

    printf ("%s: exit(%d)\n", t->name, status);
    thread_exit ();
}

static bool
is_good_ptr (int *vaddr)
{
	bool ret = is_user_vaddr ((const void*)vaddr);
	if (vaddr < USER_BASE)
	  ret = false;
	if (ret)
      ret = pagedir_get_page (thread_current ()->pagedir, vaddr) == NULL ? false : true;
    return ret;
}

mapid_t 
sys_mmap (int fd, void *addr)
{
  if (fd < 2 || addr == 0 || (int)addr % PGSIZE != 0)
  {
    return -1;
  }
  mapid_t ret = -1;
  struct proc_file *pfile = get_pfile (fd);
  if (pfile != NULL)
  {
    filesys_lock_acquire (&filesys_lock);
    void *upage = pg_round_down (addr), *check_upage;
    uint32_t read_bytes, zero_bytes;
    read_bytes = file_length (pfile->fptr);
    if (read_bytes == 0)
    {
      filesys_lock_release (&filesys_lock);
      return -1;
    }
    zero_bytes = PGSIZE - read_bytes % PGSIZE;
    /* Check if the contiguous memory is not present.
       If any page is present, mapping should fail.*/
    check_upage = upage;
    while (check_upage < upage + read_bytes + zero_bytes)
    {
      if (pagedir_get_page (thread_current ()->pagedir, check_upage) != NULL)
      {
        ret = -2;
        break;
      }
      check_upage += PGSIZE;
    } 

    if (ret != -2)
    {
      /* We need to hold the filesys_lock when entering load_segment (). */
      ret = load_segment (pfile->fptr, 0, upage, read_bytes, zero_bytes, true, thread_current ()->mapid_next) == true ? 1 : -2;
    }
    filesys_lock_release (&filesys_lock);

  }
  /* Map the file successfully. Increment mapid_next. */
  if (ret == 1)
  {
    ret = thread_current ()->mapid_next++;
  }
  return ret >= 0 ? ret : -1;
}

void 
sys_munmap (mapid_t mapping)
{
  struct thread *t = thread_current ();
  struct list_elem *iter = list_begin (&t->sup_list);
  int cnt = 0;
  struct page_table_entry *spte;
  filesys_lock_acquire (&filesys_lock);
  while (iter != list_end (&t->sup_list))
  {
    struct list_elem *iter_this = iter;
    iter = list_next (iter);

    spte = get_pte (iter_this); 
    /* If the page is modified, we write the data of page back
       to the mapped file.*/
    if (pte_get_mapid (spte) == mapping && pagedir_is_dirty (t->pagedir, pte_get_uva(spte)))
    {
      cnt++ ; 
      if (cnt == 1)
      {
        file_seek (pte_get_file (spte), 0);
      }
      file_write (pte_get_file (spte), pte_get_uva(spte), PGSIZE);
    }
    if (pte_get_mapid (spte) == mapping)
    {
      pte_set_mapped (spte, false);
      /* Mark page "not present" in pd. Later accesses will fault. */
      pagedir_clear_page(t->pagedir, pte_get_uva(spte));
    }
  }
  if (cnt > 0)
  {
    file_seek (pte_get_file (spte), 0);
  }

  filesys_lock_release (&filesys_lock);
}

static struct proc_file*
get_pfile (int fd)
{
  for (struct list_elem *iter = list_begin (&thread_current ()->open_files); iter != list_end (&thread_current ()->open_files); iter = list_next (iter))
  {
    struct proc_file *pfile = list_entry (iter, struct proc_file, elem);
    if (pfile != NULL && pfile->fd == fd)
    {
      return pfile;
    }
  }
  return NULL;
}