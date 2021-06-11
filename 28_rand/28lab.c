#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define RAND_NUMS_COUNT 100

int waitForChild(pid_t childPid)
{
    void (*istat)(int);
    void (*qstat)(int);
    void (*hstat)(int);
    istat = signal(SIGINT, SIG_IGN);
    qstat = signal(SIGQUIT, SIG_IGN);
    hstat = signal(SIGHUP, SIG_IGN);
    int status;
    int r;
    while ((r = waitpid(childPid, &status, 0)) == (pid_t)-1 && errno == EINTR)
        ;
    if (r == (pid_t)-1)
        status = -1;
    (void)signal(SIGINT, istat);
    (void)signal(SIGQUIT, qstat);
    (void)signal(SIGHUP, hstat);
    return (status);
}

int main()
{
    srand((unsigned)time(NULL));

    FILE* sortPipe[2];
    if (0 != p2open("sort -n", sortPipe))
    {
        fprintf(stderr, "p2open(\"sort\"): %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    pid_t forkedPid = fork();
    if (forkedPid == -1)
    {
        perror("fork()");
        return EXIT_FAILURE;
    }
    else if (forkedPid == 0)
    {
        for (size_t i = 0; i < RAND_NUMS_COUNT; i++)
        {
            int randNum = rand() % 100;
            fprintf(sortPipe[0], "%d\n", randNum);
        }
        return EXIT_SUCCESS;
    }
    else
    {
        if (-1 == waitForChild(forkedPid))
        {
            fprintf(stderr, "wait(): %s\n", strerror(errno));
            p2close(sortPipe);
            return EXIT_FAILURE;
        }

        pclose(sortPipe[0]);

        int i = 0;
        char buf[1024];
        while (fgets(buf, sizeof(buf), sortPipe[1]))
        {
            buf[strcspn(buf, "\n")] = '\0';
            i++;
            printf("%s ", buf);
            if (i % 10 == 0)
            {
                printf("\n");
            }
        }
        printf("\n");

        p2close(sortPipe);
        return 0;
    }
}
