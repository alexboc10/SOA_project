#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "data/constants.h"
#include "data/structures.h"

#define MODULE_NAME "TBDE"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("TAG-based data exchange. A message is sent/received according to tag+level matching");

extern void get_syscalls_addresses(void);
extern int install_syscalls(void);
extern int uninstall_syscalls(void);
extern int TST_alloc(void);
extern int TST_dealloc(void);

static int __init install(void) {

   /* This function allocates the TST */
   TST_alloc();
   /* This function allows to gather new syscalls addresses */
   get_syscalls_addresses();
   /* This function installs the new syscalls inside the syscall table */
   install_syscalls();

   return 0;
}

static void __exit uninstall(void) {

   uninstall_syscalls();
   TST_dealloc();

}

module_init(install);
module_exit(uninstall);

