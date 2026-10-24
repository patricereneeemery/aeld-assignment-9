long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -EINVAL;

    if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        return -EFAULT;

    printk(KERN_INFO "IOCTL: write_cmd=%u offset=%u\n",
           seekto.write_cmd, seekto.write_cmd_offset);

    // TODO: your existing logic to compute new f_pos goes here
    // After setting filp->f_pos, print:
    printk(KERN_INFO "IOCTL: new_pos=%lld\n", filp->f_pos);

    return 0;
}
