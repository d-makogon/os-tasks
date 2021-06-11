#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* const argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s program_name\n", argv[0]);
        return EXIT_FAILURE;
    }
    FILE* pipe = popen(argv[1], "w");
    if (!pipe)
    {
        perror("popen()");
        return EXIT_FAILURE;
    }
    const char buf[] = "SoMe TexT\n";
    if (EOF == fputs(buf, pipe))
    {
        perror("fputs()");
        pclose(pipe);
        return EXIT_FAILURE;
    }
    if (-1 == pclose(pipe))
    {
        perror("pclose()");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
