#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "aesd_ioctl.h"

#define PORT 9000
#define BUF_SIZE 1024

int main()
{
    int server_fd, client_fd, dev_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];
    ssize_t bytes_read;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        dev_fd = open("/dev/aesdchar", O_RDWR);
        if (dev_fd < 0) {
            perror("open /dev/aesdchar");
            close(client_fd);
            continue;
        }

        memset(buffer, 0, BUF_SIZE);
        ssize_t total_len = 0;

        // Read full client message until connection closes
        while ((bytes_read = recv(client_fd, buffer + total_len,
                                  BUF_SIZE - total_len, 0)) > 0)
        {
            total_len += bytes_read;
        }

        // Check for IOCTL command
        char *cmd = strstr(buffer, "AESDCHAR_IOCSEEKTO:");
        if (cmd) {
            struct aesd_seekto seekto;

            // Parse "AESDCHAR_IOCSEEKTO:X,Y"
            if (sscanf(cmd, "AESDCHAR_IOCSEEKTO:%u,%u",
                       &seekto.write_cmd, &seekto.write_cmd_offset) == 2)
            {
                // Perform the ioctl seek
                if (ioctl(dev_fd, AESDCHAR_IOCSEEKTO, &seekto) != 0) {
                    perror("ioctl");
                }
            }

            // Remove IOCTL command from write buffer
            *cmd = '\0';
        }

        // Write remaining data (excluding IOCTL command)
        if (strlen(buffer) > 0) {
            write(dev_fd, buffer, strlen(buffer));
        }

        // Read from device and send back to client
        memset(buffer, 0, BUF_SIZE);
        while ((bytes_read = read(dev_fd, buffer, BUF_SIZE)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
            memset(buffer, 0, BUF_SIZE);
        }

        close(dev_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
