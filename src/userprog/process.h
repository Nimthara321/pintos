#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

/* Structure to track child process status and synchronization with parent. */
struct child_status{
  tid_t tid;                  
  int exit_status;            
  bool load_success;          
  struct semaphore sema_load; 
  struct semaphore sema_exit; 
  bool waited;                
  int ref_cnt;                
  struct lock lock;           
  struct list_elem elem;      
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
