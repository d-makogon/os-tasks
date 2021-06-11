#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "list.h"

#define TIMEOUT_SEC 5
#define BUFFER_SIZE 1024

bool sigalarm = false;

void fwritef(int fd, const char* format, ...)
{
    va_list valist;
    va_start(valist, format);

    int length = vsnprintf(NULL, 0, format, valist);
    if (length < 0) return;
    ++length;
    char* res = (char*)malloc((size_t)length * sizeof(*res));
    if (!res) return;
    if (0 > vsnprintf(res, (size_t)length, format, valist))
    {
        free(res);
        return;
    }
    write(fd, res, (size_t)(length - 1));
    free(res);
}

void sigalarm_handler()
{
    sigalarm = true;
}

ssize_t readLine(int fd, char* buf, size_t len)
{
    if (sigalarm)
    {
        return -1;
    }
    ssize_t r = read(fd, buf, len);
    if (r <= 0)
    {
        return r;
    }

    buf[r] = '\0';

    size_t firstLineLength = strcspn(buf, "\r\n");

    // if read more than one line
    if (firstLineLength < (size_t)r)
    {
        // if couldn't seek after 1st line
        if (-1 == lseek(fd, -((off_t)r - (off_t)firstLineLength - 1), SEEK_CUR))
        {
            return r;
        }
        // if successfully seeked back to the start of 2nd line
        else
        {
            buf[firstLineLength + 1] = '\0';
            return (ssize_t)(firstLineLength + 1);
        }
    }

    return r;
}

int Do(int argc, char const* argv[])
{
    List* fdsList = ListCreate();
    int fdsCount = argc - 1;

    char* readBuf = (char*)calloc(BUFFER_SIZE, sizeof(*readBuf));
    if (!readBuf)
    {
        fwritef(STDERR_FILENO, "malloc(): %s\n", strerror(errno));
        ListDestroy(fdsList);
        return 1;
    }

    fwritef(STDOUT_FILENO, "fd\tfilename\n");
    for (int i = 0; i < fdsCount; i++)
    {
        int curFd;
        if (-1 == (curFd = open(argv[i + 1], O_RDONLY)))
        {
            fwritef(STDERR_FILENO, "%s: %s\n", argv[i + 1], strerror(errno));
            free(readBuf);
            ListDestroy(fdsList);
            return 1;
        }
        if (!ListAppend(fdsList, curFd))
        {
            fwritef(STDERR_FILENO, "Error appending to list\n");
            free(readBuf);
            ListDestroy(fdsList);
            close(curFd);
            return 1;
        }
        fwritef(STDOUT_FILENO, "%d\t%s\n", curFd, argv[i + 1]);
    }

    fwritef(STDOUT_FILENO, "\n\n");

    struct sigaction old_sigalarm_action;

    struct sigaction sigalarm_action;
    sigalarm_action.sa_handler = sigalarm_handler;
    sigfillset(&sigalarm_action.sa_mask);
    sigalarm_action.sa_flags = 0;
    sigaction(SIGALRM, &sigalarm_action, &old_sigalarm_action);

    Node* curFdNode = fdsList->head;
    Node* prevFdNode = NULL;
    while (curFdNode)
    {
        Node* nextFdNode = curFdNode->next;

        sigalarm = false;
        alarm(TIMEOUT_SEC);
        if (prevFdNode != curFdNode)
        {
            fwritef(STDOUT_FILENO, "reading from fd %d...\n", curFdNode->fd);
        }

        ssize_t readCount = readLine(curFdNode->fd, readBuf, BUFFER_SIZE - 1);

        if (!sigalarm)
        {
            alarm(0);
            if (readCount <= 0)
            {
                close(curFdNode->fd);
                ListRemove(fdsList, curFdNode);
            }
            else
            {
                fwritef(STDOUT_FILENO, "%s", readBuf);

                char lastChar = readBuf[readCount - 1];
                if ((lastChar != '\n') && (lastChar != '\r'))
                {
                    nextFdNode = curFdNode;
                }
            }
        }
        else
        {
            fwritef(STDOUT_FILENO, "timeout\n");
        }

        prevFdNode = curFdNode;
        curFdNode = nextFdNode;
        if (!curFdNode)
        {
            curFdNode = fdsList->head;
        }
    }

    sigaction(SIGALRM, &old_sigalarm_action, NULL);

    free(readBuf);
    ListDestroy(fdsList);
    return 0;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
    {
        fwritef(STDERR_FILENO, "Usage: %s file...\n", argv[0]);
        return EXIT_FAILURE;
    }

    return Do(argc, argv);
}
