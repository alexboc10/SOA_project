#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include "../include/constants.h"

#define LIBNAME "TAG_CTL"

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

