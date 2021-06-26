#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "../include/constants.h"

extern char *receive_message(int, int, char *, size_t);

unsigned long tag_receive_addr(void);
EXPORT_SYMBOL(tag_receive_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_receive, int, tag, int, level, char *, buffer, size_t, size) {
#else
asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size) {
#endif
   char *msg;
   int err;

   if (tag < 0 || tag > (TAG_SERVICES_NUM-1)) {
      printk("%s: thread %d - the specified tag is not valid\n", MODNAME, current->pid);
      return -1;
   }

   if (level < 0 || level >= LEVELS) {
      printk("%s: thread %d - the specified level is not valid\n", MODNAME, current->pid);
      return -1;
   }

   char kern_buffer[size];
   int dim = sizeof(kern_buffer) / sizeof(kern_buffer[0]);

   printk("%s: thread %d - kern_buffer allocated\n", MODNAME, current->pid);

   if ((dim < (int) size) || (size > MAX_MSG_SIZE) || (dim > MAX_MSG_SIZE)) {
      printk("%s: thread %d - -the size of the message must be lower or equal than buffer size and lower than %d byte\n", MODNAME, current->pid, MAX_MSG_SIZE);
      return -1;
   }

   /* Message receiving after a waiting period */
   msg = receive_message(tag+1, level, kern_buffer, size);

   /* If the message exists, it is copied in the user level buffer */
   if (msg != NULL) {
      err = copy_to_user(buffer, msg, size);
      if (err != -1) {
         return 0;
      } else {
         return -1;
      }
   } else {
      return -1;
   }
}

unsigned long tag_receive_addr(void) {
   return (unsigned long) __x64_sys_tag_receive;
}
