#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define LIBNAME "TAG_CTL"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_ctl syscall");

unsigned long tag_ctl_addr(void);
EXPORT_SYMBOL(tag_ctl_addr);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _tag_ctl, int, tag, int, command) {
#else
asmlinkage int sys_tag_ctl(int tag, int command) {
#endif

   printk("Sono tag_ctl!!!\n");

   return 0;
}

unsigned long tag_ctl_addr(void) {
   return (unsigned long) __x64_sys_tag_ctl;
}

