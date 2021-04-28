#include <stdio.h>

int main()
{
    char *line = NULL;
    size_t len = 0;

    printf("binary 0.0.1 v1 line_based\n");
    printf("\n");

    if (getline(&line, &len, stdin) == -1)
    {
        perror("Couldn't read from stdin");
        return 1;
    }

    printf("operation=validate_promise\n");
    printf("promiser=foobar\n");
    printf("result=valid\n");
    printf("\n");

    if (getline(&line, &len, stdin) == -1)
    {
        perror("Couldn't read from stdin");
        return 1;
    }

    printf("operation=evaluate_promise\n");
    printf("promiser=foobar\n");
    printf("result=kept\n");
    printf("\n");
    return 0;
}
