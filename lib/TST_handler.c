#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/uidgid.h>

#include "../include/constants.h"
#include "../include/messages_list.h"
#include "../include/TST_handler.h"

#define LIBNAME "TST_HANDLER"

extern long sys_goto_sleep(level_t *);
extern long sys_awake(level_t *);

/* The TST is kept in memory in order to handle the services.
   Every element is a tag_service object */
static tag_t *tst = NULL;

/* This array keeps the usage status of the services */
static volatile int tst_status[TAG_SERVICES_NUM] = {[0 ... (TAG_SERVICES_NUM-1)] = 0};

/* The first TST index free for tag service allocation */
int first_index_free = -1;

void TST_dealloc(void) {
   kfree(tst);
}

int TST_alloc(void) {

   /* TST allocation */
   tst = (tag_t *) kmalloc(TAG_SERVICES_NUM*sizeof(tag_t), GFP_KERNEL);
   if (tst == NULL) {
      printk("%s: unable to allocate the TST\n", LIBNAME);
      return -1;
   }

   memset(tst, 0, TAG_SERVICES_NUM*sizeof(tag_t));

   first_index_free = 0;

   return 0;
}

int create_service(int key, int permission) {

   /* Serialized access to TST */
   spin_lock(&tst_spinlock);

   /* The TST is full */
   if (first_index_free == -1) {
      spin_unlock(&tst_spinlock);
      printk("%s: the TST is full. Allocation failed\n", LIBNAME);
      return -1;
   }

   tag_t new_service;

   /* The module istantiates a private tag service, accessible (and, then, openable) 
      only by the threads of the creator process */
   if (key == IPC_PRIVATE) {
      new_service.key = first_index_free;
      /* This is the key the thread can use to open the process-limited service */
      key = first_index_free + 1;
      new_service.priv = 1;
   } else {
      if (tst_status[key-1] == 1) {
         spin_unlock(&tst_spinlock);
         printk("%s: the tag service with the specified key already exists. Allocation failed\n", LIBNAME);
         return -1;
      }

      new_service.key = key-1;
      new_service.priv = 0;
   }

   /* The IPC_PRIVATE resource can be accessed only by thread belonging to the
      creator process. So the resource has a process visibility */
   new_service.owner = current->tgid;

   /* The service can be accessed and used by any user or only by a specified user */
   if (permission == ANY) {
      new_service.perm = -1;
   } else if (permission == ONLY_OWNER) {
      new_service.perm = current->cred->uid.val;
   }

   /* Service allocated */
   tst[new_service.key] = new_service;
   tst_status[new_service.key] = 1;

   int i;
   /* Updating the first free index variable */
   for (i=first_index_free+1; i<TAG_SERVICES_NUM; i++) {
      if (tst_status[i] != 0) {
         first_index_free = i;
      }
      if (i == TAG_SERVICES_NUM-1) {
         first_index_free = -1;
      }
   }

   spin_unlock(&tst_spinlock);

   /* The return value is the key (not the tag), since
      the service is not still usable */
   return key;

}

int check_privileges(int key) {
   tag_t the_tag_service = tst[key-1];

   /* IPC_PRIVATE used during the istantiation */
   if (the_tag_service.priv == 1 && the_tag_service.owner != current->tgid) {
      printk("%s: the process cannot use this tag service.\n", LIBNAME);
      return -1;
   }

   /* Service not accessible by all the users */
   if (the_tag_service.perm != -1 && the_tag_service.perm != current->cred->uid.val) {
      printk("%s: the user cannot use this tag service\n", LIBNAME);
      return -1;
   }

   return 0;
}

int open_service(int key, int permission) {

   spin_lock(&tst_spinlock);

    /* The service does not exist */
   if (tst_status[key-1] == 0) {
      spin_unlock(&tst_spinlock);
      printk("%s: the specified key does not match any existing tag service. Opening failed\n", LIBNAME);
      return -1;
   }

   if (check_privileges(key) == -1) {
      spin_unlock(&tst_spinlock);
      printk("%s: opening denied\n", LIBNAME);
      return -1;
   }

   spin_unlock(&tst_spinlock);

   /* The return value is the tag. It can be actually used
      for operations */
   return key-1;
}

