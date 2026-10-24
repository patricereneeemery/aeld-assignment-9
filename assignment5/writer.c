#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *text = argv[2];

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    fprintf(fp, "%s\n", text);
    fclose(fp);

    return 0;
}
