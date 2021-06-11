#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

int execvpe(const char* file, char* const argv[], char* const envp[])
{
    char** oldEnv = environ;

    // if file contains path to executable
    if (strchr(file, '/'))
    {
        execve(file, argv, envp);
        perror("execve() error");
        return -1;
    }

    // else if file is just a name of executable
    // then search for this executable in PATH

    char* path = getenv("PATH");

    if (!path)
    {
        execve(file, argv, envp);
        perror("execve() error");
        return -1;
    }

    /*
    * https://github.com/illumos/illumos-gate/blob/29940bf8af05acccf7b0a08eec421d2c1db7d560/usr/src/uts/common/os/exec.c#L1461
    * 
    * execve(path, argv, envp) must construct a new stack with the specified
    * arguments and environment variables (see exec_args() for a description
    * of the user stack layout).  To do this, we copy the arguments and
    * environment variables from the old user address space into the kernel,
    * free the old as, create the new as, and copy our buffered information
    * to the new stack. 
    * 
    */
    environ = (char**)envp;

    size_t fileNameLength = strnlen(file, sysconf(_SC_ARG_MAX));

    long maxPathLenFromRoot = pathconf("/", _PC_PATH_MAX);
    char* pathFromRoot = (char*)calloc(maxPathLenFromRoot + 2, sizeof(*pathFromRoot));
    if (!pathFromRoot)
    {
        perror("malloc() error");
        environ = oldEnv;
        return -1;
    }

    for (char* pathDir = strtok(path, ":"); pathDir; pathDir = strtok(NULL, ":"))
    {
        pathFromRoot[0] = '\0';
        strcat(pathFromRoot, pathDir);
        strcat(pathFromRoot, "/");
        strncat(pathFromRoot, file, pathconf(pathFromRoot, _PC_PATH_MAX));

        if (0 == access(pathFromRoot, F_OK))
        {
            // file exists, execute
            execve(pathFromRoot, argv, envp);
            perror("execve() error");
            environ = oldEnv;
            free(pathFromRoot);
            return -1;
        }
    }

    execvp(file, argv);

    perror("execvp() error");
    free(pathFromRoot);
    environ = oldEnv;
    return -1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s program_name [args]...\n", argv[0]);
        return 1;
    }

    char* newEnv[] = {"MYVAR=MYVALUE", NULL};

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork() error");
        return 1;
    }
    else if (pid == 0)
    {
        execvpe(argv[1], argv + 1, newEnv);
        return errno;
    }
    else
    {
        int status = 0;
        if (pid != waitpid(pid, &status, 0))
        {
            perror("waitpid error");
            return 1;
        }
        if (!WIFEXITED(status))
        {
            fprintf(stderr, "child process didn't terminate normally\n");
            return 1;
        }
        else
        {
            int childExitStatus = WEXITSTATUS(status);
            printf("%s exited with status %d\n", argv[1], childExitStatus);
            return childExitStatus;
        }
    }
}
