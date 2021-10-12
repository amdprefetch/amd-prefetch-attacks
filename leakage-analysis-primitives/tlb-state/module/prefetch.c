/* See LICENSE file for license and copyright information */

#include <asm/tlbflush.h>
#include <asm/uaccess.h>
#include <drm/drm_cache.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/memory.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "prefetch.h"

MODULE_AUTHOR("IAIK");
MODULE_DESCRIPTION("Device to let kernel access addresses");
MODULE_LICENSE("GPL");

#define BUFFER_SIZE (4096*256)

inline __attribute__((always_inline)) void myprefetch(size_t p) {
  asm volatile ("prefetcht0 (%0)" : : "r" (p));
}

static int device_open(struct inode *inode, struct file *file) {
  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  return 0;
}

static long access_address(unsigned long virtual_address) {
  size_t i = 0;
  asm volatile("stac");

  for (i = 0; i < 10000; i++) {
    myprefetch(virtual_address);
  }

  asm volatile("clac");
  return 0;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num,
                         unsigned long ioctl_param) {
  switch (ioctl_num) {
    case PREFETCH_PROFILE_IOCTL_CMD_ACCESS_ADDRESS:
      {
        access_address(ioctl_param);
        return 0;
      }
    default:
      return -1;
  }

  return 0;
}

static struct file_operations f_ops = {
  .unlocked_ioctl = device_ioctl,
  .open = device_open,
  .release = device_release
};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = PREFETCH_PROFILE_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

int init_module(void) {
  size_t r = 0;

  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[prefetch] Failed registering device with %zu\n", r);
    return 1;
  }

  printk(KERN_INFO "[prefetch] Loaded.\n");

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);

  printk(KERN_INFO "[prefetch] Removed.\n");
}
