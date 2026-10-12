#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);
static bool is_valid_user_ptr (const void *uaddr, size_t size);
static void validate_user_ptr (const void *uaddr, size_t size);
static void validate_user_str(const char *uaddr);
static struct process_file *get_process_file (int fd);

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool
is_valid_user_ptr(const void *uaddr, size_t size)
{
  if(uaddr ==NULL)
    return false;
  if(!is_user_vaddr(uaddr))
    return false;
  if(size==0)
    return true;

  const uint8_t *start =(const uint8_t *) uaddr;
  const uint8_t *end= start + size;
  if ((uintptr_t) end<(uintptr_t) start)
    return false;
  if(!is_user_vaddr (end - 1))
    return false;
  struct thread *cur= thread_current();
  const uint8_t *page;
  for(page =pg_round_down (start); page < end; page += PGSIZE)
    {
      if(pagedir_get_page(cur->pagedir, page)==NULL)
        return false;
    }
  return true;
}

static void
validate_user_ptr(const void *uaddr, size_t size)
{
  if(!is_valid_user_ptr(uaddr,size))
    sys_exit(-1);
}

UNUSED static void
validate_user_str(const char *uaddr)
{
  if(uaddr==NULL || !is_user_vaddr(uaddr))
    sys_exit(-1);

  struct thread *cur=thread_current();
  const char *ptr=uaddr;
  while(true)
    {
      if(!is_user_vaddr(ptr))
        sys_exit (-1);
      if(pg_ofs (ptr)==0 || ptr==uaddr)
        {
          if(pagedir_get_page(cur->pagedir, ptr)== NULL)
            sys_exit(-1);
        }
      if(*ptr== '\0')
        break;
      ptr++;
    }
}

static struct process_file *
get_process_file(int fd)
{
  if(fd<2)
    return NULL;
  struct thread *cur= thread_current();
  struct list_elem *e;
  for (e=list_begin(&cur->file_descriptors);
       e !=list_end(&cur->file_descriptors);
       e =list_next(e))
    {
      struct process_file *pf= list_entry(e, struct process_file, elem);
      if(pf->fd==fd)
        return pf;
    }
  return NULL;
}

