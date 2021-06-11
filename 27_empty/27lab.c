#include <ctype.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

bool isLineEmpty(const char* str)
{
    // return true if (NULL) or (\0) or (\n\0) or (\r\n\0)
    return (str == NULL) || (str[0] == '\0') || (str[0] == '\n' && str[1] == '\0') || (str[0] == '\r' && str[1] == '\n' && str[2] == '\0');
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE* in;
    FILE* pipe[2];

    if (NULL == (in = fopen(argv[1], "r")))
    {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    if (0 != p2open("wc -l", pipe))
    {
        fprintf(stderr, "p2open() failed\n");
        return EXIT_FAILURE;
    }

    char* buffer = (char*)calloc(BUFFER_SIZE, sizeof(*buffer));
    if (!buffer)
    {
        perror("malloc()");
        goto error;
    }

    while (NULL != fgets(buffer, BUFFER_SIZE, in))
    {
        if (isLineEmpty(buffer))
        {
            if (EOF == fputs(buffer, pipe[0]))
            {
                perror("fputs()");
                goto error;
            }
        }
    }

    if (-1 == pclose(pipe[0]))
    {
        perror("pclose()");
        goto error;
    }

    if (NULL == fgets(buffer, BUFFER_SIZE, pipe[1]))
    {
        perror("fgets()");
        goto error;
    }

    int emptyLinesCount;
    if (1 != sscanf(buffer, "%d", &emptyLinesCount))
    {
        perror("sscanf()");
        goto error;
    }

    printf("number of empty lines: %d\n", emptyLinesCount);
    free(buffer);
    fclose(in);
    p2close(pipe);
    return EXIT_SUCCESS;

error:
    free(buffer);
    fclose(in);
    p2close(pipe);
    return EXIT_FAILURE;
}
