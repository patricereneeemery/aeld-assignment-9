#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "aesdchar.h"
#include "aesd_ioctl.h"
#include "aesd-circular-buffer.h"

MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev {
    struct aesd_circular_buffer buffer;
    struct mutex lock;
    char *leftover;
    size_t leftover_size;
};

static struct aesd_dev dev;

static int aesd_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int aesd_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t aesd_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    char *kbuf;

    if (count == 0)
        return 0;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    mutex_lock(&dev.lock);

    if (!dev.leftover) {
        dev.leftover = kmalloc(count, GFP_KERNEL);
        if (!dev.leftover) {
            mutex_unlock(&dev.lock);
            kfree(kbuf);
            return -ENOMEM;
        }
        memcpy(dev.leftover, kbuf, count);
        dev.leftover_size = count;
    } else {
        char *newbuf = krealloc(dev.leftover, dev.leftover_size + count, GFP_KERNEL);
        if (!newbuf) {
            mutex_unlock(&dev.lock);
            kfree(kbuf);
            return -ENOMEM;
        }
        dev.leftover = newbuf;
        memcpy(dev.leftover + dev.leftover_size, kbuf, count);
        dev.leftover_size += count;
    }

    if (memchr(dev.leftover, '\n', dev.leftover_size)) {
        struct aesd_buffer_entry entry;
        entry.buffptr = dev.leftover;
        entry.size = dev.leftover_size;

        aesd_circular_buffer_add_entry(&dev.buffer, &entry);

        dev.leftover = NULL;
        dev.leftover_size = 0;
    }

    retval = count;
    mutex_unlock(&dev.lock);
    kfree(kbuf);
    return retval;
}

static ssize_t aesd_read(struct file *filp, char __user *buf,
                         size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;

    mutex_lock(&dev.lock);

    printk(KERN_INFO "READ: f_pos=%lld\n", *f_pos);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev.buffer,
                                                            *f_pos,
                                                            &entry_offset);
    if (!entry) {
        retval = 0;
        goto out;
    }

    if (count > entry->size - entry_offset)
        count = entry->size - entry_offset;

    if (copy_to_user(buf, entry->buffptr + entry_offset, count)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev.lock);
    return retval;
}

static loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t new_pos = 0;
    loff_t total_size = 0;
    int i;

    mutex_lock(&dev.lock);

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (dev.buffer.entry[i].buffptr)
            total_size += dev.buffer.entry[i].size;
    }

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = filp->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = total_size + offset;
        break;
    default:
        mutex_unlock(&dev.lock);
        return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_size) {
        mutex_unlock(&dev.lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;
    mutex_unlock(&dev.lock);
    return new_pos;
}

static long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    size_t new_pos = 0;
    uint8_t idx;
    uint8_t i;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -EINVAL;

    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    printk(KERN_INFO "IOCTL: write_cmd=%u offset=%u\n",
           seekto.write_cmd, seekto.write_cmd_offset);

    mutex_lock(&dev.lock);

    if (seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        mutex_unlock(&dev.lock);
        return -EINVAL;
    }

    idx = (dev.buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (!dev.buffer.entry[idx].buffptr ||
        seekto.write_cmd_offset >= dev.buffer.entry[idx].size) {
        mutex_unlock(&dev.lock);
        return -EINVAL;
    }

    i = dev.buffer.out_offs;
    while (i != idx) {
        if (dev.buffer.entry[i].buffptr)
            new_pos += dev.buffer.entry[i].size;
        i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    new_pos += seekto.write_cmd_offset;

    filp->f_pos = new_pos;

    printk(KERN_INFO "IOCTL: new_pos=%zu\n", new_pos);

    mutex_unlock(&dev.lock);
    return 0;
}

static const struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
};

static int __init aesd_init(void)
{
    int result;

    mutex_init(&dev.lock);
    memset(&dev.buffer, 0, sizeof(dev.buffer));
    dev.leftover = NULL;
    dev.leftover_size = 0;

    result = register_chrdev(AESD_MAJOR, "aesdchar", &aesd_fops);
    if (result < 0)
        return result;

    return 0;
}

static void __exit aesd_exit(void)
{
    int i;

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (dev.buffer.entry[i].buffptr)
            kfree(dev.buffer.entry[i].buffptr);
    }

    if (dev.leftover)
        kfree(dev.leftover);

    unregister_chrdev(AESD_MAJOR, "aesdchar");
}

module_init(aesd_init);
module_exit(aesd_exit);
