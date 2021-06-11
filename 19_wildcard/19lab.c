#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void printMatches(const char* pattern, const char* startDir)
{
    DIR* d;
    struct dirent* dirEntry;
    bool hasMatches = false;
    if ((d = opendir(startDir)))
    {
        do
        {
            errno = 0;
            if ((dirEntry = readdir(d)))
            {
                struct stat fileStat;
                if (0 != lstat(dirEntry->d_name, &fileStat))
                {
                    fprintf(stderr, "%s: %s\n", dirEntry->d_name, strerror(errno));
                    continue;
                }
                if (S_ISREG(fileStat.st_mode) && 0 == fnmatch(pattern, dirEntry->d_name, 0))
                {
                    printf("%s\n", dirEntry->d_name);
                    hasMatches = true;
                }
            }
        } while (dirEntry);

        if (errno != 0)
        {
            fprintf(stderr, "error reading %s: %s\n", startDir, strerror(errno));
        }
    }
    else 
    {
        fprintf(stderr, "%s: %s\n", startDir, strerror(errno));
    }

    if (!hasMatches)
    {
        printf("%s\n", pattern);
    }
    
    if (0 != closedir(d))
    {
        fprintf(stderr, "%s: %s\n", startDir, strerror(errno));
    }
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s pattern\n", argv[0]);
        return EXIT_FAILURE;
    }

    printMatches(argv[1], ".");

    return EXIT_SUCCESS;
}
