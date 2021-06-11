#include <ctype.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS_QUEUE 2
#define DEFAULT_CLIENTS_N 2
#define BUFFER_SIZE 1024

const char serverSocketName[] = "socket";
const char closingMsg[] = "stopping server";

bool closeFlag = false;

void sigintHandler()
{
    closeFlag = true;
}

void processClient(struct pollfd* clientPollFd)
{
    static char buf[BUFFER_SIZE];
    ssize_t readCount = read(clientPollFd->fd, buf, sizeof(buf));
    if (readCount > 0)
    {
        printf("message from %d: ", clientPollFd->fd);
        for (ssize_t i = 0; i < readCount; i++)
        {
            printf("%c", (char)(toupper(buf[i])));
        }
        fflush(stdout);
    }
    else
    {
        // do not poll from this client anymore
        printf("%d disconnected\n", clientPollFd->fd);
        close(clientPollFd->fd);
        clientPollFd->fd = -1;
    }
}

bool addClient(int clientFd, struct pollfd** pollFds, size_t startIdx, size_t* pollFdsSize)
{
    for (size_t i = startIdx; i < *pollFdsSize; i++)
    {
        if ((*pollFds)[i].fd == -1)
        {
            (*pollFds)[i].fd = clientFd;
            return true;
        }
    }

    size_t newClientsN = 20;

    struct pollfd* newArray = (struct pollfd*)realloc(*pollFds, (*pollFdsSize + newClientsN) * sizeof(*newArray));
    if (!newArray)
    {
        return false;
    }
    *pollFds = newArray;

    struct pollfd* client = &((*pollFds)[*pollFdsSize]);
    client->fd = clientFd;
    client->events = POLLIN;
    client->revents = 0;

    for (size_t i = 1; i < newClientsN; i++)
    {
        (*pollFds)[i + *pollFdsSize].fd = -1;
        (*pollFds)[i + *pollFdsSize].events = POLLIN;
        (*pollFds)[i + *pollFdsSize].revents = 0;
    }

    *pollFdsSize += newClientsN;
    return true;
}

struct pollfd* createClientsList(size_t size)
{
    struct pollfd* clientsArray = (struct pollfd*)calloc(size, sizeof(*clientsArray));
    if (!clientsArray)
    {
        return NULL;
    }
    for (size_t i = 0; i < size; i++)
    {
        clientsArray[i].fd = -1;
        clientsArray[i].events = POLLIN;
        clientsArray[i].revents = 0;
    }
    return clientsArray;
}

int main()
{
    struct sigaction sigintAction;
    sigintAction.sa_handler = &sigintHandler;
    sigfillset(&(sigintAction.sa_mask));
    sigintAction.sa_flags = 0;

    int serverFd;
    if (-1 == (serverFd = socket(PF_UNIX, SOCK_STREAM, 0)))
    {
        perror("socket()");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, serverSocketName, sizeof(addr.sun_path) - 1);

    if (0 != bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)))
    {
        perror("bind()");
        close(serverFd);
        return 1;
    }

    sigaction(SIGINT, &sigintAction, NULL);

    if (0 != listen(serverFd, MAX_CLIENTS_QUEUE))
    {
        if (!closeFlag)
        {
            perror("listen()");
        }
        unlink(serverSocketName);
        close(serverFd);
        return 1;
    }

    printf("listening for connections...\n");

    size_t pollFdsSize = DEFAULT_CLIENTS_N;
    struct pollfd* pollFds = createClientsList(pollFdsSize);
    if (!pollFds)
    {
        if (!closeFlag)
        {
            perror("listen()");
        }
        unlink(serverSocketName);
        close(serverFd);
        return 1;
    }

    size_t addrLen = sizeof(addr);

    // server pollfd
    pollFds[0].fd = serverFd;
    pollFds[0].events = POLLIN;
    pollFds[0].revents = 0;

    int exitStatus = EXIT_SUCCESS;

    while (!closeFlag)
    {
        // poll for connections or messages
        int polledCount = poll(pollFds, pollFdsSize, -1);

        if (-1 == polledCount)
        {
            if (!closeFlag)
            {
                perror("poll()");
                exitStatus = EXIT_FAILURE;
            }
            break;
        }
        else if (polledCount > 0)
        {
            // if there are new connections
            if (pollFds[0].revents & POLLIN)
            {
                polledCount--;
                pollFds[0].revents = 0;

                // accept new client
                int clientFd = accept(serverFd, (struct sockaddr*)&addr, (socklen_t*)&addrLen);
                if (clientFd < 0)
                {
                    if (!closeFlag)
                    {
                        perror("accept()");
                        exitStatus = EXIT_FAILURE;
                    }
                    break;
                }

                printf("new connection (fd=%d)\n", clientFd);

                // store clientFd in array
                if (!addClient(clientFd, &pollFds, 1, &pollFdsSize))
                {
                    if (!closeFlag)
                    {
                        fprintf(stderr, "error adding client\n");
                        exitStatus = EXIT_FAILURE;
                    }
                    break;
                }
            }

            if (polledCount == 0)
            {
                continue;
            }

            // receive msgs from clients
            for (size_t i = 1; i < pollFdsSize; i++)
            {
                if (pollFds[i].revents & POLLIN)
                {
                    processClient(pollFds + i);
                }
            }
        }
    }

    printf("%s\n", closingMsg);
    unlink(serverSocketName);
    close(serverFd);
    for (size_t i = 0; i < pollFdsSize; i++)
    {
        if (pollFds[i].fd != -1)
        {
            close(pollFds[i].fd);
        }
    }
    free(pollFds);

    return exitStatus;
}
