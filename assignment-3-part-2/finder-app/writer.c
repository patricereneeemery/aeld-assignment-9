/**
 * file writer.c
 * Patrice R Emery Writes a string to a file.
 *
 * TODO: Implement the writer utility for assignment 2 and 3.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments: %d", argc - 1);
        fprintf(stderr, "Usage: %s <writefile> <writestr>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];

    FILE *fp = fopen(writefile, "w");
    if (!fp) {
        syslog(LOG_ERR, "Failed to open file: %s", writefile);
        perror("Error");
        closelog();
        return 1;
    }

    fprintf(fp, "%s", writestr);
    syslog(LOG_DEBUG, "Writing \"%s\" to \"%s\"", writestr, writefile);

    fclose(fp);
    closelog();
    return 0;
}
