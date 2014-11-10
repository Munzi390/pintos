#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/string.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"

#define print 0
#define BUF_SIZE 255

struct file_info* find_file_info(int fd);
void free_file(struct file_info * f_d);

struct process_info* find_child(tid_t tid);

static void syscall_handler (struct intr_frame *);
int syscall_open(const char *file);
void syscall_exit(int status);
int syscall_wait(pid_t pid);
pid_t syscall_exec (const char *file);
int syscall_filesize(int fd);
int syscall_create(const char *file,unsigned size);
bool syscall_remove(const char *file);
int syscall_read (int fd, void *buffer, unsigned size);
int syscall_write (int fd, const void *buffer, unsigned size);
void syscall_close(int fd);
void syscall_seek(int fd, unsigned pos);
off_t syscall_tell(int fd);


struct file_info{
	int fd; 								/* file discripter num */
	struct file *file; 						/* pointer of opend file */
	struct list_elem elem; 					/* list handler of fd_list of process */
};


/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if((int)uaddr==0x4){syscall_exit(-1);}
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
	if((int)udst==0x4){syscall_exit(-1);}
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}


/* check the string is in user_address */
static void
valid_string (const char *ptr)
{
	int ret = 0;
	if (!is_user_vaddr (ptr))
		syscall_exit (-1);

	for (;(ret=get_user((uint8_t*)ptr))!='\0'; ptr++)
	{
		if (!is_user_vaddr (ptr) || ret == -1)
			syscall_exit (-1);
	}
}

static void
valid_addr (const char *ptr)
{
	int ret = 0;

	if(is_kernel_vaddr(ptr) || !is_user_vaddr (ptr) || ptr==NULL){
		syscall_exit (-1);
	}

	int i = 0;
	for(; i<4; i++)
    {
		if(!is_user_vaddr (ptr) || ret == -1)
			syscall_exit (-1);
		ret = get_user ((uint8_t *)ptr++);
    }
}



/* close file, but not close file_info */
void
free_file(struct file_info * f_d){
	if(f_d==NULL){
		return;
	}
	else{
		list_remove(&f_d->elem);
		lock_acquire(&filesys_lock);
		file_close(f_d->file);
		lock_release(&filesys_lock);
		free(f_d);
	}
}

/* find file_info by given fd number in current_thread's open_list */
struct file_info*
find_file_info(int fd){
	if(list_empty(&thread_current()->open_list)){
			return NULL;
		}
	struct file_info *temp = list_entry(list_front(&thread_current()->open_list),struct file_info,elem);

	for(; temp->elem.next!=NULL; temp=list_entry(temp->elem.next,struct file_info, elem)){
		if(temp->fd == fd){
			return temp;
		}
	}
	return NULL;
}

struct process_info*
find_child(tid_t tid){
	if(list_empty(&thread_current()->child_list)){
		return NULL;
		}
	struct process_info *temp=list_entry(list_front(&thread_current()->child_list),struct process_info,elem);

	for(;temp->elem.next!=NULL;temp=list_entry(temp->elem.next,struct process_info, elem)){
		if(temp->pid==(pid_t)tid){
			return temp;
		}
	}
	return NULL;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
 }

void
syscall_exit(int status){

	printf ("%s: exit(%d)\n",thread_current()->name, status);
	struct thread *t= thread_current();
	if(t->process_file != NULL){
	    	  file_allow_write(t->process_file);
	 }
	struct file_info *temp;

	/* free all open files, but not file_infos */
	while(list_size(&t->open_list)){
		temp = list_entry(list_pop_front(&t->open_list),struct file_info,elem);
		free_file(temp);
	}
	/* if child alive, set child's exit status */
	if (thread_current ()->child != NULL)
		thread_current ()->child->exit_status = status;

	thread_exit ();
	NOT_REACHED ();
}

int
syscall_wait(pid_t pid){
	return process_wait((tid_t)pid);
}

pid_t
syscall_exec (const char *file)
{
  valid_string(file);
  /* valid_int(file); */

  if(file == NULL){
    syscall_exit(-1);
  }

  int child_pid = (pid_t)process_execute(file);

  /* exec_wait is sema_up in final part of start_process */
  sema_down(&thread_current()->exec_wait);

  /* execution=1 when child running start_process */
  if(thread_current()->execution){
	  return child_pid;
  }
  /* execution != 1, then child running fail*/
  return -1;
}


void syscall_close(int fd)
{
	struct file_info* closing_file = find_file_info(fd);
	free_file(closing_file);
}



int
syscall_open(const char *file)
{
	if(file==NULL){
		syscall_exit(-1);
	}

	valid_string(file);
	/* valid_int(file); */

	lock_acquire(&filesys_lock);
	struct file *open = filesys_open(file);

	/* open fail */
	if(open==NULL){
		return -1;
	}

	lock_release(&filesys_lock);

	struct file_info *file_info = malloc(sizeof(struct file_info));

	file_info->fd = thread_current()->file_count++;
	file_info->file = open;
	list_push_back(&thread_current()->open_list,&file_info->elem);

	return file_info->fd;
}

int
syscall_filesize(int fd){
	struct file_info *temp=find_file_info(fd);

	if(temp!=NULL){
		return (int)file_length(temp->file);
	}

	else{
		return -1;
	}
}

int
syscall_create(const char *file, unsigned size)
{
	valid_string(file);

	if(file==NULL){
		syscall_exit(-1);
	}

	lock_acquire(&filesys_lock);

	bool success = filesys_create(file,size);

	lock_release(&filesys_lock);

	return success;
}

