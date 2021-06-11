#include <aio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 4
#define MAX_CLIENTS_QUEUE 4
#define DEFAULT_CLIENTS_N 4
#define BUFFER_SIZE 1024

const char serverSocketName[] = "socket";
const char closingMsg[] = "stopping server";

bool closeFlag = false;

void sigintHandler()
{
    closeFlag = true;
}

struct buf
{
    bool isUsed;
    struct aiocb aiocb;
    unsigned char* data;
};

struct buf* curBuf = NULL;

int nClients = 0;

void processClient(struct buf* reqBuf)
{
    ssize_t readCount = aio_return(&reqBuf->aiocb);
    if (readCount > 0)
    {
        printf("message from %d: ", reqBuf->aiocb.aio_fildes);
        for (ssize_t i = 0; i < readCount; i++)
        {
            int c = toupper(reqBuf->data[i]);
            if (c != fputc(c, stdout))
            {
                if (errno != EINTR)
                {
                    perror("fputc()");
                }
                return;
            }
        }
        fflush(stdout);
        if (aio_read(&reqBuf->aiocb) < 0)
        {
            if (!closeFlag)
            {
                perror("aio_read()");
            }
        }
    }
    else
    {
        // do not poll this client anymore
        printf("%d disconnected\n", reqBuf->aiocb.aio_fildes);
        close(reqBuf->aiocb.aio_fildes);
        reqBuf->isUsed = false;
        nClients--;
    }
}

struct buf* initClient(struct buf* client, int fd)
{
    client->isUsed = (fd != -1);
    client->aiocb.aio_fildes = fd;
    client->aiocb.aio_buf = client->data;
    client->aiocb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    client->aiocb.aio_sigevent.sigev_signo = SIGIO;
    client->aiocb.aio_sigevent.sigev_value.sival_ptr = client;
    client->aiocb.aio_offset = 0;
    client->aiocb.aio_nbytes = sizeof(client->data);
    return client;
}

struct buf* addClient(int clientFd, struct buf** bufs, size_t* bufsCount)
{
    for (size_t i = 0; i < *bufsCount; i++)
    {
        struct buf* curBuf = *bufs + i;
        if (!curBuf->isUsed)
        {
            curBuf->isUsed = true;
            curBuf->aiocb.aio_fildes = clientFd;
            nClients++;
            return curBuf;
        }
    }
    return NULL;
}

struct buf* createClientsList(size_t size)
{
    struct buf* clientsArray = (struct buf*)calloc(size, sizeof(*clientsArray));
    if (!clientsArray)
    {
        return NULL;
    }
    for (size_t i = 0; i < size; i++)
    {
        unsigned char* data = (unsigned char*)malloc(BUFFER_SIZE * sizeof(*data));
        if (!data)
        {
            perror("malloc()");
            unlink(serverSocketName);
            exit(1);
        }
        clientsArray[i].data = data;
        initClient(clientsArray + i, -1);
    }
    return clientsArray;
}

sigjmp_buf toread;

void sigioHandler(int signo, siginfo_t* info, void* context)
{
    if (signo != SIGIO || info->si_signo != SIGIO)
    {
        return;
    }

    struct buf* reqBuf = (struct buf*)info->si_value.sival_ptr;
    int err = aio_error(&reqBuf->aiocb);
    if (err == 0)
    {
        curBuf = reqBuf;
        siglongjmp(toread, 1);
    }
}

int main()
{
    struct sigaction sigintAction;
    sigintAction.sa_handler = &sigintHandler;
    sigfillset(&(sigintAction.sa_mask));
    sigintAction.sa_flags = 0;

    struct sigaction sigioAction;
    memset(&sigioAction, 0, sizeof(sigioAction));
    sigioAction.sa_sigaction = &sigioHandler;
    sigioAction.sa_flags = SA_SIGINFO;

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
    sigaction(SIGIO, &sigioAction, NULL);

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

    size_t bufsCount = DEFAULT_CLIENTS_N;
    struct buf* bufs = createClientsList(bufsCount);
    if (!bufs)
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

    int exitStatus = EXIT_SUCCESS;

    sigset_t sigioSet;
    sigaddset(&sigioSet, SIGIO);

    sigprocmask(SIG_BLOCK, &sigioSet, NULL);

    // reading
    if (sigsetjmp(toread, 1) == 1)
    {
        processClient(curBuf);
    }
    sigprocmask(SIG_UNBLOCK, &sigioSet, NULL);

    while (!closeFlag)
    {
        if (nClients < MAX_CLIENTS_QUEUE)
        {
            int clientFd = accept(serverFd, (struct sockaddr*)&addr, (socklen_t*)&addrLen);
            if (clientFd < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                perror("accept()");
                continue;
            }

            // add client and initiate aio_read
            struct buf* newClient = addClient(clientFd, &bufs, &bufsCount);
            if (!newClient)
            {
                if (!closeFlag)
                {
                    fprintf(stderr, "error adding client %d\n", clientFd);
                    continue;
                }
                else
                {
                    break;
                }
            }

            if (aio_read(&newClient->aiocb) < 0)
            {
                if (!closeFlag)
                {
                    fprintf(stderr, "aio_read() queuing error (%d): %s\n", errno, strerror(errno));
                    exitStatus = EXIT_FAILURE;
                }
                break;
            }
            printf("new client (fd=%d)\n", clientFd);
        }
        else
        {
            pause();
        }
    }

    printf("%s\n", closingMsg);
    unlink(serverSocketName);
    close(serverFd);
    for (size_t i = 0; i < bufsCount; i++)
    {
        if (bufs[i].isUsed)
        {
            free(bufs[i].data);
            close(bufs[i].aiocb.aio_fildes);
        }
    }

    free(bufs);
    return exitStatus;
}
