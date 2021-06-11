#pragma once

#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct Client
{
    struct pollfd* clientPollfd; // read/write from/to client
    struct pollfd* remotePollfd; // read/write from/to remote host
    bool isUsed;
} Client;

typedef struct ClientsInfo
{
    Client* clientsArray;
    size_t clientsCount;

    struct pollfd* pollfds;
    size_t pollfdsCount;
} ClientsInfo;

ClientsInfo* CreateClientsInfo(size_t initialClientsCount, int serverFd);

void DestroyClientsInfo(ClientsInfo* clients);

void InitClient(Client* client, int clientFd, int remoteConnFd);

bool AddClient(ClientsInfo* clients, int clientFd, int remoteConnFd);

void RemoveClient(Client* client);

void RemoveAllClients(ClientsInfo* clients);
