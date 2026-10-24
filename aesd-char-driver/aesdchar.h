#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include <linux/cdev.h>
#include <linux/mutex.h>
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

/* ---- Function prototypes matching your main.c ---- */

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);

ssize_t aesd_write(struct file *filp, const char __user *buf,
                   size_t count, loff_t *f_pos);

ssize_t aesd_read(struct file *filp, char __user *buf,
                  size_t count, loff_t *f_pos);

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence);

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

int aesd_init_module(void);
void aesd_cleanup_module(void);

/* ---- Debug macros ---- */

#define AESD_DEBUG 1

#undef PDEBUG
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
#    define PDEBUG(fmt, args...) printk(KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)
#endif

/* ---- Device structure ---- */

struct aesd_dev {
    struct cdev cdev;
    struct aesd_circular_buffer buffer;
    struct mutex lock;

    char *write_buf;
    size_t write_buf_size;
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */

