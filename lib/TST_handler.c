#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/rwlock.h>
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

extern long sys_goto_sleep(level_t *);
extern long sys_awake(level_t *);

/* The TST is kept in memory in order to handle the services.
   Every element is a tag_service object */
static volatile tag_t *tst;

/* This array keeps the usage status of the services */
static volatile int tst_status[TAG_SERVICES_NUM] = {[0 ... (TAG_SERVICES_NUM-1)] = 0};

/* The first TST index free for tag service allocation. Useful for IPC_PRIVATE services */
static volatile int first_index_free = -1;

char *status_builder(void) {
   int i, j;
   int len = 0;
   tag_t *tag_obj;
   level_t *level_obj;

   char *service_status = (char *) vmalloc(sizeof(char) * 8192);

   if (!service_status) {
      printk("%s: error while allocating memory with vmalloc.\n", MODNAME);
      return NULL;
   }

   spin_lock(&tst_spinlock);

   strcpy(service_status, "");

   for (i=0; i<TAG_SERVICES_NUM; i++) {
      if (tst_status[i] == 1) {
         tag_obj = &(tst[i]);

         printk("%s: key %d\n", MODNAME, tag_obj->key);

         if (tag_obj != NULL) {
            char tag[4];
            char creator[16];
            int key = i+1;

            printk("%s:var key: %d\n", MODNAME, key);

            /* Tag service creator */
            sprintf(creator, "%d", tag_obj->owner);
            /* Tag service identifier */
            sprintf(tag, "%d", key);

            for (j=0; j<LEVELS; j++) {
               level_obj = &(tag_obj->levels[j]);
               if (level_obj->active == 1) {
                  printk("%s:level: %d\n", MODNAME, j);

                  char level[4];
                  char waiting_threads[4];

                  /* Level considered */
                  sprintf(level, "%d", j);
                  /* Waiting threads on the considered tag service level */
                  sprintf(waiting_threads, "%d", level_obj->waiting_threads);

                  printk("%s: tag: %d - creator: %d - level: %d - waiting_threads: %d\n", MODNAME, key, tag_obj->owner, j, level_obj->waiting_threads);

                  strcat(service_status, tag);
                  len += sizeof(tag);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, creator);
                  len += sizeof(creator);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, level);
                  len += sizeof(level);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, waiting_threads);
                  len += sizeof(waiting_threads);
                  strcat(service_status, "\n");
                  len += sizeof("\n");
               }
            }
         }
      }
   }

   service_status[len] = '\0';
   len = strlen(service_status);
   spin_unlock(&tst_spinlock);

   if (len == 0) {
      printk("%s: no active service\n", MODNAME);
      return "";
   }

   char *final_status = vmalloc(sizeof(char)*len);

   if (!final_status) {
      printk("%s: error while allocating memory with vmalloc\n", MODNAME);
      vfree(service_status);
      return NULL;
   }

   strncpy(final_status, service_status, len);
   final_status[len] = '\0';
   vfree(service_status);

   return final_status;
}

void TST_dealloc(void) {
   vfree(tst);
}

int TST_alloc(void) {

   /* TST allocation */
   tst = (tag_t *) vmalloc(TAG_SERVICES_NUM*sizeof(tag_t));
   if (!tst) {
      printk("%s: unable to allocate the TST\n", MODNAME);
      return -1;
   }

   memset(tst, 0, TAG_SERVICES_NUM*sizeof(tag_t));

   first_index_free = 0;

   return 0;
}

int create_service(int key, int permission) {
   tag_t *new_service;
   int i;

   /* Serialized access to TST */
   spin_lock(&tst_spinlock);

   /* The TST is full */
   if (first_index_free == -1) {
      spin_unlock(&tst_spinlock);
      printk("%s: thread %d - the TST is full. Allocation failed\n", MODNAME, current->pid);
      return -1;
   }

   new_service = (tag_t *) vmalloc(sizeof(tag_t));

   /* The module istantiates a private tag service, accessible (and, then, openable) 
      only by the threads belonging to the creator process */
   if (key == IPC_PRIVATE) {
      new_service->key = first_index_free;
      /* This is the key the thread can use to open the process-limited service */
      key = first_index_free + 1;
      new_service->priv = 1; /* This means the owner checking is required */
   } else {
      if (tst_status[key-1] == 1) {
         vfree(new_service);
         spin_unlock(&tst_spinlock);
         printk("%s: thread %d - the tag service with key %d already exists. Allocation failed\n", MODNAME, current->pid, key);
         return -1;
      }

      new_service->key = key-1;
      new_service->priv = 0;
   }

   /* The IPC_PRIVATE resource can be accessed only by thread belonging to the
      creator process. So the resource has a process visibility */
   new_service->owner = current->tgid;
   new_service->open = 0;

   /* The service can be accessed and used by any user or only by a specified user */
   if (permission == ANY) {
      new_service->perm = -1;
   } else if (permission == ONLY_OWNER) {
      new_service->perm = current->cred->uid.val;
   }

   /* The spinlocks are initialized. They will be useful during the different operations */
   spin_lock_init(&(new_service->awake_all_lock));
   /* In sending and receiving context a read/write lock is useful in order to distinguish
      the reading operations from the writing ones. This improves the performances */
   rwlock_init(&(new_service->send_lock));
   rwlock_init(&(new_service->receive_lock));

   /* All the tag service levels are initialized */
   level_t *level_obj;
   for (i=0; i<LEVELS; i++) {
      level_obj = &(new_service->levels[i]);

      /* Just the minimum needed is initialized for every level */
      spin_lock_init(&(new_service->level_activation_lock[i]));
      /* Every level is activated only when it is necessary */
      level_obj->active = 0;
   }

   /* Service allocation */
   tst[new_service->key] = *new_service;
   tst_status[new_service->key] = 1;

   /* Updating the first_index_free variable */
   for (i=first_index_free+1; i<TAG_SERVICES_NUM; i++) {
      if (tst_status[i] == 0) {
         first_index_free = i;
         break;
      }
      if (i == TAG_SERVICES_NUM-1) {
         first_index_free = -1;
      }
   }

   spin_unlock(&tst_spinlock);

   printk("%s: thread %d - tag service with key %d correctly created\n", MODNAME, current->pid, key);

   /* The return value is the key (not the tag), since
      the service is not still usable */
   return key;

}

int check_privileges(int key) {
   tag_t *the_tag_service = &(tst[key-1]);

   /* IPC_PRIVATE used during the istantiation */
   if (the_tag_service->priv == 1 && the_tag_service->owner != current->tgid) {
      printk("%s: thread %d - Usage of the tag service with key %d denied\n", MODNAME, current->pid, key);
      return -1;
   }

   /* Service not accessible by all the users */
   if (the_tag_service->perm != -1 && the_tag_service->perm != current->cred->uid.val) {
      printk("%s: thread %d - the user %d cannot use the tag service with key %d\n", MODNAME, current->pid, current->cred->uid.val, key);
      return -1;
   }

   if (the_tag_service->open == 0) {
      printk("%s: thread %d - the tag service with key %d is closed\n", MODNAME, current->pid, key);
      return EOPEN;
   }

   return 0;
}

int open_service(int key, int permission) {
   tag_t *the_tag_service;

   spin_lock(&tst_spinlock);

    /* The service does not exist */
   if (tst_status[key-1] == 0) {
      spin_unlock(&tst_spinlock);
      printk("%s: thread %d - the key %d does not match any existing tag service. Opening failed\n", MODNAME, current->pid, key);
      return -1;
   }

   /* If the service is already open, the tag is anyway returned */
   if (check_privileges(key) == -1) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   the_tag_service = &(tst[key-1]);

   /* Tag service actual opening */
   the_tag_service->open = 1;

   asm volatile("mfence");

   printk("%s: thread %d - the service with tag %d is now open\n", MODNAME, current->pid, key);

   spin_unlock(&tst_spinlock);

   /* The return value is the tag. It can be actually used
      for operations */
   return key-1;
}

