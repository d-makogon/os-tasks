#pragma once

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>

int GetLocalTcpSockAddr(struct sockaddr_in* sockAddr, uint16_t port);

int CreateServerTcpSocket(const struct sockaddr_in* sockAddr, int backlog);

int ResolveAddress(struct sockaddr_in* sockAddr, const char* host, uint16_t port);

int ConnectToTcpSocket(struct sockaddr_in* sockAddr);
