#ifndef AESD_IOCTL_H
#define AESD_IOCTL_H

#include <linux/ioctl.h>

struct aesd_seekto {
    unsigned int write_cmd;
    unsigned int write_cmd_offset;
};

#define AESDCHAR_IOCSEEKTO _IOW('a', 1, struct aesd_seekto)

#endif
