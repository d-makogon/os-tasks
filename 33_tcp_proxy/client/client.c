#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../sockets/sockets.h"

#define BUFFER_SIZE 1024

bool closeFlag = false;

void SigintHandler()
{
    closeFlag = true;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (0 != SetSimpleSignalHandler(SIGINT, &SigintHandler))
    {
        PrintError(stderr, "error setting SIGINT handler");
        return EXIT_FAILURE;
    }

    const char* host = argv[1];
    int port;
    if (!str2int(argv[2], &port))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error converting %s to int", argv[2]);
        }
        return EXIT_FAILURE;
    }

    struct sockaddr_in remoteAddr;
    if (0 != ResolveAddress(&remoteAddr, host, (uint16_t)port))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error resolving %s:%d", host, port);
        }
        return EXIT_FAILURE;
    }

    int connFd = ConnectToTcpSocket(&remoteAddr);
    if (connFd < 0)
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error connecting to %s:%d", host, port);
        }
        return EXIT_FAILURE;
    }

    char buf[BUFFER_SIZE];
    memset(buf, '\0', sizeof(buf));

    struct pollfd sockReadPollfd;
    sockReadPollfd.fd = connFd;
    sockReadPollfd.events = POLLIN;
    sockReadPollfd.revents = 0;

    struct pollfd usrReadPollfd;
    usrReadPollfd.fd = STDIN_FILENO;
    usrReadPollfd.events = POLLIN;
    usrReadPollfd.revents = 0;

    struct pollfd pollfds[2];
    pollfds[0] = sockReadPollfd;
    pollfds[1] = usrReadPollfd;

    while (1)
    {
        int polledCount = poll(pollfds, 2, -1);
        if (polledCount == -1)
        {
            if (!closeFlag)
            {
                perror("poll()");
            }
            shutdown(connFd, SHUT_RDWR);
            close(connFd);
            return EXIT_FAILURE;
        }
        else
        {
            // read from remote host
            if (pollfds[0].revents & POLLIN)
            {
                pollfds[0].revents = 0;

                ssize_t readCount = read(connFd, buf, sizeof(buf));

                // read failure
                if (readCount < 0)
                {
                    if (!closeFlag)
                    {
                        perror("read");
                    }
                    shutdown(connFd, SHUT_RDWR);
                    close(connFd);
                    return EXIT_FAILURE;
                }

                // close connection
                if (readCount == 0)
                {
                    fprintf(stderr, "host closed the connection\n");
                    break;
                }

                buf[readCount] = '\0';

                fprintf(stderr, "read %zd bytes:\n", readCount);
                printf("%s", buf);
                fflush(stdout);
            }

            // read from client
            if (pollfds[1].revents & POLLIN)
            {
                pollfds[1].revents = 0;

                if (!fgets(buf, sizeof(buf), stdin))
                {
                    if (errno != 0)
                    {
                        perror("fgets()");
                        shutdown(connFd, SHUT_RDWR);
                        close(connFd);
                        return EXIT_FAILURE;
                    }
                    continue;
                }

                ssize_t writtenBytes = write(connFd, buf, 1 + strcspn(buf, "\n"));

                // write failure
                if (writtenBytes < 0)
                {
                    if (!closeFlag)
                    {
                        perror("write()");
                    }
                    shutdown(connFd, SHUT_RDWR);
                    close(connFd);
                    return EXIT_FAILURE;
                }

                // close connection
                if (writtenBytes == 0)
                {
                    fprintf(stderr, "host closed the connection\n");
                    break;
                }

                fprintf(stderr, "written %zd bytes\n", writtenBytes);
            }
        }
    }

    close(connFd);
    shutdown(connFd, SHUT_RDWR);
    return EXIT_SUCCESS;
}
