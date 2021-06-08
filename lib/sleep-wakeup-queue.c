#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>

/* This code is based on Francesco Quaglia example. It has been adapted for the specific
   purpose of the project. */

#define LIBNAME "SW_QUEUE_HANDLER"

#define NO (0)
#define YES (NO+1)
#define AUDIT if(1)

long sys_goto_sleep(level_t *level) {
   volatile wq_t me;
   wq_t *aux;

   DECLARE_WAIT_QUEUE_HEAD(the_queue);//here we use a private queue - wakeup is selective via wake_up_process

   me.next = NULL;
   me.task = current;
   me.pid  = current->pid;
   me.awake = NO;
   me.already_hit = NO;

   AUDIT
   printk("%s: sys_goto_sleep on strong fifo sleep/wakeup queue called from thread %d\n",LIBNAME,current->pid);

   preempt_disable();

   spin_lock(&(level->queue_lock));

   aux = &(level->first_elem);
   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue - service damaged\n",LIBNAME);
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

   AUDIT
   printk("%s: thread %d actually going to sleep\n",LIBNAME,current->pid);

   wait_event_interruptible(the_queue, me.awake == YES);

   preempt_disable();//all preempt enable/disable calls in this module are redundant - stand here as an example of usage

   spin_lock(&(level->queue_lock));

   aux = &(level_first_elem);
   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue upon wakeup - service damaged\n",LIBNAME);
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

   AUDIT
   printk("%s: thread %d exiting sleep for a wakeup or signal\n",LIBNAME, current->pid);

   if (me.awake == NO) {
      AUDIT
      printk("%s: thread %d exiting sleep for signal\n",LIBNAME, current->pid);
      return -EINTR;
   }

   return 0;
}

long sys_awake(level_t *level) {
   struct task_struct *the_task;
   int its_pid = -1;
   wq_t *aux;

   printk("%s: sys_awake called from thread %d\n",LIBNAME,current->pid);

   aux = &(level->first_elem);

   preempt_disable();

   spin_lock(&(level->queue_lock));

   if (aux == NULL) {
      spin_unlock(&(level->queue_lock));
      preempt_enable();
      printk("%s: malformed sleep-wakeup-queue\n",LIBNAME);
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

   AUDIT
   printk("%s: called the awake of thread %d\n",LIBNAME,its_pid);

   return its_pid;
}