int send_message(int key, int level, char *buffer, size_t size) {
   tag_t *tag;
   level_t *level_obj;
   int err;

   spin_lock(&tst_spinlock);

   if (tst_status[key-1] == 0) {
      printk("%s: thread %d - the specified tag does not exist. Operation failed\n", MODNAME, current->pid);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if ((err = check_privileges(key)) == -1) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (err == EOPEN) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &(tst[key-1]);

   /* The order is inverted for access correctness. It can't be possible
      to access to a not existing tag service */
   read_lock(&(tag->send_lock));
   spin_unlock(&tst_spinlock);

   level_obj = &(tag->levels[level]);

   /* If required, the level object is initialized */
   spin_lock(&(tag->level_activation_lock[level]));
   if (level_obj->active == 0) {
      /* The level-specific message list is a RCU list */
      rcu_messages_list_init(&(level_obj->msg_list));

      level_obj->active = 1;
      level_obj->waiting_threads = 0;
      level_obj->head.task = NULL;
      level_obj->head.pid = -1;
      level_obj->head.awake = -1;
      level_obj->head.already_hit = -1;
      level_obj->head.next = NULL;

      /* Spinlocks for thread queueing and message storing are initialized */
      spin_lock_init(&(level_obj->queue_lock));
      spin_lock_init(&(level_obj->msg_lock));

      asm volatile("mfence");
   }

   spin_unlock(&(tag->level_activation_lock[level]));

   msg_t message;
   message.size = size;
   message.reading_threads = level_obj->waiting_threads;
   message.msg = (char *) vmalloc(size);

   if (!message.msg) {
      printk("%s: thread %d - error in memory allocation for sending message\n", MODNAME, current->pid);
      read_unlock(&(tag->send_lock));
      return -1;
   }

   /* The message is copied into the service buffer */
   strncpy(message.msg, buffer, size);

   spin_lock(&(level_obj->msg_lock));

   if (level_obj->waiting_threads == 0) {
      printk("%s: thread %d - no waiting threads. The message will not be sent\n", MODNAME, current->pid);
   } else {
      /* The message sending awakes the waiting threads (that ones on the specific level).
         They will read the message through RCU mechanism */
      int res = rcu_messages_list_insert(&(level_obj->msg_list), message);

      if (res == -1) {
         spin_unlock(&(level_obj->msg_lock));
         read_unlock(&(tag->send_lock));

         return -1;
      }

      int resumed_pid;

      asm volatile("mfence");

      resumed_pid = -1;

      /* Until there is a thread to wake up */
      while (resumed_pid != 0) {
         /* The first waiting thread is woke up */
         resumed_pid = sys_awake(level_obj);

         if (resumed_pid == 0) break;

         printk("%s: thread %d woke up thread %d\n", MODNAME, current->pid, resumed_pid);
      }
   }

   spin_unlock(&(level_obj->msg_lock));

   read_unlock(&(tag->send_lock));

   return 0;
}

char *receive_message(int key, int level, char* buffer, size_t size) {
   tag_t *tag;
   level_t *level_obj;
   int err;

   spin_lock(&tst_spinlock);

   if (tst_status[key-1] == 0) {
      printk("%s: thread %d - the specified tag does not exist. Operation failed\n", MODNAME, current->pid);
      spin_unlock(&tst_spinlock);
      return NULL;
   }

   if ((err = check_privileges(key)) == -1) {
      spin_unlock(&tst_spinlock);
      return NULL;
   }

   if (err == EOPEN) {
      spin_unlock(&tst_spinlock);
      return NULL;
   }

   tag = &(tst[key-1]);

   read_lock(&(tag->receive_lock));
   spin_unlock(&tst_spinlock);

   level_obj = &(tag->levels[level]);

   /* If required, the level object is initialized */
   spin_lock(&(tag->level_activation_lock[level]));
   if(level_obj->active == 0){
      /* The level-specific message list is a RCU list */
      rcu_messages_list_init(&(level_obj->msg_list));

      level_obj->active = 1;
      level_obj->waiting_threads = 0;
      level_obj->head.task = NULL;
      level_obj->head.pid = -1;
      level_obj->head.awake = -1;
      level_obj->head.already_hit = -1;
      level_obj->head.next = NULL;

      /* Spinlock for thread queueing and message storing are initialized */
      spin_lock_init(&(level_obj->queue_lock));
      spin_lock_init(&(level_obj->msg_lock));

      asm volatile("mfence");
   }

   spin_unlock(&(tag->level_activation_lock[level]));

   /* There is one more thread waiting for a message receiving */
   level_obj->waiting_threads++;

   printk("%s: thread %d wants to sleep waiting for a message\n", MODNAME, current->pid);

   read_unlock(&(tag->receive_lock));

   /* The thread requests for a sleep, waiting for the message receiving */
   sys_goto_sleep(level_obj);

   /* This is the next instruction executed after the awakening (if it happens) */
   read_lock(&(tag->receive_lock));
   spin_lock(&(level_obj->msg_lock));

   msg_t *the_msg = level_obj->msg_list.head;

   /* The thread is resumed but there is no available message */
   if (the_msg == NULL) {
      spin_unlock(&(level_obj->msg_lock));
      asm volatile("mfence");

      printk("%s: thread %d - no available messages\n", MODNAME, current->pid);
      level_obj->waiting_threads--;

      read_unlock(&(tag->receive_lock));

      return NULL;
   }

   /* If the msg is smaller than the expected, size is fixed */
   if (size > the_msg->size) {
      size = the_msg->size;
   }

   /* Actual message copy on thread buffer */
   strncpy(buffer, the_msg->msg, size);
   buffer[size] = '\0';
   printk("%s: thread %d - message read\n", MODNAME, current->pid);

   int reading_threads;
   reading_threads = the_msg->reading_threads-1;

   /* The message has been read by all the interested threads. So it
      can be removed from the RCU list */
   if (reading_threads == 0) {
      rcu_messages_list_remove(&(level_obj->msg_list), the_msg);
   }

   level_obj->waiting_threads--;

   spin_unlock(&(level_obj->msg_lock));

   read_unlock(&(tag->receive_lock));

   return buffer;
}

int awake_all_threads(int key) {
   tag_t *tag;
   int resumed_pid, i;
   int err;

   spin_lock(&tst_spinlock);

   if (tst_status[key-1] == 0) {
      printk("%s: thread %d - the specified tag does not exist. Operation failed\n", MODNAME, current->pid);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if ((err = check_privileges(key)) == -1) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (err == EOPEN) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &tst[key-1];

   spin_lock(&(tag->awake_all_lock));

   // Check for waiting threads
   for (i=0; i<LEVELS; i++) {
      resumed_pid = -1;
      if (tag->levels[i].active == 1) {
         level_t *level_obj = &(tag->levels[i]);
         while (resumed_pid != 0) {
            resumed_pid = sys_awake(level_obj);

            if (resumed_pid == 0) break;

            printk("%s: thread %d woke up at level %d.\n", MODNAME, resumed_pid, i);
         }
      }
   }

   spin_unlock(&(tag->awake_all_lock));

   spin_unlock(&tst_spinlock);

   return 0;
}

int remove_tag(int key) {
   tag_t *tag;
   int i;

   spin_lock(&tst_spinlock);

   if (tst_status[key-1] == 0) {
      printk("%s: thread %d - the specified tag does not exist. Operation failed\n", MODNAME, current->pid);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (check_privileges(key) == -1) {
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &tst[key-1];

   /* Total locking is needed in order to secure a correct removal */
   spin_lock(&(tag->awake_all_lock));
   write_lock(&(tag->send_lock));
   write_lock(&(tag->receive_lock));

   // Check for waiting threads
   for (i=0; i<LEVELS; i++) {
      if (tag->levels[i].active == 1) {
         if (tag->levels[i].waiting_threads != 0) {
            printk("%s: thread %d - the tag cannot be removed because there are threads waiting for messages at level %d.\n", MODNAME, current->pid, i);
            write_unlock(&(tag->send_lock));
            write_unlock(&(tag->receive_lock));
            spin_unlock(&(tag->awake_all_lock));
            spin_unlock(&tst_spinlock);

            return -1;
         }
      }
   }

   /* The tag service structure is deallocated */
   tst_status[key-1] = 0;

   /* The first_index_free variable is updated */
   if (first_index_free == -1 || (key-1) < first_index_free) {
      first_index_free = key-1;
   }

   printk("%s: thread %d - the tag has been correctly removed\n", MODNAME, current->pid);

   write_unlock(&(tag->receive_lock));
   write_unlock(&(tag->send_lock));

   spin_unlock(&(tag->awake_all_lock));

   spin_unlock(&tst_spinlock);

   return 0;
}
