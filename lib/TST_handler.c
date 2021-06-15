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
static tag_t *tst = NULL;

/* This array keeps the usage status of the services */
static volatile int tst_status[TAG_SERVICES_NUM] = {[0 ... (TAG_SERVICES_NUM-1)] = 0};

/* The first TST index free for tag service allocation */
static volatile int first_index_free = -1;

int get_tag_status(int index) {

   if (index < 0 || index > (TAG_SERVICES_NUM-1)) {
      return -1;
   }

   return tst_status[index];

}

tag_t *get_tag_service(int index) {

   if (index < 0 || index > (TAG_SERVICES_NUM-1)) {
      return NULL;
   }

   return &(tst[index]);

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
      printk("%s: the TST is full. Allocation failed\n", MODNAME);
      return -1;
   }

   new_service = (tag_t *) vmalloc(sizeof(tag_t));

   /* The module istantiates a private tag service, accessible (and, then, openable) 
      only by the threads of the creator process */
   if (key == IPC_PRIVATE) {
      new_service->key = first_index_free;
      /* This is the key the thread can use to open the process-limited service */
      key = first_index_free + 1;
      new_service->priv = 1; /* This means the owner checking is required */
   } else {
      if (tst_status[key-1] == 1) {
         spin_unlock(&tst_spinlock);
         printk("%s: the tag service with key %d already exists. Allocation failed\n", MODNAME, key);
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
   rwlock_init(&(new_service->send_lock));
   rwlock_init(&(new_service->receive_lock));

   /* All the tag service levels are initialized */
   level_t *level_obj;
   for (i=0; i<LEVELS; i++) {
      level_obj = &(new_service->levels[i]);

      /* Just the minimum needed is initialized for every level */
      spin_lock_init(&(new_service->level_activation_lock[i]));
      level_obj->active = 0;
   }

   /* Service allocated */
   tst[new_service->key] = *new_service;
   tst_status[new_service->key] = 1;

   /* Updating the first free index variable */
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

   printk("%s: tag service with key %d correctly created\n", MODNAME, key);

   /* The return value is the key (not the tag), since
      the service is not still usable */
   return key;

}

int check_privileges(int key) {
   tag_t *the_tag_service = &(tst[key-1]);

   /* IPC_PRIVATE used during the istantiation */
   if (the_tag_service->priv == 1 && the_tag_service->owner != current->tgid) {
      printk("%s: the thread %d cannot use the tag service with key %d\n", MODNAME, current->tgid, key);
      return -1;
   }

   /* Service not accessible by all the users */
   if (the_tag_service->perm != -1 && the_tag_service->perm != current->cred->uid.val) {
      printk("%s: the user %d cannot use the tag service with key %d\n", MODNAME, current->cred->uid.val, key);
      return -1;
   }

   if (the_tag_service->open == 0) {
      printk("%s: the tag service with key %d is closed\n", MODNAME, key);
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
      printk("%s: the key %d does not match any existing tag service. Opening failed\n", MODNAME, key);
      return -1;
   }

   /* If the service is already open, the tag is anyway returned */
   if (check_privileges(key) == -1) {
      spin_unlock(&tst_spinlock);
      printk("%s: tag service opening with key %d denied\n", MODNAME, key);
      return -1;
   }

   the_tag_service = &(tst[key-1]);

   /* Tag service actual opening */
   the_tag_service->open = 1;

   asm volatile("mfence");

   printk("%s: the service with tag %d is now open\n", MODNAME, key);

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
      printk("%s: the specified tag does not exist. Operation failed\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if ((err = check_privileges(key)) == -1) {
      printk("%s: sending operation denied\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (err == EOPEN) {
      printk("%s: the service is closed. It is not usable\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &(tst[key-1]);

   /* The order is inverted for access correctness */
   read_lock(&(tag->send_lock));
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
      printk("%s: error in memory allocation for sending message\n", MODNAME);
      read_unlock(&(tag->send_lock));
      return -1;
   }

   /* The message is copied into the service buffer */
   strncpy(message.msg, buffer, size);

   spin_lock(&(level_obj->msg_lock));

   if (level_obj->waiting_threads == 0) {
      printk("%s: no waiting threads. The message will not be sent\n", MODNAME);
   } else {
      /* The message sending awakes the waiting threads (that ones on the specific level).
         They will read the message through RCU mechanism */
      int res = rcu_messages_list_insert(&(level_obj->msg_list), message);

      if (res == -1) {
         spin_unlock(&(level_obj->msg_lock));
         read_unlock(&(tag->send_lock));

         printk("%s: error in RCU message insertion\n", MODNAME);

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
      printk("%s: the specified tag does not exist. Operation failed\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return NULL;
   }

   if ((err = check_privileges(key)) == -1) {
      printk("%s: receiving operation denied\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return NULL;
   }

   if (err == EOPEN) {
      printk("%s: the service is closed. It is not usable\n", MODNAME);
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

   printk("%s: thread %d wants to sleep\n", MODNAME, current->pid);

   read_unlock(&(tag->receive_lock));

   /* The thread requests for a sleep, waiting for the message receiving */
   sys_goto_sleep(level_obj);

   read_lock(&(tag->receive_lock));

   /* This is the first instruction performed after the awakening */
   printk("%s: thread %d resumed\n", MODNAME, current->pid);

   spin_lock(&(level_obj->msg_lock));

   msg_t *the_msg = level_obj->msg_list.head;

   /* The thread is resumed but there is no available message */
   if (the_msg == NULL) {
      spin_unlock(&(level_obj->msg_lock));
      asm volatile("mfence");

      printk("%s: no available messages for thread %d\n", MODNAME, current->pid);
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
   printk("%s: message read by thread %d\n", MODNAME, current->pid);

   int reading_threads;
   reading_threads = the_msg->reading_threads-1;

   /* The message has been read by all the interested threads */
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
      printk("%s: the specified tag does not exist. Operation failed\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if ((err = check_privileges(key)) == -1) {
      printk("%s: awakening operation denied\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (err == EOPEN) {
      printk("%s: the service is closed. It is not usable\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &tst[key-1];

   spin_lock(&(tag->awake_all_lock));

   //check for waiting threads first
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
      printk("%s: the specified tag does not exist. Operation failed\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   if (check_privileges(key) == -1) {
      printk("%s: removing operation denied\n", MODNAME);
      spin_unlock(&tst_spinlock);
      return -1;
   }

   tag = &tst[key-1];

   /* Total locking is needed in order to secure a correct removal */
   spin_lock(&(tag->awake_all_lock));
   write_lock(&(tag->send_lock));
   write_lock(&(tag->receive_lock));

   //check for waiting threads first
   for (i=0; i<LEVELS; i++) {
      if (tag->levels[i].active == 1) {
         if (tag->levels[i].waiting_threads != 0) {
            printk("%s: the tag cannot be removed because there are threads waiting for messages at level %d.\n", MODNAME, i);
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

   if (first_index_free == -1 || (key-1) < first_index_free) {
      first_index_free = key-1;
   }

   printk("%s: the tag has been correctly removed\n", MODNAME);

   write_unlock(&(tag->receive_lock));
   write_unlock(&(tag->send_lock));

   spin_unlock(&(tag->awake_all_lock));

   spin_unlock(&tst_spinlock);

   return 0;
}
