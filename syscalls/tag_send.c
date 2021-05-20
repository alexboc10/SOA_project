#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define MODULE_NAME "TAG_GET"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_get syscall");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _tag_get, int, key, int, command, int, permission) {
#else
asmlinkage int sys_tag_get(int key, int command, int permission) {
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
