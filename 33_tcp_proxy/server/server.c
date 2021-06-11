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
#include "clients.h"

#define BACKLOG 1000
#define DEFAULT_CLIENTS_COUNT 10
#define BUFFER_SIZE 1024

const char closingMsg[] = "stopping server";
struct sockaddr_in remoteSockAddr;
char* remoteHostName;
int remoteHostPort;

bool closeFlag = false;

void sigintHandler()
{
    closeFlag = true;
}

typedef enum CommErrorCode
{
    SUCCESS,
    CONN_CLOSE,
    UNABLE_TO_WRITE,
    SYSCALL_ERR,
} CommErrorCode;

CommErrorCode ForwardData(int srcFd, int destFd)
{
    static char buf[BUFFER_SIZE];

    struct pollfd writePollfd = {
            .fd = destFd,
            .events = POLLOUT,
            .revents = 0
    };

    int writePoll = poll(&writePollfd, 1, 0);

    if (writePoll <= 0)
    {
        if (writePoll < 0 && !closeFlag)
        {
            perror("poll()");
            return SYSCALL_ERR;
        }
        return UNABLE_TO_WRITE;
    }

    ssize_t readCount = recv(srcFd, buf, sizeof(buf), 0);
    if (readCount == -1)
    {
        fprintf(stderr, "fd %d recv(): %s\n", srcFd, strerror(errno));
        return SYSCALL_ERR;
    }

    if (readCount == 0)
    {
        return CONN_CLOSE;
    }

    buf[readCount] = '\0';

    printf("recvd %zd bytes from %d\n", readCount, srcFd);

    ssize_t sentBytes = send(destFd, buf, (size_t)readCount, MSG_NOSIGNAL);
    if (sentBytes == -1)
    {
        fprintf(stderr, "fd %d send(): %s\n", destFd, strerror(errno));
        return SYSCALL_ERR;
    }

    if (sentBytes == 0)
    {
        return CONN_CLOSE;
    }

    printf("sent %zd/%zd bytes to %d\n", sentBytes, readCount, destFd);

    return SUCCESS;
}

int AcceptNewConnection(ClientsInfo* clients)
{
    int clientFd = accept(clients->pollfds[0].fd, (struct sockaddr*)NULL, NULL);
    if (clientFd < 0)
    {
        if (!closeFlag)
        {
            perror("accept()");
        }
        return -1;
    }

    printf("accepted new client (fd=%d)\n", clientFd);

    int remoteConnFd = ConnectToTcpSocket(&remoteSockAddr);
    if (-1 == remoteConnFd)
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error connecting to %s:%d", remoteHostName, remoteHostPort);
        }
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
        return -1;
    }

    printf("connected to %s:%d (fd=%d)\n", remoteHostName, remoteHostPort, remoteConnFd);

    if (!AddClient(clients, clientFd, remoteConnFd))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error adding client with fd=%d", clientFd);
        }
        shutdown(clientFd, SHUT_RDWR);
        shutdown(remoteConnFd, SHUT_RDWR);
        close(clientFd);
        close(remoteConnFd);
        return -1;
    }
    printf("added client to list\n");

    return 0;
}

int ServerLoop(ClientsInfo* clients)
{
    int polledCount = poll(clients->pollfds, clients->pollfdsCount, -1);

    if (-1 == polledCount)
    {
        if (!closeFlag)
        {
            perror("poll()");
        }
        return -1;
    }
    else if (polledCount > 0)
    {
        // accept new connections to our server
        if (clients->pollfds[0].revents & POLLIN)
        {
            printf("accepting new client\n");

            polledCount--;
            clients->pollfds[0].revents = 0;

            if (0 != AcceptNewConnection(clients))
            {
                return -1;
            }
        }

        if (polledCount == 0)
        {
            return 0;
        }

        // clIdx   pollfds
        //  0       1  2
        //  1       3  4
        // ...
        // (i/2)    i  i+1
        for (size_t i = 1; i < clients->pollfdsCount; i++)
        {
            if (clients->pollfds[i].revents & POLLIN)
            {
                clients->pollfds[i].revents = 0;

                size_t clientIndex = (i / 2) - (1 - i % 2);
                Client* curClient = clients->clientsArray + clientIndex;
                CommErrorCode err;

                // check read pollfds
                if (i % 2 == 1)
                {
                    // forward data from client to server
                    err = ForwardData(curClient->clientPollfd->fd, curClient->remotePollfd->fd);
                }

                // check write pollfds
                else
                {
                    // forward data from server to client
                    err = ForwardData(curClient->remotePollfd->fd, curClient->clientPollfd->fd);
                }

                if (err == CONN_CLOSE)
                {
                    printf("closing connection with %zu\n", clientIndex);
                    RemoveClient(curClient);
                }
                else if (err == SYSCALL_ERR)
                {
                    return -1;
                }
                else if (err == UNABLE_TO_WRITE)
                {
                    // try again
                    clients->pollfds[i].revents = POLLIN;
                }
            }
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    close(STDIN_FILENO);

    if (argc != 4)
    {
        fprintf(stderr, "%s listen_port remote_addr remote_port\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (0 != SetSimpleSignalHandler(SIGINT, &sigintHandler))
    {
        PrintError(stderr, "error setting SIGINT handler");
        return EXIT_FAILURE;
    }

    int listenPort;
    if (!str2int(argv[1], &listenPort))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error converting %s to int", argv[1]);
        }
        return EXIT_FAILURE;
    }

    if (!str2int(argv[3], &remoteHostPort))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error converting %s to int", argv[3]);
        }
        return EXIT_FAILURE;
    }

    remoteHostName = argv[2];

    struct sockaddr_in localSockAddr;
    if (0 != GetLocalTcpSockAddr(&localSockAddr, (uint16_t)listenPort))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error creating local sockaddr on port %d", listenPort);
        }
        return EXIT_FAILURE;
    }

    int serverFd = CreateServerTcpSocket(&localSockAddr, (uint16_t)listenPort);
    if (serverFd == -1)
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error creating tcp socket on %s:%d", inet_ntoa(localSockAddr.sin_addr),
                       ntohs(localSockAddr.sin_port));
        }
        return EXIT_FAILURE;
    }

    if (0 != ResolveAddress(&remoteSockAddr, remoteHostName, (uint16_t)remoteHostPort))
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error resolving %s:%d", remoteHostName, remoteHostPort);
        }
        return EXIT_FAILURE;
    }

    printf("resolved %s:%d as %s:%d\n", remoteHostName, remoteHostPort, inet_ntoa(remoteSockAddr.sin_addr),
           ntohs(remoteSockAddr.sin_port));

    size_t clientsCount = DEFAULT_CLIENTS_COUNT;
    ClientsInfo* clients = CreateClientsInfo(clientsCount, serverFd);
    if (!clients)
    {
        if (!closeFlag)
        {
            PrintError(stderr, "error creating clients array");
        }
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        return EXIT_FAILURE;
    }

    printf("listening for connections on %s:%d...\n", inet_ntoa(localSockAddr.sin_addr), listenPort);

    int exitStatus = EXIT_SUCCESS;
    while (!closeFlag)
    {
        if (-1 == ServerLoop(clients))
        {
            exitStatus = EXIT_FAILURE;
            break;
        }
    }

    printf("%s\n", closingMsg);
    close(serverFd);
    RemoveAllClients(clients);
    DestroyClientsInfo(clients);
    return exitStatus;
}
