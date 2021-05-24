#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define MODULE_NAME "TAG_GET"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_get syscall");

void error_handler(int error) {

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _tag_get, int, key, int, command, int, permission) {
#else
asmlinkage int sys_tag_get(int key, int command, int permission) {
#endif

   if (command == CMD_CREATE) {
      /* The thread wants to istantiate a new TAG service, indexing it with
         a 'key' (non existing) and with permission options (optional).
         The istantiation will be done inside the TST, making it visible
         for all the allowed threads. */
      goto create;
   } else if (command == CMD_OPEN) {
      goto open;
   } else {
      error_handler();
   }

create:

   /* This function looks for an existing TAG service with 'key' as index.
      If it exists, the istantiation fails. */
   if (is_key_free(key) != 0) {
      error_handler();
   }

   /* This function allocates an element in the TST, with 'key' as index
       and with 'permission', if defined */
   create_service(key, permission);

open:


   return 0;
}

static int __init install(void) {

   return 0;
}

static void __exit uninstall(void) {

}

module_init(install);
module_exit(uninstall);


