#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "include/sys_table_handler.h"
#include "include/TST_handler.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("TAG-based data exchange. A message is sent/received according to tag+level matching");

extern void get_syscalls_addresses(void);

extern int install_syscalls(void);
extern int uninstall_syscalls(void);

extern int TST_alloc(void);
extern void TST_dealloc(void);

static int __init install(void) {

   /* Initialization of TST operations spinlock */
   spin_lock_init(&tst_spinlock);

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

