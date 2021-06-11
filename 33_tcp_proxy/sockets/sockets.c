#include "sockets.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int GetLocalTcpSockAddr(struct sockaddr_in* sockAddr, uint16_t port)
{
    if (!sockAddr)
    {
        return -1;
    }

    memset(sockAddr, 0, sizeof(*sockAddr));

    sockAddr->sin_family = AF_INET;
    sockAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    sockAddr->sin_port = htons(port);

    return 0;
}

int CreateServerTcpSocket(const struct sockaddr_in* sockAddr, int backlog)
{
    if (!sockAddr)
    {
        return -1;
    }

    int sockFd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockFd < 0)
    {
        return -1;
    }

    if (0 != bind(sockFd, (const struct sockaddr*)sockAddr, sizeof(*sockAddr)))
    {
        close(sockFd);
        return -1;
    }

    if (0 != listen(sockFd, backlog))
    {
        close(sockFd);
        return -1;
    }

    return sockFd;
}

int ResolveAddress(struct sockaddr_in* sockAddr, const char* host, uint16_t port)
{
    if (!sockAddr || !host)
    {
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (0 != getaddrinfo(host, NULL, &hints, &res))
    {
        return -1;
    }

    memcpy(sockAddr, (void*)res->ai_addr, sizeof(*sockAddr));
    sockAddr->sin_port = htons(port);

    freeaddrinfo(res);
    return 0;
}

int ConnectToTcpSocket(struct sockaddr_in* sockAddr)
{
    if (!sockAddr)
    {
        return -1;
    }

    int connFd = socket(PF_INET, SOCK_STREAM, 0);
    if (connFd < 0)
    {
        return -1;
    }

    if (0 > connect(connFd, (struct sockaddr*)sockAddr, sizeof(*sockAddr)))
    {
        close(connFd);
        return -1;
    }

    return connFd;
}
