#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT "9000"
#define DATAFILE "/var/tmp/aesdsocketdata"

static int server_fd = -1;
static int client_fd = -1;
static volatile sig_atomic_t exit_requested = 0;

/*************** SIGNAL HANDLING ***************/
void handle_signal(int signo)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_requested = 1;
}

void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }
}

/*************** DAEMON MODE ***************/
void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);
}

/*************** SOCKET SETUP ***************/
int create_server_socket(void)
{
    struct addrinfo hints, *res, *p;
    int sockfd, rv;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(rv));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (!p) {
        syslog(LOG_ERR, "Failed to bind socket");
        return -1;
    }

    if (listen(sockfd, 10) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/*************** CLIENT HANDLING ***************/
void handle_client(int client_fd, struct sockaddr *addr)
{
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;

    inet_ntop(AF_INET, &(addr_in->sin_addr), client_ip, sizeof(client_ip));
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    int data_fd = open(DATAFILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (data_fd == -1) {
        syslog(LOG_ERR, "open data file failed");
        return;
    }

    size_t buf_size = 1024;
    char *buf = malloc(buf_size);
    if (!buf) {
        syslog(LOG_ERR, "malloc failed");
        close(data_fd);
        return;
    }

    size_t used = 0;
    ssize_t n;

    while (!exit_requested) {

        // Expand buffer if needed
        if (used == buf_size) {
            buf_size *= 2;
            char *tmp = realloc(buf, buf_size);
            if (!tmp) {
                syslog(LOG_ERR, "realloc failed");
                free(buf);
                close(data_fd);
                return;
            }
            buf = tmp;
        }

        n = recv(client_fd, buf + used, buf_size - used, 0);
        if (n < 0) {
            syslog(LOG_ERR, "recv failed");
            break;
        }
        if (n == 0) break;

        used += n;

        // TODO: detect newline packet completion
        char *newline = memchr(buf, '\n', used);
        if (newline) {
            size_t packet_len = (newline - buf) + 1;

            // TODO: append packet to file
            write(data_fd, buf, packet_len);

            // shift remaining data
            size_t remaining = used - packet_len;
            memmove(buf, buf + packet_len, remaining);
            used = remaining;

            close(data_fd);
            data_fd = open(DATAFILE, O_RDONLY);
            if (data_fd == -1) {
                syslog(LOG_ERR, "open for read failed");
                free(buf);
                return;
            }

            // TODO: send full file back to client
            char filebuf[1024];
            ssize_t r;
            while ((r = read(data_fd, filebuf, sizeof(filebuf))) > 0) {
                send(client_fd, filebuf, r, 0);
            }

            break;
        }
    }

    free(buf);
    close(data_fd);

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

/*************** MAIN ***************/
int main(int argc, char *argv[])
{
    int daemon_mode = 0;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);
    setup_signals();

    server_fd = create_server_socket();
    if (server_fd == -1) {
        closelog();
        return EXIT_FAILURE;
    }

    if (daemon_mode) daemonize();

    while (!exit_requested) {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (exit_requested) break;
            syslog(LOG_ERR, "accept failed");
            continue;
        }

        handle_client(client_fd, (struct sockaddr *)&client_addr);
        close(client_fd);
        client_fd = -1;
    }

    if (client_fd != -1) close(client_fd);
    if (server_fd != -1) close(server_fd);

    remove(DATAFILE);

    closelog();
    return 0;
}
