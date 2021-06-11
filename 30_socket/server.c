#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

const char serverSocketName[] = "socket";
const char closingMsg[] = "stopping server";

bool closeFlag = false;

void sigintHandler()
{
    closeFlag = true;
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

    if (0 != listen(serverFd, 1))
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

    size_t addrLen = sizeof(addr);
    int clientFd = accept(serverFd, (struct sockaddr*)&addr, (socklen_t*)&addrLen);
    if (clientFd < 0)
    {
        if (!closeFlag)
        {
            perror("accept()");
        }
        unlink(serverSocketName);
        close(serverFd);
        return 1;
    }

    printf("new connection (fd=%d)\n", clientFd);

    char buf[1024];
    memset(buf, 0, sizeof(buf));

    ssize_t readCount;
    while (0 < (readCount = read(clientFd, buf, sizeof(buf))))
    {
        printf("message from client: ");
        for (ssize_t i = 0; i < readCount; i++)
        {
            printf("%c", (char)(toupper(buf[i])));
        }
    }

    printf("%s\n", closingMsg);
    unlink(serverSocketName);
    close(clientFd);
    close(serverFd);
    return 0;
}
