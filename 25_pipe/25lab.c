#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main()
{
    int fds[2];
    if (0 != pipe(fds))
    {
        perror("pipe()");
        return EXIT_FAILURE;
    }
    pid_t pid;
    if (-1 == (pid = fork()))
    {
        perror("fork()");
        close(fds[0]);
        close(fds[1]);
        return EXIT_FAILURE;
    }
    else if (pid == 0)
    {
        if (close(fds[1]) != 0)
        {
            perror("close(fds[1])");
        }

        char* buf = (char*)calloc(BUFFER_SIZE + 1, sizeof(*buf));
        if (!buf)
        {
            close(fds[0]);
            return EXIT_FAILURE;
        }
        ssize_t r = read(fds[0], buf, BUFFER_SIZE);
        for (ssize_t i = 0; i < r; i++)
        {
            char c = (char)toupper(buf[i]);
            write(STDOUT_FILENO, &c, 1);
        }

        free(buf);
        if (close(fds[0]) != 0)
        {
            perror("close(fds[0])");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    else
    {
        if (close(fds[0]) != 0)
        {
            perror("close(fds[0])");
        }

        const char buf[] = "SoMe TeXt\n";
        if (-1 == write(fds[1], buf, sizeof(buf)))
        {
            perror("write()");
            close(fds[1]);
            return EXIT_FAILURE;
        }

        int chldStatus;
        if (pid != wait(&chldStatus))
        {
            perror("wait()");
            close(fds[1]);
            return EXIT_FAILURE;
        }

        if (close(fds[1]) != 0)
        {
            perror("close(fds[1])");
            return EXIT_FAILURE;
        }
        return WEXITSTATUS(chldStatus);
    }
}
