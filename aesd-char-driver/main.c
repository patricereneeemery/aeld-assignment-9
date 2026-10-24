/**
 * @file main.c
 * @brief AESD character driver implementation (scull-style)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "aesdchar.h"
#include "aesd_ioctl.h"
#include "aesd-circular-buffer.h"

int aesd_major = 0;
int aesd_minor = 0;

MODULE_AUTHOR("Patrice Renee Emery");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &aesd_device;
    PDEBUG("open");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf,
                  size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    size_t to_copy;
    ssize_t retval = 0;
    size_t total_bytes = 0;
    uint8_t idx;
    uint8_t count_entries;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    printk(KERN_INFO "READ: f_pos=%lld\n", *f_pos);

    /* Compute total bytes stored in circular buffer */
    idx = dev->buffer.out_offs;
    count_entries = dev->buffer.full ?
        AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED :
        dev->buffer.in_offs;

    for (uint8_t i = 0; i < count_entries; i++) {
        total_bytes += dev->buffer.entry[idx].size;
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    /* EOF */
    if (*f_pos >= total_bytes)
        goto out;

    /* Find entry + offset */
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(
        &dev->buffer,
        *f_pos,
        &entry_offset);

    if (!entry)
        goto out;

    to_copy = min(count, entry->size - entry_offset);

    if (copy_to_user(buf, entry->buffptr + entry_offset, to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += to_copy;
    retval = to_copy;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf,
                   size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = count;
    char *kbuf;
    size_t i, start = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        retval = -ENOMEM;
        goto out;
    }

    if (copy_from_user(kbuf, buf, count)) {
        retval = -EFAULT;
        goto free_kbuf;
    }

    /* Append new data to partial write buffer */
    dev->write_buf = krealloc(dev->write_buf,
                              dev->write_buf_size + count,
                              GFP_KERNEL);
    if (!dev->write_buf) {
        retval = -ENOMEM;
        goto free_kbuf;
    }

    memcpy(dev->write_buf + dev->write_buf_size, kbuf, count);
    dev->write_buf_size += count;

    /* Process all complete commands ending in '\n' */
    for (i = 0; i < dev->write_buf_size; i++) {
        if (dev->write_buf[i] == '\n') {
            size_t cmd_size = i + 1 - start;
            char *cmd = kmalloc(cmd_size, GFP_KERNEL);

            if (!cmd) {
                retval = -ENOMEM;
                goto free_kbuf;
            }

            memcpy(cmd, dev->write_buf + start, cmd_size);

            struct aesd_buffer_entry entry = {
                .buffptr = cmd,
                .size = cmd_size,
            };

            aesd_circular_buffer_add_entry(&dev->buffer, &entry);

            start = i + 1;
        }
    }

    /* Preserve leftover partial command */
    if (start < dev->write_buf_size) {
        size_t leftover = dev->write_buf_size - start;
        memmove(dev->write_buf, dev->write_buf + start, leftover);
        dev->write_buf_size = leftover;

        if (leftover == 0) {
            kfree(dev->write_buf);
            dev->write_buf = NULL;
        } else {
            dev->write_buf = krealloc(dev->write_buf, leftover, GFP_KERNEL);
            if (!dev->write_buf) {
                dev->write_buf_size = 0;
                retval = -ENOMEM;
                goto free_kbuf;
            }
        }
    } else {
        kfree(dev->write_buf);
        dev->write_buf = NULL;
        dev->write_buf_size = 0;
    }

free_kbuf:
    kfree(kbuf);
out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t new_pos = 0;
    size_t total_bytes = 0;
    uint8_t idx;
    uint8_t count_entries;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    /* Compute total bytes stored in circular buffer */
    idx = dev->buffer.out_offs;
    count_entries = dev->buffer.full ?
        AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED :
        dev->buffer.in_offs;

    for (uint8_t i = 0; i < count_entries; i++) {
        total_bytes += dev->buffer.entry[idx].size;
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;

    case SEEK_CUR:
        new_pos = filp->f_pos + offset;
        break;

    case SEEK_END:
        new_pos = total_bytes + offset;
        break;

    default:
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_bytes) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;

    mutex_unlock(&dev->lock);
    return new_pos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    size_t new_pos = 0;
    uint8_t idx;
    uint8_t count;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;

    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    printk(KERN_INFO "IOCTL: write_cmd=%u offset=%u\n",
           seekto.write_cmd, seekto.write_cmd_offset);

    count = dev->buffer.full ?
        AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED :
        dev->buffer.in_offs;

    if (seekto.write_cmd >= count) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    idx = dev->buffer.out_offs;

    for (uint32_t i = 0; i < seekto.write_cmd; i++)
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (seekto.write_cmd_offset >= dev->buffer.entry[idx].size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    /* Compute new_pos EXACTLY like aesd_read */
    {
        uint8_t cur = dev->buffer.out_offs;
        for (uint32_t i = 0; i < seekto.write_cmd; i++) {
            new_pos += dev->buffer.entry[cur].size;
            cur = (cur + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        }
    }

    new_pos += seekto.write_cmd_offset;
    filp->f_pos = new_pos;

    printk(KERN_INFO "IOCTL: new_pos=%zu\n", new_pos);

    mutex_unlock(&dev->lock);
    return 0;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_ERR "Error %d adding aesd cdev", err);

    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);

    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device, 0, sizeof(struct aesd_dev));
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
        unregister_chrdev_region(dev, 1);

    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    uint8_t i;

    cdev_del(&aesd_device.cdev);

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (aesd_device.buffer.entry[i].buffptr)
            kfree(aesd_device.buffer.entry[i].buffptr);
    }

    if (aesd_device.write_buf)
        kfree(aesd_device.write_buf);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
