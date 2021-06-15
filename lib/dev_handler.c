#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "../include/constants.h"
#include "../include/TST_handler.h"
#include "../include/dev_handler.h"

extern int get_tag_status(int);
extern tag_t *get_tag_service(int);

static int Major;
char *final_status;

static DEFINE_MUTEX(device_state);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)      MAJOR(session->f_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)      MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_dentry->d_inode->i_rdev)
#endif

char *status_builder(void) {
   int i, j;
   int len = 0;
   tag_t *tag_obj;
   level_t *level_obj;

   char *service_status = (char *) vmalloc(sizeof(char) * 8192);

   if (!service_status) {
      printk("%s: error while allocating memory with vmalloc.\n", MODNAME);
      return NULL;
   }

   spin_lock(&tst_spinlock);

   strcpy(service_status, "");

   for (i=0; i<TAG_SERVICES_NUM; i++) {
      if (get_tag_status(i) == 1) {
         tag_obj = get_tag_service(i+1);

         if (tag_obj != NULL) {
            char tag[4];
            char creator[32];
            int key = i+1;

            /* Tag service creator */
            sprintf(creator, "%d", tag_obj->owner);
            /* Tag service identifier */
            sprintf(tag, "%d", key);

            for (j=0; j<LEVELS; j++) {
               if (tag_obj->levels[j].active == 1) {
                  char level[4];
                  char waiting_threads[3];
                  level_obj = &(tag_obj->levels[j]);

                  /* Level considered */
                  sprintf(level, "%d", j);
                  /* Waiting threads on the considered tag service level */
                  sprintf(waiting_threads, "%d", level_obj->waiting_threads);

                  strcat(service_status, tag);
                  len += sizeof(tag);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, creator);
                  len += sizeof(creator);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, level);
                  len += sizeof(level);
                  strcat(service_status, ", ");
                  len += sizeof(", ");
                  strcat(service_status, waiting_threads);
                  len += sizeof(waiting_threads);
                  strcat(service_status, "\n");
                  len += sizeof("\n");
               }
            }
         }
      }
   }

   service_status[len] = '\0';
   len = strlen(service_status);
   spin_unlock(&tst_spinlock);

   if (len == 0) {
      printk("%s: no active service\n", MODNAME);
      return "";
   }

   final_status = vmalloc(sizeof(char) * len);

   if (!final_status) {
      printk("%s: error while allocating memory with vmalloc\n", MODNAME);
      vfree(service_status);
      return NULL;
   }

   strncpy(final_status, service_status, len);
   final_status[len] = '\0';
   vfree(service_status);

   return final_status;
}

void free_status_buffer(void){
        vfree(final_status);
        return;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
   int ret;
   char status[8192];
   int valid_bytes;

   len = 8192;
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n", MODNAME, get_major(filp), get_minor(filp));

   /* The service status is gathered */
   strcpy(status, status_builder());
   free_status_buffer();

   valid_bytes = strlen(status);

   /* The service status is copied to user level buffer */
   ret = copy_to_user(buff, status, len);

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

static struct file_operations fops = {
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release
};

int register_dev(void) {

   Major = __register_chrdev(0, 0, 256, DEVNAME, &fops);

   if (Major < 0) {
      printk("%s: registering device failed\n", MODNAME);
      return -1;
   }

   printk("%s: new device registered, it is assigned major number %d\n", MODNAME, Major);

   return 0;
}

void unregister_dev(void) {

   unregister_chrdev(Major, DEVNAME);

   return;
}