bool
syscall_remove(const char *file)
{
	valid_string(file);

	if(file==NULL){
		syscall_exit(-1);
	}

	lock_acquire(&filesys_lock);

	bool success = filesys_remove(file);

	lock_release(&filesys_lock);

	return success;
}


int
syscall_read (int fd, void *buffer, unsigned length)
{
	if(get_user(buffer)==-1){
		syscall_exit(-1);
	}
	
	int read;
	int temp;
	int reading;

	int i;

	struct file_info *file_info = find_file_info (fd);
	char temp_buf[BUF_SIZE];

	if (file_info == NULL)
		return -1;

	reading = 0;

	for(;length>0;length-=read)
	{
		temp = length < BUF_SIZE ? length : BUF_SIZE;
		
		lock_acquire(&filesys_lock);
		
		read = file_read (file_info->file, temp_buf, temp);
		
		lock_release(&filesys_lock);
		
		if (read == 0) break;

		for (i = 0; i < read; i++)
		{
			if (!is_user_vaddr (buffer + reading + i))
	        	  syscall_exit (-1);
			if (!put_user (buffer + reading + i, temp_buf[i]))
				syscall_exit (-1);
		}

		reading += read;
	}

	return reading;
}

int
syscall_write (int fd, const void *buffer, unsigned size)
{
	if(get_user(buffer)==-1){
		syscall_exit(-1);
	}

	struct file_info *file_info = NULL;
	int writing;
	int read;
	int temp;
	
	int i;

	if (fd != STDOUT_FILENO)
	{
		file_info = find_file_info(fd);
		if (file_info == NULL) return -1;
	}
	
	writing = 0;
	
	/* similarly inode_write_at */
	for(;size>0;size-=read)
	{
		temp = size < 512 ? size : 512;
		
		for (i = 0; i < temp; i++)
		{
			if (!is_user_vaddr (buffer + writing + i)) syscall_exit (-1);
			read = get_user (buffer + writing + i);

			if (read == -1)
				syscall_exit (-1);
	        }	
		
		if (fd == STDOUT_FILENO)
		{
			putbuf ((char *) buffer + writing, temp);
			read = temp;
		}
		else
		{
			lock_acquire(&filesys_lock);
			
			read = file_write (file_info->file, buffer + writing, temp);
			lock_release(&filesys_lock);

			if (read == 0) break;
		}

		writing += read;
	}

	return writing;
}

void
syscall_seek(int fd, unsigned pos)
{
	struct file* seek = find_file_info(fd)->file;
	if(seek == NULL){
		syscall_exit(-1);
	}
	else{
		file_seek(seek,pos);
	}
}

off_t
syscall_tell(int fd)
{
	struct file* seek = find_file_info(fd)->file;
	if(seek == NULL){
		return -1;
	}
	else{
		return file_tell(seek);
	}
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  valid_addr(f->esp);
  uint32_t sysnum = *((int *)f->esp);

  switch (sysnum){
  case SYS_HALT:
  {
	  power_off();
	  break;
  }
  case SYS_EXIT:
  {
	  valid_addr(f->esp+4);
	  syscall_exit(*(int *)(f->esp+4));
	  break;
  }
  case SYS_WAIT:
  {
	  valid_addr(f->esp+4);
	  f->eax = syscall_wait(*(int *)(f->esp+4));
	  break;
  }
  case SYS_EXEC:
  {
	  valid_addr(f->esp+4);
	  f->eax = syscall_exec(*(char **)(f->esp+4));
	  break;
  }
  case SYS_OPEN:
  {
	  valid_addr((f->esp + 4));
	  f->eax = syscall_open(*(char **)(f->esp+4));
	  break;
  }
  case SYS_FILESIZE:
  {
	  valid_addr(f->esp+4);
	  f->eax = syscall_filesize(*(int *)(f->esp+4));
	  break;
  }
  case SYS_CREATE:
  {
	  valid_addr(f->esp+4);
	  valid_addr(f->esp+8);
	  f->eax = syscall_create(*(char **)(f->esp+4),*(unsigned *)(f->esp+8));
	  break;
  }
  case SYS_REMOVE:
  {
	  valid_addr(f->esp+4);
	  f->eax = syscall_remove(*(char **)(f->esp+4))?1:0;
	  break;
  }
  case SYS_READ:
  {
	  valid_addr(f->esp+4);
	  valid_addr(f->esp+8);
	  valid_addr(f->esp+12);
	  f->eax = syscall_read(*(int *)(f->esp+4),*(char **)(f->esp+8),*(unsigned *)(f->esp+12));
	  break;
  }
  case SYS_WRITE:
  {
	  valid_addr(f->esp+4);
	  valid_addr(f->esp+8);
	  valid_addr(f->esp+12);
	  f->eax = syscall_write(*(int *)(f->esp+4),*(char **) (f->esp + 8),*(unsigned *)(f->esp+12));
	  break;
  }
  case SYS_CLOSE:
  {
	  valid_addr(f->esp+4);
	  syscall_close(*(int *)(f->esp+4));
	  break;
  }
  case SYS_SEEK:
  {
	  valid_addr(f->esp+4);
	  valid_addr(f->esp+8);
	  syscall_seek(*(int *)(f->esp+4),*(unsigned *)(f->esp+8));
	  break;
  }
  case SYS_TELL:
  {
	  valid_addr(f->esp+4);
	  f->eax = syscall_tell(*(int *)(f->esp+4));
	  break;
  }
  default :
  {
	  break;
  }
}
}

