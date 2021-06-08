#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define LIBNAME "TAG_SEND"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_send syscall");

unsigned long tag_send_addr(void);
EXPORT_SYMBOL(tag_send_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_send, int, tag, int, level, char *, buffer, size_t, size) {
#else
asmlinkage int sys_tag_send(int tag, int level, char *buffer, size_t size) {
#endif
   int ret;

   if (tag < 0 || tag > (TAG_SERVICES_NUM-1)) {
      printk("%s: the specified tag is not valid\n", LIBNAME);
      return -1;
   }

   if (level < 0 || level >= LEVELS) {
      printk("%s: the specified level is not valid\n", LIBNAME);
      return -1;
   }

   if (strlen(buffer) < (int) size) {
      printk("%s: the size of the message must be lower or equal than buffer size\n", LIBNAME);
      return -1;
   }

   ret = send_message(tag+1, level, buffer, size);

   return ret;
}

unsigned long tag_send_addr(void) {
   return (unsigned long) __x64_sys_tag_send;
}
