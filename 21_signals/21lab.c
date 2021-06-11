#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int timesRinged = 0;
int quit = 0;

void sigint_handler()
{
    write(STDOUT_FILENO, "\a", 1);
    timesRinged++;
}

void sigquit_handler()
{
    quit = 1;
}

int main()
{
    struct sigaction sigint_action;
    sigint_action.sa_handler = sigint_handler;
    if (0 != sigaddset(&sigint_action.sa_mask, SIGQUIT))
    {
        perror("sigaddset()");
        return EXIT_FAILURE;
    }
    sigint_action.sa_flags = 0;
    if (0 != sigaction(SIGINT, &sigint_action, NULL))
    {
        perror("sigaction()");
        return EXIT_FAILURE;
    }

    struct sigaction sigquit_action;
    sigquit_action.sa_handler = sigquit_handler;
    if (0 != sigaddset(&sigquit_action.sa_mask, SIGINT))
    {
        perror("sigaddset()");
        return EXIT_FAILURE;
    }
    sigquit_action.sa_flags = 0;
    if (0 != sigaction(SIGQUIT, &sigquit_action, NULL))
    {
        perror("sigaction()");
        return EXIT_FAILURE;
    }

    while (!quit)
    {
        pause();
    }

    printf("\nTimes ringed: %d\n", timesRinged);

    return 0;
}
