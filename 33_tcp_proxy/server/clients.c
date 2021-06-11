#include "clients.h"

#include <stdlib.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>

#define REALLOC_COUNT 20

void InitPollfd(struct pollfd* pollfd, int fd)
{
    if (!pollfd)
    {
        return;
    }

    pollfd->fd = fd;
    pollfd->events = POLLIN;
    pollfd->revents = 0;
}

ClientsInfo* CreateClientsInfo(size_t initialClientsCount, int serverFd)
{
    ClientsInfo* clientsInfo = (ClientsInfo*)calloc(1, sizeof(*clientsInfo));
    if (!clientsInfo)
    {
        return NULL;
    }

    Client* clientsArray = (Client*)calloc(initialClientsCount, sizeof(*clientsArray));
    if (!clientsArray)
    {
        free(clientsInfo);
        return NULL;
    }

    struct pollfd* pollfds = (struct pollfd*)calloc(initialClientsCount * 2 + 1, sizeof(*pollfds));
    if (!pollfds)
    {
        free(clientsInfo);
        free(clientsArray);
        return NULL;
    }

    // init server pollfd for accepting new connections
    InitPollfd(pollfds, serverFd);

    // init clients with fds == -1
    for (size_t i = 0; i < initialClientsCount; i++)
    {
        clientsArray[i].clientPollfd = &pollfds[i * 2 + 1];
        clientsArray[i].remotePollfd = &pollfds[i * 2 + 2];
        InitClient(clientsArray + i, -1, -1);
    }

    clientsInfo->clientsArray = clientsArray;
    clientsInfo->clientsCount = initialClientsCount;
    clientsInfo->pollfds = pollfds;
    clientsInfo->pollfdsCount = initialClientsCount * 2 + 1;

    return clientsInfo;
}

void DestroyClientsInfo(ClientsInfo* clients)
{
    if (clients)
    {
        if (clients->clientsArray)
        {
            free(clients->clientsArray);
        }
        if (clients->pollfds)
        {
            free(clients->pollfds);
        }
        free(clients);
    }
}

void InitClient(Client* client, int clientFd, int remoteConnFd)
{
    if (!client)
    {
        return;
    }

    client->isUsed = (clientFd != -1);
    InitPollfd(client->clientPollfd, clientFd);
    InitPollfd(client->remotePollfd, remoteConnFd);
}

bool ResizeClientsInfo(ClientsInfo* clients, size_t newSize)
{
    if (!clients)
    {
        return false;
    }

    Client* clientsArray = (Client*)realloc(clients->clientsArray, newSize * sizeof(*(clients->clientsArray)));
    if (!clientsArray)
    {
        return false;
    }

    struct pollfd* pollfds = (struct pollfd*)realloc(clients->pollfds, (newSize * 2 + 1) * sizeof(*(clients->pollfds)));
    if (!pollfds)
    {
        free(clientsArray);
        return false;
    }

    clients->clientsArray = clientsArray;
    clients->pollfds = pollfds;
    clients->clientsCount = newSize;
    clients->pollfdsCount = newSize * 2 + 1;

    for (size_t i = 0; i < newSize; i++)
    {
        clients->clientsArray[i].clientPollfd = &clients->pollfds[i * 2 + 1];
        clients->clientsArray[i].remotePollfd = &clients->pollfds[i * 2 + 2];
    }

    return true;
}

bool AddClient(ClientsInfo* clients, int clientFd, int remoteConnFd)
{
    if (!clients)
    {
        return false;
    }

    for (size_t i = 0; i < clients->clientsCount; i++)
    {
        Client* curClient = clients->clientsArray + i;

        // if there is empty slot in clientsArray
        if (!curClient->isUsed)
        {
            InitClient(curClient, clientFd, remoteConnFd);
            return curClient;
        }
    }

    // realloc if no space left
    size_t oldClientsCount = clients->clientsCount;
    size_t newClientsCount = oldClientsCount + REALLOC_COUNT;

    if (!ResizeClientsInfo(clients, newClientsCount))
    {
        return false;
    }

    // init new client
    InitClient(clients->clientsArray + oldClientsCount, clientFd, remoteConnFd);

    // init other newly allocated clients with fds == -1
    for (size_t i = oldClientsCount + 1; i < newClientsCount; i++)
    {
        InitClient(clients->clientsArray + i, -1, -1);
    }

    return true;
}

void RemoveClient(Client* client)
{
    if (!client)
    {
        return;
    }

    shutdown(client->clientPollfd->fd, SHUT_RDWR);
    shutdown(client->remotePollfd->fd, SHUT_RDWR);
    close(client->clientPollfd->fd);
    close(client->remotePollfd->fd);

    InitClient(client, -1, -1);
}

void RemoveAllClients(ClientsInfo* clients)
{
    if (!clients)
    {
        return;
    }

    for (size_t i = 0; i < clients->clientsCount; i++)
    {
        RemoveClient(clients->clientsArray + i);
    }
}
