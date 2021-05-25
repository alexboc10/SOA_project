#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "data/constants.h"
#include "data/structures.h"

#define MODULE_NAME "TAG_SERVICE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("TAG-based thread message exchange");

extern void get_syscalls_addresses(void);
extern int install_syscalls(void);
extern int uninstall_syscalls(void);

static int __init install(void) {

   get_syscalls_addresses();
   install_syscalls();

   return 0;
}

static void __exit uninstall(void) {

   uninstall_syscalls();

}

module_init(install);
module_exit(uninstall);

