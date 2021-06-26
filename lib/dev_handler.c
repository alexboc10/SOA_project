#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "../include/constants.h"
#include "../include/TST_handler.h"
#include "../include/dev_handler.h"

extern char *status_builder(void);

static int Major;

static DEFINE_MUTEX(device_state);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session) MAJOR(session->f_inode->i_rdev)
#define get_minor(session) MINOR(session->f_inode->i_rdev)
#else
#define get_major(session) MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session) MINOR(session->f_dentry->d_inode->i_rdev)
#endif

/* Reading operation */
static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
   int ret;
   char status[8192];
   int valid_bytes;

   char *res_status = (char *) vmalloc(sizeof(char)*8192);

   len = 8192;
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

   /* The service status is gathered */
   res_status = status_builder();
   strcpy(status, res_status);

   valid_bytes = strlen(status);

   /* The service status is copied to user level buffer */
   //ret = copy_to_user(buff, status, len);

   if (*off > valid_bytes) {
      return 0;
   }

   if ((valid_bytes - *off) < len) {
      len = valid_bytes - *off;
   }

   ret = copy_to_user(buff,&(status[*off]),len);

   *off += (len - ret);

   return len - ret;
}

static int dev_open(struct inode *inode, struct file *file) {

   // this device file is single instance
   if (!mutex_trylock(&device_state)) {
      return -1;
   }

   printk("%s: device file successfully opened by thread %d\n", MODNAME, current->pid);

   return 0;
}


static int dev_release(struct inode *inode, struct file *file) {

   mutex_unlock(&device_state);

   printk("%s: device file closed by thread %d\n", MODNAME, current->pid);

   return 0;

}

/* Char device driver */
static struct file_operations fops = {
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release
};

int register_dev(void) {

   /* The major choice is specified */
   Major = __register_chrdev(MAJOR_NM, 0, 256, DEVNAME, &fops);

   if (Major < 0) {
      printk("%s: registering device failed\n", MODNAME);
      return -1;
   }

   if (MAJOR_NM != 0) {
      printk("%s: new device registered, it is assigned major number %d\n", MODNAME, Major);
   }

   return 0;
}

void unregister_dev(void) {

   unregister_chrdev(MAJOR_NM, DEVNAME);

   return;
}
