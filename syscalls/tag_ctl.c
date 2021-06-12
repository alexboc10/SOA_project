#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../include/constants.h"

#define LIBNAME "TAG_CTL"

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
      printk("%s: the specified tag is not valid\n", LIBNAME);
      return -1;
   }

   if (command == AWAKE_ALL) {
      goto awake;
   } else if (command == REMOVE) {
      goto remove;
   } else {
      printk("%s: 'command' argument must be AWAKE_ALL (%d) or REMOVE (%d)\n", LIBNAME, AWAKE_ALL, REMOVE);
      return -1;
   }

awake:

   ret = awake_all_threads(tag+1);

remove:

   ret = remove_tag(tag+1);

   return ret;
}

unsigned long tag_ctl_addr(void) {
   return (unsigned long) __x64_sys_tag_ctl;
}

