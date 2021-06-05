#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
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
#include "../data/constants.h"
#include "../data/structures.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("Library for TAG service table management");

#define LIBNAME "TST_HANDLER"

int create_service(int, int);
EXPORT_SYMBOL(create_service);

int open_service(int, int);
EXPORT_SYMBOL(open_service);

int TST_alloc(void);
EXPORT_SYMBOL(TST_alloc);

void TST_dealloc(void);
EXPORT_SYMBOL(TST_dealloc);

DEFINE_MUTEX(tst_mutex);

/* The TST is kept in memory in order to handle the service.
   Every element is a tag_service object */
struct tag_service *tst = NULL;

/* The first TST index free for tag service allocation */
int first_index_free;

void TST_dealloc(void) {
   kfree(tst);
}

int TST_alloc(void) {

   /* TST allocation */
   tst = (struct tag_service *) kmalloc(TAG_SERVICE_NUM*sizeof(struct tag_service), GFP_KERNEL);
   if (tst == NULL) {
      printk("%s: unable to allocate TST\n", LIBNAME);
      return -1;
   }

   memset(tst, 0, TAG_SERVICE_NUM*sizeof(struct tag_service));

   first_index_free = 0;

   return 0;
}

int find_key(int key) {
   int i;

   for (i=0; i<TAG_SERVICE_NUM; i++) {
      if (&tst[i] != NULL) {
         if (tst[i].key == key) {
            return i;
         }
      }
   }

   return -1;

}

int is_accessible(int tsd) {

   if (tst[tsd].owner == ANY) {
      return 0;
   }

   if (tst[tsd].owner != ANY && tst[tsd].owner == (int) get_current_user()->uid.val) {
      return 0;
   }

   return -1;

}

int is_key_free(int key) {
   int i;

   for (i=0; i<TAG_SERVICE_NUM; i++) {
      if (&tst[i] != NULL) {
         if (tst[i].key == key) {
            return -1;
         }
      }
   }

   return 0;

}

int create_service(int key, int permission) {
   int tsd = -1;

   /* Serialized access to TST updates */
   mutex_lock(&tst_mutex);

   /* Key already used */
   if (!(is_key_free(key)) && key != IPC_PRIVATE) {
      mutex_unlock(&tst_mutex);
      printk("%s: the service with key %d is already used. Allocation failed\n", LIBNAME, key);
      return -1;
   }

   /* No entry available */
   if (first_index_free == -1) {
      mutex_unlock(&tst_mutex);
      printk("%s: the TST is full. Allocation failed\n", LIBNAME);
      return -1;
   }

   struct tag_service new_service;
   new_service.key = key;
   /* The service is close at the beginning */
   new_service.open = 0;
   if (permission == ANY) {
      new_service.owner = 0;
   } else {
      new_service.owner = (int) get_current_user()->uid.val;
   }

   tst[first_index_free] = new_service;
   tsd = first_index_free;

   int i;
   /* Updating the first free index variable */
   for (i=first_index_free+1; i<TAG_SERVICE_NUM; i++) {
      if (&tst[i] != NULL) {
         first_index_free = i;
      }
      if (i == TAG_SERVICE_NUM-1) {
         first_index_free = -1;
      }
   }

   mutex_unlock(&tst_mutex);

   return tsd;

}

int open_service(int key, int permission) {
   int tsd = -1;

   if (key == IPC_PRIVATE) {
      tsd = create_service(key, LIMITED);
      mutex_lock(&tst_mutex);
   } else {
      mutex_lock(&tst_mutex);
      tsd = find_key(key);
   }

   /* The service does not exist */
   if (tsd == -1) {
      mutex_unlock(&tst_mutex);
      printk("%s: the specified key does not match any existing tag service. Opening failed\n", LIBNAME);
      return -1;
   }

   if (is_accessible(tsd) == -1) {
      mutex_unlock(&tst_mutex);
      printk("%s: the specified service is not accessible by the current user. Opening failed\n", LIBNAME);
      return -1;
   }

   if (tst[tsd].open == 1 && tst[tsd].key == IPC_PRIVATE) {
      mutex_unlock(&tst_mutex);
      printk("%s: the specified service cannot be reopened\n", LIBNAME);
      return -1;
   }

   tst[tsd].open = 1;

   mutex_unlock(&tst_mutex);

   return tsd;
}
