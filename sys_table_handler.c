/* This module is based on Francesco Quaglia's code. It has been
   adapted for our purposes but it follows the guide lines of
   Quaglia repository */

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include "data/constants.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro Boccini");
MODULE_DESCRIPTION("Tag-based thread data exchange");

#define MODNAME "TAG_SERVICE"

extern int sys_vtpmo(unsigned long vaddr);

/* To migrate */
#define ADDRESS_MASK 0xfffffffffffff000

#define START 			0xffffffff00000000ULL		// use this as starting address --> this is a biased search since does not start from 0xffff000000000000
#define MAX_ADDR		0xfffffffffff00000ULL
#define FIRST_NI_SYSCALL	134
#define SECOND_NI_SYSCALL	174
#define THIRD_NI_SYSCALL	182
#define FOURTH_NI_SYSCALL	183
//#define FIFTH_NI_SYSCALL	214
//#define SIXTH_NI_SYSCALL	215
//#define SEVENTH_NI_SYSCALL	236

unsigned long *hacked_ni_syscall = NULL;
unsigned long **hacked_syscall_tbl = NULL;

unsigned long sys_call_table_address = 0x0;
//module_param(sys_call_table_address, ulong, 0660);

unsigned long sys_ni_syscall_address = 0x0;
//module_param(sys_ni_syscall_address, ulong, 0660);


int good_area(unsigned long * addr){
   int i;

   for (i=1;i<FIRST_NI_SYSCALL;i++) {
      if (addr[i] == addr[FIRST_NI_SYSCALL]) goto bad_area;
   }

   return 1;

bad_area:

   return 0;
}

/* This routine checks if the page contains the begin of the syscall_table.  */
int validate_page(unsigned long *addr){
   int i = 0;
   unsigned long page = (unsigned long) addr;
   unsigned long new_page = (unsigned long) addr;

   for(; i < PAGE_SIZE; i+=sizeof(void*)){
      new_page = page+i+SEVENTH_NI_SYSCALL*sizeof(void*);

      // If the table occupies 2 pages check if the second one is materialized in a frame
      if (((page+PAGE_SIZE) == (new_page & ADDRESS_MASK)) && (sys_vtpmo(new_page) == NO_MAP)) break;

      // go for pattern matching
      addr = (unsigned long*) (page+i);
      if(((addr[FIRST_NI_SYSCALL] & 0x3) == 0)
       && (addr[FIRST_NI_SYSCALL] != 0x0) // not points to 0x0
       && (addr[FIRST_NI_SYSCALL] > 0xffffffff00000000)	// not points to a location lower than 0xffffffff00000000
     //&& ((addr[FIRST_NI_SYSCALL] & START) == START)
       && (addr[FIRST_NI_SYSCALL] == addr[SECOND_NI_SYSCALL])
       && (addr[FIRST_NI_SYSCALL] == addr[THIRD_NI_SYSCALL])
       && (addr[FIRST_NI_SYSCALL] == addr[FOURTH_NI_SYSCALL])
     //&& (addr[FIRST_NI_SYSCALL] == addr[FIFTH_NI_SYSCALL])
     //&& (addr[FIRST_NI_SYSCALL] == addr[SIXTH_NI_SYSCALL])
     //&& (addr[FIRST_NI_SYSCALL] == addr[SEVENTH_NI_SYSCALL])
       && (good_area(addr))) {
         hacked_ni_syscall = (void*)(addr[FIRST_NI_SYSCALL]); // save ni_syscall
	 sys_ni_syscall_address = (unsigned long)hacked_ni_syscall;
	 hacked_syscall_tbl = (void*)(addr); // save syscall_table address
	 sys_call_table_address = (unsigned long) hacked_syscall_tbl;

         return 1;
      }
   }

   return 0;
}

/* This routines looks for the syscall table. */
void syscall_table_finder(void){
   unsigned long k; // current page
   unsigned long candidate; // current page

   for (k=START; k < MAX_ADDR; k+=4096) {
      candidate = k;
      /* Check if the candidate virtual page is mapped in frame */
      if (sys_vtpmo(candidate) != NO_MAP) {
         /* Check if candidate maintains the syscall_table */
	 if (validate_page((unsigned long *)(candidate))) {
	    printk("%s: syscall table found at %px\n",MODNAME,(void*)(hacked_syscall_tbl));
	    printk("%s: sys_ni_syscall found at %px\n",MODNAME,(void*)(hacked_ni_syscall));
	    break;
	 }
      }
   }
}

int free_entries[NR_NEW_SYSCALLS];
//module_param_array(free_entries,int,NULL,0660);//default array size already known - here we expose what entries are free

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _tag_get, int, key, int, command, int, permission) {
#else
asmlinkage int sys_tag_get(int key, int command, int permission) {
#endif

        printk("%s: thread %d requests a sys_tag_get() with key=%d, command=%d and permission=%d as parameters\n", MODNAME, current->pid, key, command, permission);

        return 0;
}

/* Syscall pointers */
static unsigned long sys_tag_get = (unsigned long) __x64_sys_tag_get;
static unsigned long sys_tag_send = (unsigned long) __x64_sys_tag_send;
static unsigned long sys_tag_receive = (unsigned long) __x64_sys_tag_receive;
static unsigned long sys_tag_ctl = (unsigned long) __x64_sys_tag_ctl;

unsigned long cr0;

static inline void
write_cr0_forced(unsigned long val)
{
    unsigned long __force_order;

    /* __asm__ __volatile__( */
    asm volatile(
        "mov %0, %%cr0"
        : "+r"(val), "+m"(__force_order));
}

static inline void
protect_memory(void)
{
    write_cr0_forced(cr0);
}

static inline void
unprotect_memory(void)
{
    write_cr0_forced(cr0 & ~X86_CR0_WP);
}

static int __init install(void) {
   int i,j;

   syscall_table_finder();

   if(!hacked_syscall_tbl){
      printk("%s: failed to find the sys_call_table\n",MODNAME);
      return -1;
   }

   j=0;
   /* Registering indexes for the new syscalls */
   for (i=0; i<NR_ENTRIES; i++) {
      if (hacked_syscall_tbl[i] == hacked_ni_syscall) {
         printk("%s: found sys_ni_syscall entry at syscall_table[%d]\n",MODNAME,i);

         free_entries[j++] = i;
         if(j>=NR_NEW_SYSCALLS) break;
      }
   }

   /* Writing the new syscall on the syscall table */
   cr0 = read_cr0();
   unprotect_memory();
   hacked_syscall_tbl[FIRST_NI_SYSCALL] = (unsigned long*)sys_tag_get;
   protect_memory();
   printk("%s: a sys_call with 3 parameters has been installed on the sys_call_table at displacement %d\n", MODNAME, FIRST_NI_SYSCALL);

   printk("%s: module correctly mounted\n",MODNAME);

   return 0;
}

static void __exit uninstall(void) {

   cr0 = read_cr0();
   unprotect_memory();
   hacked_syscall_tbl[FIRST_NI_SYSCALL] = (unsigned long*)hacked_ni_syscall;
   hacked_syscall_tbl[SECOND_NI_SYSCALL] = (unsigned long*)hacked_ni_syscall;
   hacked_syscall_tbl[THIRD_NI_SYSCALL] = (unsigned long*)hacked_ni_syscall;
   hacked_syscall_tbl[FOURTH_NI_SYSCALL] = (unsigned long*)hacked_ni_syscall;
   protect_memory();

   printk("%s: shutting down\n",MODNAME);
}

module_init(install);
module_exit(uninstall);
