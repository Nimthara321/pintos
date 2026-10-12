#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/synch.h"

extern struct lock filesys_lock;

struct process_file{
  int fd;
  struct file *file;
  struct list_elem elem;
};

void syscall_init (void);
void sys_exit (int status);

#endif /* userprog/syscall.h */