int send_message(int key, int level, char *buffer, size_t size) {
   tag_t *tag;
   level_t *level_obj;

   if (tst_status[key-1] == 0) {
      printk("%s: the specified tag does not exist. Operation failed\n", LIBNAME);
      return -1;
   }

   if (check_privileges(key) == -1) {
      printk("%s: operation denied\n", LIBNAME);
      return -1;
   }

   level_obj = &(tag->levels[level]);

   //Prepare the message structure
   int waiting_threads = level_obj->waiting_threads;

   msg_t message;
   message.size = size;
   message.reading_threads = waiting_threads;
   message.msg = (char *) vmalloc(size);

   if (!message.msg) {
      printk("%s: error while allocating memory with vmalloc.\n", LIBNAME);
      return -1;
   }

   strncpy(message.msg, buffer, size);

   spin_lock(&(tag->level_activation_spinlock[level]));
   if(level_obj->active == 0){
      rcu_messages_list_init(&(level_obj->msg_list));
      level_obj->active = 1;
      level_obj->first_elem.task = NULL;
      level_obj->first_elem.pid = -1;
      level_obj->first_elem.awake = -1;
      level_obj->first_elem.already_hit = -1;
      level_obj->first_elem.next = NULL;

      spin_lock_init(&(level_obj->queue_spinlock));
      spin_lock_init(&(level_obj->msg_spinlock));

      asm volatile("mfence");
   }

   spin_unlock(&(tag->level_activation_spinlock[level]));
   spin_lock(&(level_obj->msg_spinlock));

   if (level_obj->waiting_threads == 0) {
      printk("%s: no waiting threads. The message will not be sent\n", LIBNAME);
   } else {
      msg_t *res = rcu_messages_list_insert(&(level_obj->msg_list), message);
      int awakened_pid;

      asm volatile("mfence");

      awakened_pid = -1;

      while (awakened_pid != 0) {
         awakened_pid = sys_awake(level_obj);
         printk("%s: thread %d woke up thread %d\n", LIBNAME, current->pid, awakened_pid);
      }
   }

   spin_unlock(&(level_obj->msg_spinlock));

   return 0;
}

int receive_message(int key, int level, char* buffer, size_t size) {
   tag_t *tag;
   level_t *level_obj;

   if (tst_status[key-1] == 0) {
      printk("%s: the specified tag does not exist. Operation failed\n", LIBNAME);
      return -1;
   }

   if (check_privileges(key) == -1) {
      printk("%s: operation denied\n", LIBNAME);
      return -1;
   }

   level_obj = &(tag->levels[level]);

   spin_lock(&(tag->level_activation_spinlock[level]));

   if (level_obj->active == 0) {
      level_obj->active = 1;
   }

   spin_unlock(&(tag->level_activation_spinlock[level]));

   /* There is one more thread waiting for a message receiving */
   level_obj->waiting_threads++;

   printk("%s: request to sleep from thread %d\n", LIBNAME, current->pid);

   /* The thread requests for a sleep, waiting for the message receiving */
   sys_goto_sleep(level_obj);

   printk("%s: thread %d awekened\n", LIBNAME, current->pid);

   spin_lock(&(level_obj->msg_spinlock));

   msg_t *the_msg = level_obj->msg_list.head;

   if (the_msg == NULL) {
      spin_unlock(&(level_obj->msg_spinlock));
      asm volatile("mfence");
      printk("%s: no available messages for thread %d\n", LIBNAME, current->pid);
      level_obj->waiting_threads--;
      return -1;
   }

   /* If the msg is smaller than the expected, size is fixed */
   if (size > the_msg->size) {
      size = the_msg->size;
   }

   strncpy(buffer, the_msg->msg, size);
   buffer[size] = '\0';
   printk("%s: message read by thread %d\n", LIBNAME, current->pid);

   int counter;
   /* RCU list handling */
   counter = the_msg->reading_threads-1;
   if (counter == 0) {
      rcu_messages_list_remove(&(level_obj->msg_list), the_msg);
   }

   level_obj->waiting_threads--;

   spin_unlock(&(level_obj->msg_spinlock));

   return 0;
}
