#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "include/sys_table_handler.h"
#include "include/TST_handler.h"
#include "include/dev_handler.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("TAG-based data exchange. A message is sent/received according to tag+level matching");

extern void get_syscalls_addresses(void);

extern int install_syscalls(void);
extern int uninstall_syscalls(void);

extern int TST_alloc(void);
extern void TST_dealloc(void);

extern int register_dev(void);
extern void unregister_dev(void);

static int __init install(void) {

   /* Initialization of TST operations spinlock */
   spin_lock_init(&tst_spinlock);

   /* TST allocation */
   TST_alloc();
   /* New syscalls addresses gathering */
   get_syscalls_addresses();
   /* New syscalls installing */
   install_syscalls();
   /* Char device registering */
   register_dev();

   return 0;
}

static void __exit uninstall(void) {

   /* Syscalls uninstalling */
   uninstall_syscalls();
   /* TST deallocation */
   TST_dealloc();
   /* Char device unregistering */
   unregister_dev();

}

module_init(install);
module_exit(uninstall);

