#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../include/constants.h"

unsigned long tag_ctl_addr(void);
EXPORT_SYMBOL(tag_ctl_addr);

extern int awake_all_threads(int);
extern int remove_tag(int);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _tag_ctl, int, tag, int, command) {
#else
asmlinkage int sys_tag_ctl(int tag, int command) {
#endif
   int ret;

   if (tag < 0 || tag > (TAG_SERVICES_NUM-1)) {
      printk("%s: thread %d - the specified tag is not valid\n", MODNAME, current->pid);
      return -1;
   }

   if (command == AWAKE_ALL) {
      goto awake;
   } else if (command == REMOVE) {
      goto remove;
   } else {
      printk("%s: thread %d - 'command' argument must be AWAKE_ALL (%d) or REMOVE (%d)\n", MODNAME, current->pid, AWAKE_ALL, REMOVE);
      return -1;
   }

awake:

   /* This function tries to awake all the threads waiting on a specific tag service, if
      existing. */
   ret = awake_all_threads(tag+1);

   return ret;

remove:

   /* This function deallocates, if existing, the specified tag service */
   ret = remove_tag(tag+1);

   return ret;
}

unsigned long tag_ctl_addr(void) {
   return (unsigned long) __x64_sys_tag_ctl;
}

