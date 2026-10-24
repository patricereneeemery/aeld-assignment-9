#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct aesd_seekto {
    uint32_t write_cmd;
    uint32_t write_cmd_offset;
};

#define AESDCHAR_IOC_MAGIC 'Z'
#define AESDCHAR_IOCSEEKTO _IOW(AESDCHAR_IOC_MAGIC, 1, struct aesd_seekto)

int main(void)
{
    int fd;
    ssize_t rc;
    char buf[256];
    struct aesd_seekto seek;

    fd = open("/dev/aesdchar", O_RDWR);
    if (fd < 0) {
        perror("open /dev/aesdchar");
        return EXIT_FAILURE;
    }

    rc = write(fd, "first line\n", strlen("first line\n"));
    if (rc < 0) { perror("write first"); goto out; }

    rc = write(fd, "second line\n", strlen("second line\n"));
    if (rc < 0) { perror("write second"); goto out; }

    rc = write(fd, "third line\n", strlen("third line\n"));
    if (rc < 0) { perror("write third"); goto out; }

    seek.write_cmd = 1;       /* second command (0-based) */
    seek.write_cmd_offset = 3;/* offset into "second line\n" */

    if (ioctl(fd, AESDCHAR_IOCSEEKTO, &seek) < 0) {
        perror("ioctl AESDCHAR_IOCSEEKTO");
        goto out;
    }

    rc = read(fd, buf, sizeof(buf) - 1);
    if (rc < 0) {
        perror("read after ioctl");
        goto out;
    }

    buf[rc] = '\0';
    printf("Read after seek: \"%s\"\n", buf);

out:
    close(fd);
    return 0;
}