static void
syscall_handler(struct intr_frame *f)
{
  validate_user_ptr(f->esp, sizeof (int));
  int syscall_num= *(int *) f->esp;

  switch(syscall_num)
    {
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      validate_user_ptr(f->esp + 4, sizeof (int));
      sys_exit(*(int *) (f->esp + 4));
      break;

    case SYS_EXEC:
      {
        validate_user_ptr (f->esp + 4, sizeof (char *));
        const char *cmd_line = *(const char **) (f->esp + 4);
        validate_user_str (cmd_line);
        f->eax =process_execute (cmd_line);
        break;
      }

    case SYS_WAIT:
      {
        validate_user_ptr (f->esp + 4, sizeof (int));
        tid_t pid= *(int *) (f->esp + 4);
        f->eax = process_wait (pid);
        break;
      }

    case SYS_CREATE:
      {
        validate_user_ptr(f->esp +4, sizeof(char *) + sizeof(unsigned));
        const char *file = *(const char **) (f->esp + 4);
        unsigned initial_size = *(unsigned *)(f->esp + 8);
        validate_user_str(file);
        lock_acquire(&filesys_lock);
        f->eax = filesys_create (file, initial_size);
        lock_release(&filesys_lock);
        break;
      }

    case SYS_REMOVE:
      {
        validate_user_ptr(f->esp + 4, sizeof (char *));
        const char *file = *(const char **) (f->esp + 4);
        validate_user_str(file);
        lock_acquire(&filesys_lock);
        f->eax = filesys_remove (file);
        lock_release(&filesys_lock);
        break;
      }

    case SYS_OPEN:
      {
        validate_user_ptr(f->esp + 4, sizeof (char *));
        const char *file = *(const char **) (f->esp + 4);
        validate_user_str(file);
        lock_acquire (&filesys_lock);
        struct file *f_ptr= filesys_open (file);
        if (f_ptr ==NULL)
          {
            lock_release(&filesys_lock);
            f->eax = -1;
          }
        else
          {
            struct process_file *pf = malloc (sizeof (struct process_file));
            if (pf == NULL)
              {
                file_close(f_ptr);
                lock_release(&filesys_lock);
                f->eax = -1;
              }
            else
              {
                struct thread *cur = thread_current();
                pf->file = f_ptr;
                pf->fd = cur->next_fd++;
                list_push_back(&cur->file_descriptors, &pf->elem);
                lock_release(&filesys_lock);
                f->eax = pf->fd;
              }
          }
        break;
      }

    case SYS_FILESIZE:
      {
        validate_user_ptr(f->esp + 4, sizeof(int));
        int fd = *(int *) (f->esp + 4);
        struct process_file *pf = get_process_file(fd);
        if (pf == NULL)
          f->eax = -1;
        else
          {
            lock_acquire(&filesys_lock);
            f->eax = file_length(pf->file);
            lock_release(&filesys_lock);
          }
        break;
      }

    case SYS_READ:
      {
        validate_user_ptr(f->esp + 4, 3 * sizeof(int));
        int fd = *(int *)(f->esp + 4);
        void *buffer = *(void **)(f->esp + 8);
        unsigned size = *(unsigned *)(f->esp + 12);
        validate_user_ptr(buffer, size);
        if (fd == 0)
          {
            uint8_t *buf = (uint8_t *) buffer;
            for(unsigned i = 0; i < size; i++)
              buf[i] = input_getc ();
            f->eax = size;
          }
        else if(fd == 1)
          {
            f->eax = -1;
          }
        else
          {
            struct process_file *pf = get_process_file(fd);
            if(pf == NULL)
              f->eax = -1;
            else
              {
                lock_acquire(&filesys_lock);
                f->eax = file_read(pf->file, buffer, size);
                lock_release(&filesys_lock);
              }
          }
        break;
      }

    case SYS_WRITE:
      {
        validate_user_ptr(f->esp + 4, 3 * sizeof(int));
        int fd= *(int *) (f->esp + 4);
        const void *buffer = *(const void **) (f->esp + 8);
        unsigned size = *(unsigned *) (f->esp + 12);
        validate_user_ptr(buffer, size);
        if(fd==1)
          {
            putbuf(buffer, size);
            f->eax= size;
          }
        else if(fd == 0)
          {
            f->eax = -1;
          }
        else
          {
            struct process_file *pf = get_process_file (fd);
            if(pf == NULL)
              f->eax = -1;
            else
              {
                lock_acquire (&filesys_lock);
                f->eax = file_write (pf->file, buffer, size);
                lock_release (&filesys_lock);
              }
          }
        break;
      }

    case SYS_SEEK:
      {
        validate_user_ptr(f->esp + 4, 2 * sizeof (int));
        int fd = *(int *)(f->esp + 4);
        unsigned position = *(unsigned *)(f->esp + 8);
        struct process_file *pf = get_process_file (fd);
        if(pf != NULL)
          {
            lock_acquire(&filesys_lock);
            file_seek(pf->file, position);
            lock_release(&filesys_lock);
          }
        break;
      }

    case SYS_TELL:
      {
        validate_user_ptr(f->esp + 4, sizeof (int));
        int fd = *(int *)(f->esp + 4);
        struct process_file *pf = get_process_file (fd);
        if(pf == NULL)
          f->eax = -1;
        else
          {
            lock_acquire(&filesys_lock);
            f->eax= file_tell (pf->file);
            lock_release(&filesys_lock);
          }
        break;
      }

    case SYS_CLOSE:
      {
        validate_user_ptr(f->esp + 4, sizeof (int));
        int fd = *(int *)(f->esp + 4);
        if (fd >= 2)
          {
            struct process_file *pf = get_process_file (fd);
            if (pf != NULL)
              {
                lock_acquire(&filesys_lock);
                file_close(pf->file);
                lock_release(&filesys_lock);
                list_remove(&pf->elem);
                free(pf);
              }
          }
        break;
      }

    default:
      sys_exit(-1);
      break;
    }
}

void
sys_exit (int status)
{
  struct thread *cur = thread_current ();
  cur->exit_status = status;
  thread_exit ();
}
