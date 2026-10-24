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
#include <pthread.h>
#include <time.h>

#define PORT "9000"
#define DATAFILE "/var/tmp/aesdsocketdata"

pthread_t timestamp_thread;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_info {
    pthread_t thread_id;
    int client_fd;
    int thread_complete;
    struct sockaddr_storage client_addr;
    struct thread_info *next;
};

struct thread_info *thread_list = NULL;

static int server_fd = -1;
static volatile sig_atomic_t exit_requested = 0;

/*************** SIGNAL HANDLING ***************/
void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        exit_requested = 1;
        shutdown(server_fd, SHUT_RDWR);   // unblock accept()
    }
}

void setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
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

    size_t buf_size = 1024;
    char *buf = malloc(buf_size);
    if (!buf) {
        syslog(LOG_ERR, "malloc failed");
        return;
    }

    size_t used = 0;
    ssize_t n;

    while (!exit_requested) {

        if (used == buf_size) {
            buf_size *= 2;
            char *tmp = realloc(buf, buf_size);
            if (!tmp) {
                syslog(LOG_ERR, "realloc failed");
                free(buf);
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

        /*************** THREAD-SAFE FILE WRITE ***************/
        pthread_mutex_lock(&file_mutex);

        int data_fd = open(DATAFILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (data_fd == -1) {
            syslog(LOG_ERR, "open data file failed");
            pthread_mutex_unlock(&file_mutex);
            free(buf);
            return;
        }

        // write all received data, no newline dependency
        if (write(data_fd, buf, used) == -1) {
            syslog(LOG_ERR, "write to data file failed");
            close(data_fd);
            pthread_mutex_unlock(&file_mutex);
            free(buf);
            return;
        }
        close(data_fd);

        /*************** THREAD-SAFE FILE READ ***************/
        data_fd = open(DATAFILE, O_RDONLY);
        if (data_fd == -1) {
            syslog(LOG_ERR, "open for read failed");
            pthread_mutex_unlock(&file_mutex);
            free(buf);
            return;
        }

        char filebuf[1024];
        ssize_t r;
        while ((r = read(data_fd, filebuf, sizeof(filebuf))) > 0) {
            if (send(client_fd, filebuf, r, 0) == -1) {
                syslog(LOG_ERR, "send failed");
                break;
            }
        }

        close(data_fd);
        pthread_mutex_unlock(&file_mutex);
        /******************************************************/

        break;
    }

    free(buf);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

/*************** THREAD LIST MANAGEMENT ***************/
void add_thread(struct thread_info *tinfo)
{
    tinfo->next = thread_list;
    thread_list = tinfo;
}

void cleanup_threads()
{
    struct thread_info *curr = thread_list;
    struct thread_info *prev = NULL;

    while (curr != NULL) {
        if (curr->thread_complete) {
            pthread_join(curr->thread_id, NULL);

            if (prev == NULL) {
                thread_list = curr->next;
            } else {
                prev->next = curr->next;
            }

            struct thread_info *tmp = curr;
            curr = curr->next;
            close(tmp->client_fd);
            free(tmp);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

/*************** TIMESTAMP THREAD ***************/
void *timestamp_thread_func(void *arg)
{
    while (!exit_requested) {
        sleep(10);

        pthread_mutex_lock(&file_mutex);

        FILE *fp = fopen(DATAFILE, "a");
        if (fp) {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);

            char time_str[128];
            strftime(time_str, sizeof(time_str),
                     "timestamp: %Y-%m-%d %H:%M:%S\n", tm_info);

            fputs(time_str, fp);
            fclose(fp);
        }

        pthread_mutex_unlock(&file_mutex);
    }

    return NULL;
}

/*************** CLIENT THREAD ***************/
void *client_thread_func(void *arg)
{
    struct thread_info *tinfo = (struct thread_info *)arg;

    handle_client(tinfo->client_fd, (struct sockaddr *)&tinfo->client_addr);

    tinfo->thread_complete = 1;

    return NULL;
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

    // mutex already statically initialized, this is optional but harmless
    pthread_mutex_init(&file_mutex, NULL);

    server_fd = create_server_socket();
    if (server_fd == -1) {
        closelog();
        return EXIT_FAILURE;
    }

    if (daemon_mode) daemonize();

    pthread_create(&timestamp_thread, NULL, timestamp_thread_func, NULL);

    while (!exit_requested) {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (exit_requested) break;
            syslog(LOG_ERR, "accept failed");
            continue;
        }

        struct thread_info *tinfo = malloc(sizeof(struct thread_info));
        if (!tinfo) {
            syslog(LOG_ERR, "malloc failed");
            close(client_fd);
            continue;
        }

        tinfo->client_fd = client_fd;
        tinfo->thread_complete = 0;
        memcpy(&tinfo->client_addr, &client_addr, sizeof(client_addr));
        tinfo->next = NULL;

        pthread_create(&tinfo->thread_id, NULL, client_thread_func, tinfo);

        add_thread(tinfo);
        cleanup_threads();
    }

    // stop accepting new clients (already done in signal handler, but harmless)
    shutdown(server_fd, SHUT_RDWR);

    // let timestamp thread exit via exit_requested, then join it
    pthread_join(timestamp_thread, NULL);

    // join any remaining client threads
    struct thread_info *curr = thread_list;
    while (curr) {
        pthread_join(curr->thread_id, NULL);
        close(curr->client_fd);

        struct thread_info *tmp = curr;
        curr = curr->next;
        free(tmp);
    }

    pthread_mutex_destroy(&file_mutex);

    if (server_fd != -1) close(server_fd);
    remove(DATAFILE);

    closelog();
    return 0;
}
