#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>

#include "../include/TST_handler.h"
#include "../include/constants.h"

/* This code is based on Francesco Quaglia example. It has been adapted for the specific
   purpose of the project. */

#define YES 1
#define NO 0

long sys_goto_sleep(level_t *level) {
   volatile wq_t me;
   wq_t *aux;

   DECLARE_WAIT_QUEUE_HEAD(the_queue);//here we use a private queue - wakeup is selective via wake_up_process

   me.next = NULL;
   me.task = current;
   me.pid  = current->pid;
   me.awake = NO;
   me.already_hit = NO;

   printk("%s: sys_goto_sleep on strong fifo sleep/wakeup queue called from thread %d\n", MODNAME, current->pid);

   preempt_disable();

   spin_lock(&(level->queue_lock));

   aux = &(level->head);
   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue - service damaged\n", MODNAME);
      return -1;
   }

   for (;aux;) {//put my record at the tail
      if (aux->next == NULL) {
         aux->next = &me;
         goto sleep;
      }
      aux = aux->next;
   }

sleep:
   spin_unlock(&(level->queue_lock));

   preempt_enable();

   printk("%s: thread %d actually going to sleep\n", MODNAME, current->pid);

   wait_event_interruptible(the_queue, me.awake == YES);

   preempt_disable();//all preempt enable/disable calls in this module are redundant - stand here as an example of usage

   spin_lock(&(level->queue_lock));

   aux = &(level->head);
   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue upon wakeup - service damaged\n", MODNAME);
      return -1;
   }

   for (;aux;) {//find and remove my record
      if (aux->next != NULL) {
         if (aux->next->pid == current->pid) {
            aux->next = me.next;
            break;
         }
      }
      aux = aux->next;
   }

   spin_unlock(&(level->queue_lock));
   preempt_enable();

   printk("%s: thread %d exiting sleep for a wakeup or signal\n", MODNAME, current->pid);

   if (me.awake == NO) {
      printk("%s: thread %d exiting sleep for signal\n", MODNAME, current->pid);
      return -EINTR;
   }

   return 0;
}

long sys_awake(level_t *level) {
   struct task_struct *the_task;
   int its_pid = -1;
   wq_t *aux;

   printk("%s: sys_awake called from thread %d\n", MODNAME, current->pid);

   aux = &(level->head);

   preempt_disable();

   spin_lock(&(level->queue_lock));

   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue\n", MODNAME);
      return -1;
   }

   while (aux->next) {
      if (aux->next->already_hit == NO) {
         the_task = aux->next->task;
         aux->next->awake = YES;
         aux->next->already_hit = YES;
         its_pid = aux->next->pid;
         wake_up_process(the_task);

         goto awaken;
      }
      aux = aux->next;
   }

   spin_unlock(&(level->queue_lock));

   preempt_enable();

   return 0;

awaken:
   spin_unlock(&(level->queue_lock));

   preempt_enable();

   printk("%s: called the awakening of thread %d\n", MODNAME, its_pid);

   return its_pid;
}
