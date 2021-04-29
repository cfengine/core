#include <stdio.h>

static inline void eatline(FILE *stream)
{
    int c;
    while ((c = getc(stream)) != EOF && c != '\n')
        ;
}

int main()
{
    printf("binary 0.0.1 v1 line_based\n");
    printf("\n");

    eatline(stdin);

    printf("operation=validate_promise\n");
    printf("promiser=foobar\n");
    printf("result=valid\n");
    printf("\n");

    eatline(stdin);

    printf("operation=evaluate_promise\n");
    printf("promiser=foobar\n");
    printf("result=kept\n");
    printf("\n");
    return 0;
}
