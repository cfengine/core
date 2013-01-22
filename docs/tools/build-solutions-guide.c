/*****************************************************************************/
/*                                                                           */
/* File: build_solutions_guide.c                                             */
/*                                                                           */
/* Created: Fri Aug 19 11:06:29 2011                                         */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static void Manual(const char *examples_dir);

/*****************************************************************************/

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: build_solutions_guide <examples-dir> < in > out\n");
        return 2;
    }

    Manual(argv[1]);
    return 0;
}

/*****************************************************************************/

/*
 * Strips out \n from the end of line
 */
static void Chomp(char *str)
{
    char *s = strrchr(str, '\n');

    if (s)
    {
        *s = '\0';
    }
}

/*****************************************************************************/

#define EXAMPLE_INCLUDE_TOKEN "#CFEexample:"
#define EXAMPLE_HEADER_TOKEN "COSL.txt"

static void IncludeExampleFile(const char *examples_dir, const char *filename)
{
    char path[2048];

    snprintf(path, 2048, "%s/%s", examples_dir, filename);

    FILE *example_fh = fopen(path, "r");

    if (example_fh == NULL)
    {
        fprintf(stderr, "Unable to find file %s to include.\n", path);
        exit(1);
    }

/* Skip header */
    while (!feof(example_fh))
    {
        char line[2048];

        if (fgets(line, 2047, example_fh) == NULL)
        {
            fprintf(stderr, "Unable to read line from file '%s' - aborting", path);
            fclose(example_fh);
            return;
        }
        if (strstr(line, "COSL.txt"))
        {
            break;
        }
    }

    while (!feof(example_fh))
    {
        char buf[4096];
        size_t read_ = fread(buf, 1, 4096, example_fh);

        if (read_ < 4096 && ferror(example_fh))
        {
            fprintf(stderr, "Error reading example file %s. Bailing out.\n", path);
            exit(1);
        }

        if (fwrite(buf, 1, read_, stdout) < read_)
        {
            fprintf(stderr, "Error writing to stdout. Bailing out.\n");
            exit(1);
        }
    }

    fclose(example_fh);
}

static void Manual(const char *examples_dir)
{
    for (;;)
    {
        char line[2048];

        if (fgets(line, 2048, stdin) == NULL)
        {
            if (ferror(stdin))
            {
                fprintf(stderr, "Error during reading stdin. Bailing out.\n");
                exit(1);
            }
            else
            {
                return;
            }
        }

        if (strstr(line, EXAMPLE_INCLUDE_TOKEN))
        {
            char *filename = strstr(line, EXAMPLE_INCLUDE_TOKEN) + strlen(EXAMPLE_INCLUDE_TOKEN);

            Chomp(filename);

            IncludeExampleFile(examples_dir, filename);
        }
        else
        {
            fputs(line, stdout);
        }
    }
}
