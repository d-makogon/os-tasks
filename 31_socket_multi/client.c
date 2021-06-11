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
const char closingMsg[] = "stopping client";

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

    int socketFd;
    if (-1 == (socketFd = socket(PF_UNIX, SOCK_STREAM, 0)))
    {
        perror("socket()");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "socket", sizeof(addr.sun_path) - 1);

    if (0 != connect(socketFd, (struct sockaddr*)&addr, sizeof(addr)))
    {
        perror("connect()");
        close(socketFd);
        return 1;
    }

    sigaction(SIGINT, &sigintAction, NULL);

    printf("connected to %s\n", serverSocketName);

    char buf[1024];

    printf("> ");
    fflush(stdout);

    while (fgets(buf, sizeof(buf), stdin))
    {
        if (-1 == send(socketFd, buf, strcspn(buf, "\0") + 1, MSG_NOSIGNAL))
        {
            if (!closeFlag)
            {
                printf("server closed the connection\n");
            }

            printf("%s", closingMsg);
            close(socketFd);
            return 1;
        }

        printf("> ");
        fflush(stdout);
    }

    printf("%s\n", closingMsg);
    close(socketFd);
    return 0;
}
