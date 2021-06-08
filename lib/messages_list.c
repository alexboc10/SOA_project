#include <linux/stat.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "../include/rcu_list.h"

#define LIBNAME "MSG_HANDLER"

/* This code is based on Francesco Quaglia example. It has been adapted
   for the specific purpose of the project */

/* A new message is inserted in the RCU list */
void rcu_messages_list_init(list_t * list) {

   list->epoch = 0;
   list->standing[0] = 0;
   list->standing[1] = 0;
   list->head = NULL;
   spin_lock_init(&l->write_spinlock);
   asm volatile("mfence");

}

msg_t *rcu_messages_list_insert(list_t *istl, msg_t message) {
   msg_t *p;
   int temp;
   int grace_epoch;

   p = vmalloc(sizeof(msg_t));

   if(!p) return 0;

   p->next = NULL;

   spin_lock(&(list->write_spinlock));

   //set element
   p->msg = (char *) vmalloc(message.size);
   if (!p->msg) {
      printk("%s: error while allocating memory with vmalloc.\n", LIBNAME);
      spin_unlock(&(list->write_spinlock));
      return NULL;
   }

   strncpy(p->msg, message.msg, message.size);
   p->size = message.size;
   p->reading_threads = message.reading_threads;

   //traverse and insert
   p->next = l->head;
   l->head = p;

   goto done;

   //move to a new epoch - still under write lock
   grace_epoch = temp = list->epoch;
   temp +=1;
   temp = temp%2;
   list->epoch = temp;
   asm volatile("mfence");

   while (list->standing[grace_epoch] > 0);

   p = list->head;

done:
   spin_unlock(&list->write_spinlock);

   return p;
}

int rcu_messages_list_remove(list_t *list, msg_t *message) {
   msg_t *p, *removed = NULL;
   int temp;
   int grace_epoch;

   spin_lock(&list->write_spinlock);

   //traverse and delete
   p = list->head;

   if (p == message) {
      removed = p;
      list->head = removed->next;
   } else {
      while (p != NULL) {
         if (p->next != NULL && p->next == message) {
            removed = p->next;
            p->next = p->next->next;
            break;
         }
         p = p->next;
      }
   }

   //move to a new epoch - still under write lock
   grace_epoch = temp = list->epoch;
   temp +=1;
   temp = temp%2;
   list->epoch = temp;
   asm volatile("mfence");

   while(list->standing[grace_epoch] > 0);

   spin_unlock(&list->write_spinlock);

   if (removed) {
      vfree(removed);
      return 1;
   }

   return 0;
}


