#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2)
        return -1;
    char *text = argv[1];
    int output = STDIN_FILENO;
    int result = 0;
    result = write(output, text, strlen(text));
    if (result < 0)
        return -1;
    fsync(output);
    return 0;
}
