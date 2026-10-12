#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void sys_exit (int);
static bool is_valid_user_ptr (const void *uaddr, size_t size);
static void validate_user_ptr (const void *uaddr, size_t size);

void
syscall_init (void) 
{
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
  if(uaddr==NULL || !is_user_vadd(uaddr))
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
        validate_user_ptr(f->esp + sizeof(int),sizeof(int));
        sys_exit(*(int *) (f->esp + sizeof(int)));
        break;

      case SYS_WRITE:
        {
          validate_user_ptr(f->esp + sizeof(int), 3 * sizeof(int));
          int fd= *(int *) (f->esp + sizeof(int));
          const void *buffer = *(const void **) (f->esp + 2 * sizeof(int));
          unsigned size = *(unsigned *) (f->esp +3 * sizeof(int));
          if (size >0)
            validate_user_ptr(buffer, size);
          if(fd==1)
            {
              putbuf(buffer, size);
              f->eax= size;
            }
          else
            {
              f->eax= 0;
            }
          break;
        }

      default:
        sys_exit(-1);
        break;
    }
}

static void
sys_exit(int status)
{
  struct thread *cur= thread_current();
  cur->exit_status=status;
  thread_exit ();
}
