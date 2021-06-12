#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../include/constants.h"

extern int receive_message(int, int, char *, size_t);

unsigned long tag_receive_addr(void);
EXPORT_SYMBOL(tag_receive_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_receive, int, tag, int, level, char *, buffer, size_t, size) {
#else
asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size) {
#endif
   int ret;

   if (tag < 0 || tag > (TAG_SERVICES_NUM-1)) {
      printk("%s: the specified tag is not valid\n", MODNAME);
      return -1;
   }

   if (level < 0 || level >= LEVELS) {
      printk("%s: the specified level is not valid\n", MODNAME);
      return -1;
   }

   if ((strlen(buffer) < (int) size) || (size > MAX_MSG_SIZE) || (strlen(buffer) > MAX_MSG_SIZE)) {
      printk("%s: the size of the message must be lower or equal than buffer size and lower than %d byte\n", MODNAME, MAX_MSG_SIZE);
      return -1;
   }

   ret = receive_message(tag+1, level, buffer, size);

   return ret;
}

unsigned long tag_receive_addr(void) {
   return (unsigned long) __x64_sys_tag_receive;
}
