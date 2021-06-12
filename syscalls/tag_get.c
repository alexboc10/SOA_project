#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/ctype.h>

#include "../include/constants.h"

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

   if (permission != ONLY_OWNER && permission != ANY) {
      printk("%s: 'permission' argument must be ONLY_OWNER (%d) or ANY (%d)", MODNAME, ONLY_OWNER, ANY);
      return -1;
   }

   if (command == CMD_CREATE) {
      goto create;
   } else if (command == CMD_OPEN) {
      goto open;
   } else {
      printk("%s: 'command' argument must be CMD_CREATE (%d) or CMD_OPEN (%d)\n", MODNAME, CMD_CREATE, CMD_OPEN);
      return -1;
   }

create:

   /* IPC_PRIVATE = 0 */
   if (key < 0 || key > TAG_SERVICES_NUM) {
      printk("%s: 'key' argument must be a positive integer between 1 and %d or IPC_PRIVATE\n", MODNAME, TAG_SERVICES_NUM);
      return -1;
   }

   /* This function allocates an element in the TST, with 'key' as index
      and with 'permission' as visibility rule. The return value is the 
      key itself, useful for the service opening. IPC_PRIVATE as 'key' can
      be used exclusively here to istantiate a service visible just for
      the creator process */
   ret = create_service(key, permission);
   return ret; /* key or -1 (error) */

open:

   if (key <= 0 || key > TAG_SERVICES_NUM) {
      printk("%s: 'key' argument must be a positive integer between 1 and %d\n", MODNAME, TAG_SERVICES_NUM);
      return -1;
   }

   /* This function opens an existing tag service with 'key' as index only 
      if the calling thread has the correct privileges. This time the return 
      value is the tag service descriptor, useful for tag service operations. 
      Here IPC_PRIVATE is not available. */
   ret = open_service(key, permission);
   return ret; /* tag or -1 (error) */
}

unsigned long tag_get_addr(void) {
   return (unsigned long) __x64_sys_tag_get;
}

