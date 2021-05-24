#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define MODULE_NAME "TAG_RECEIVE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_receive syscall");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(4, _tag_receive, int, tag, int, level, char *, buffer, size_t, size) {
#else
asmlinkage int sys_tag_receive(int tag, int level, char *buffer, size_t size) {
#endif

   return 0;
}

static int __init install(void) {

   return 0;
}

static void __exit uninstall(void) {

}

module_init(install);
module_exit(uninstall);
