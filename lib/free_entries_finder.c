#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
/* Allowing access to symbols table in order
   to discover syscall table address */
#include <linux/kallsyms.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>

#include "../data/constants.h"

/* These header files allow to read/write cr0 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0)
   #include <asm/switch_to.h>
#else
   #include <asm/system.h>
#endif

#ifndef X86_CR0_WP
   #define X86_CR0_WP 0x00010000
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("This lib allows to build and exploit a syscall table free-entries pool.");

#define LIB_NAME "FREE-ENTRIES FINDER"

/* We need just 4 free entries for our purposes */
#define NM_FREE_ENTRIES 4

int entry_indexes[NM_FREE_ENTRIES] = {[0 ... (NM_FREE_ENTRIES-1)] = -1};

unsigned long *syscall_table;
unsigned long *sys_ni_syscall_addr;

int install_syscall(unsigned long);
EXPORT_SYMBOL(install_syscall);

/* This function searches for free entries inside the system call table by identifying
   entries with the same function pointer */
int get_entries(void) {
   int i,j,z,k;
   int ret = 0;

   printk("Trying to get an entry from the syscall table\n");

   j = -1;
   for (i=0; i<NR_ENTRIES; i++) {
      for (z=i+1; z<NR_ENTRIES; z++) {
         if (syscall_table[i] == syscall_table[z]) {
            printk("%s: sys_ni_syscall located\n", LIB_NAME);
            
            sys_ni_syscall_addr = syscall_table[i];

            /* Acquiring entry at index i */
            if (j<(NM_FREE_ENTRIES-1)) {
               entry_indexes[++j] = i;
               ret++;
            }
            /* Acquiring entry at index z */
            if (j<(NM_FREE_ENTRIES-1)) {
               entry_indexes[++j] = z;
               ret++;
            }

            /* Searching for remaining entries */
            for (k=z+1; k<NR_ENTRIES && j<(NM_FREE_ENTRIES-1); k++) {
               if (syscall_table[i] == syscall_table[k]) {
                  entry_indexes[++j] = k;
                  ret++;
               }
            }

            if (ret == NM_FREE_ENTRIES) {
               goto job_done;
            }

            return -1;
         }
      }
   }

   return -1;

job_done:

   return ret;
}

/* This function install the syscall pointer in the syscall table */
int install_syscall(unsigned long new_syscall_addr) {
   int i;
   unsigned long cr0;

   /* Actual syscall table update */
   cr0 = read_cr0();
   write_cr0(cr0 & ~X86_CR0_WP);

   for (i=0; i<NM_FREE_ENTRIES; i++) {
      if (entry_indexes[i] != -1) {
         ((unsigned long *)syscall_table)[entry_indexes[i]] = new_syscall_addr;
      }
   }
   
   write_cr0(cr0);

   printk("%s: new syscall installed\n", LIB_NAME);

   return 0;
}

static int __init find_entries(void) {
   int entries;

   /* The module is available for our usage? */
   if (try_module_get(THIS_MODULE) == 0) {
      printk(KERN_ERR "Module %s not available\n", LIB_NAME);
      return -1;
   }

   if (get_sys_table() == NULL) {
      printk(KERN_INFO "%s: syscall table not found\n", LIB_NAME);
      module_put(THIS_MODULE);
      return -1;
   }
   
   /* Searching for syscall table entries available */
   entries = get_entries();
   if (entries != NM_FREE_ENTRIES) {
      printk(KERN_INFO "%s: sys_ni_siscall entries not found\n", LIB_NAME);
      module_put(THIS_MODULE);
      return -1;
   }

   return 0;
}

static void __exit cleanup_entries(void) {
   unsigned long cr0;
   int i;

   printk("%s: unistalling systemcalls\n", LIB_NAME);

   cr0 = read_cr0();
   write_cr0(cr0 & ~X86_CR0_WP);
   
   for (i=0; i<NM_FREE_ENTRIES; i++) {
      if (entry_indexes[i] != -1) {
         ((unsigned long *) syscall_table)[entry_indexes[i]] = sys_ni_syscall_addr;
      }
   }

   write_cr0(cr0);

   printk("%s: syscall table restored\n", LIB_NAME);
}

module_init(find_entries);
module_exit(cleanup_entries);
