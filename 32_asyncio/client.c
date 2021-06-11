#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/conf.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

const char serverSocketName[] = "socket";
const char closingMsg[] = "closing connection";

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
    strncpy(addr.sun_path, serverSocketName, sizeof(addr.sun_path) - 1);

    sigaction(SIGINT, &sigintAction, NULL);

    if (0 != connect(socketFd, (struct sockaddr*)&addr, sizeof(addr)))
    {
        if (!closeFlag)
        {
            fprintf(stderr, "couldn't connect to server\n");
        }
        close(socketFd);
        return 1;
    }

    printf("connected to %s\n", serverSocketName);

    char buf[1024];

    printf("> ");
    fflush(stdout);

    while (fgets(buf, sizeof(buf), stdin))
    {
        ssize_t sentBytes = send(socketFd, buf, strcspn(buf, "\0") + 1, MSG_NOSIGNAL);
        if (-1 == sentBytes)
        {
            if (!closeFlag)
            {
                fprintf(stderr, "server closed the connection\n");
            }
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
