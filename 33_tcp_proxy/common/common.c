#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

int SetSimpleSignalHandler(int sig, void (* handler)())
{
    struct sigaction sigact;
    sigact.sa_handler = handler;
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    return sigaction(sig, &sigact, NULL);
}

bool str2int(const char* str, int* num)
{
    if (!str || !num)
    {
        return false;
    }

    char* end;
    long conv;
    errno = 0;
    conv = strtol(str, &end, 10);
    if ((errno == ERANGE && conv == LONG_MAX) ||
        (errno == ERANGE && conv == LONG_MIN))
    {
        return false;
    }
    if (*str == '\0' || *end != '\0')
    {
        errno = EINVAL;
        return false;
    }
    *num = (int)conv;
    return true;
}

void PrintError(FILE* stream, const char* format, ...)
{
    va_list valist;
    va_start(valist, format);

    int oldErrno = errno;

    vfprintf(stream, format, valist);

    if (oldErrno != 0)
    {
        fprintf(stream, " (%s)", strerror(oldErrno));
    }

    fprintf(stream, "\n");

    oldErrno = errno;
}