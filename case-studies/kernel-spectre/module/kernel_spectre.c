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

#include "kernel_spectre.h"

MODULE_AUTHOR("IAIK");
MODULE_DESCRIPTION("Device to let kernel access addresses");
MODULE_LICENSE("GPL");

#define BUFFER_SIZE (4096*256)
#define DIRECT_ACCESS 0 // Disable for Spectre

static __attribute__((aligned(4096))) char buffer[BUFFER_SIZE];
static char* identity_mapping = 0;

static const char* secret_data = KERNEL_SPECTRE_SECRET_DATA;

static int device_open(struct inode *inode, struct file *file) {
  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  return 0;
}

static size_t len = 3;
unsigned char throttle[8 * 4096];

static long access_address(unsigned long offset) {
#if DIRECT_ACCESS == 1
  *(volatile size_t*)(buffer + secret_data[offset] * 4096);
#else
  if (offset / throttle[0] < (len / throttle[4096 * 2])) {
    *(volatile size_t*)(buffer + secret_data[offset] * 4096);
  }
#endif

  return 0;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num,
                         unsigned long ioctl_param) {
  switch (ioctl_num) {
    case KERNEL_SPECTRE_IOCTL_CMD_GET_MAPPING:
      {
        identity_mapping = phys_to_virt(0);
        if (copy_to_user((void __user *) ioctl_param, &identity_mapping,
              sizeof(size_t))) {
            return -EFAULT;
        }
        return 0;
      }
    case KERNEL_SPECTRE_IOCTL_CMD_ACCESS:
      {
        access_address(ioctl_param);
        return 0;
      }
    case KERNEL_SPECTRE_IOCTL_CMD_GET_ADDRESS:
      {
        unsigned long address = (unsigned long) buffer;
        if (copy_to_user((void __user *) ioctl_param, &address, sizeof(size_t))) {
            return -EFAULT;
        }
        return 0;
      }
      break;
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
    .name = KERNEL_SPECTRE_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

int init_module(void) {
  size_t r = 0;

  /* Setup */
  memset(buffer, 0, BUFFER_SIZE);
  for (r = 0; r < 8; r++) {
    throttle[r * 4096] = 1;
  }

  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[kernel_spectre] Failed registering device with %zu\n", r);
    return 1;
  }

  printk(KERN_INFO "[kernel_spectre] Loaded.\n");

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);

  printk(KERN_INFO "[kernel_spectre] Removed.\n");
}
