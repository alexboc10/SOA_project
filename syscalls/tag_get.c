#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/ctype.h>

#include "../data/constants.h"
#include "../data/structures.h"

#define LIBNAME "TAG_GET"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("tag_get syscall");

extern int open_service(int, int);
extern int create_service(int, int);

unsigned long tag_get_addr(void);
EXPORT_SYMBOL(tag_get_addr);

/* This syscall allows to create/open a tag service.  */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _tag_get, int, key, int, command, int, permission) {
#else
asmlinkage int sys_tag_get(int key, int command, int permission) {
#endif
   int ret;

   if (!(isdigit(key)) || key < 0) {
      printk("%s: 'key' argument must be a positive integer or IPC_PRIVATE\n", LIBNAME);
      return -1;
   }

   if (permission != LIMITED && permission != ANY) {
      printk("%s: 'permission' argument must be LIMITED (%d) or ANY (%d)", LIBNAME, LIMITED, ANY);
      return -1;
   }

   if (command == CMD_CREATE) {
      /* The thread wants to istantiate a new TAG service, indexing it with
         a 'key' (not existing) and with permission options (optional).
         The istantiation will be done inside the TST, making it visible
         for all the threads. */
      goto create;
   } else if (command == CMD_OPEN) {
      goto open;
   } else {
      printk("%s: 'command' argument must be CMD_CREATE (%d) or CMD_OPEN (%d)\n", LIBNAME, CMD_CREATE, CMD_OPEN);
      return -1;
   }

create:

   /* This function allocates an element in the TST, with 'key' as index
      and with 'permission' as visibility rule. The return value is the 
      tag service descriptor */

   if (key == IPC_PRIVATE) {
      printk("%s: IPC_PRIVATE cannot be used in tag service creation\n", LIBNAME);
      return -1;
   }

   ret = create_service(key, permission);
   return ret; /* tag service descriptor or -1 */

open:

   /* This function opens an existing tag service with 'key' (that actually is the
      tag service descriptor) as index only if the calling thread has the correct 
      privileges. This time the return value is the tag service descriptor, useful 
      for tag service operations. IPC_PRIVATE can be used as 'key' only for this
      system call (the explicit creation of a new service is not necessary): in
      that case, the visibility is limited to current user itself. */
   ret = open_service(key, permission);
   return ret; /* tag service descriptor or -1 */
}

unsigned long tag_get_addr(void) {
   return (unsigned long) __x64_sys_tag_get;
}

