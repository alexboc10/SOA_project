#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define LIBNAME "TAG_RECEIVE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_receive syscall");

unsigned long tag_receive_addr(void);
EXPORT_SYMBOL(tag_receive_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_receive, int, tag, int, level, char *, buffer, size_t, size) {
#else
asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size) {
#endif

   printk("Sono tag_receive!!!\n");

   return 0;
}

unsigned long tag_receive_addr(void) {
   return (unsigned long) __x64_sys_tag_receive;
}
