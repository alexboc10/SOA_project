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

   printk("Sono tag_send!!!\n");

   return 0;
}

unsigned long tag_send_addr(void) {
   return (unsigned long) __x64_sys_tag_send;
}
