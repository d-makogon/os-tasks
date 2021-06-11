#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <malloc.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void printDirInfo(FILE* stream, const char* dirName)
{
    struct stat dir;
    if (lstat(dirName, &dir) != 0)
    {
        fprintf(stderr, "error opening %s: %s\n", dirName, strerror(errno));
        return;
    }

    // regular file
    int isFile = S_ISREG(dir.st_mode);

    char mode[11] = {0};

    mode[0] = (isFile) ? '-' : (S_ISDIR(dir.st_mode) ? 'd' : '?');
    mode[1] = (dir.st_mode & S_IRUSR) ? 'r' : '-';
    mode[2] = (dir.st_mode & S_IWUSR) ? 'w' : '-';
    mode[3] = (dir.st_mode & S_ISUID) ? 's' : ((dir.st_mode & S_IXUSR) ? 'x' : '-');
    mode[4] = (dir.st_mode & S_IRGRP) ? 'r' : '-';
    mode[5] = (dir.st_mode & S_IWGRP) ? 'w' : '-';
    mode[6] = (dir.st_mode & S_ISGID) ? 's' : ((dir.st_mode & S_IXGRP) ? 'x' : '-');
    mode[7] = (dir.st_mode & S_IROTH) ? 'r' : '-';
    mode[8] = (dir.st_mode & S_IWOTH) ? 'w' : '-';
    mode[9] = (dir.st_mode & S_IXOTH) ? 'x' : '-';

    struct passwd* user = getpwuid(dir.st_uid);
    struct group* group = getgrgid(dir.st_gid);

    // %X.Ys - at most Y chars in field of length X (right-justified)
    fprintf(stream, "%10.10s %2zu ", mode, dir.st_nlink);
    if (user)
    {
        fprintf(stream, "%15.15s ", user->pw_name);
    }
    else
    {
        fprintf(stream, "%15d ", dir.st_uid);
    }

    if (group)
    {
        fprintf(stream, "%15.15s ", group->gr_name);
    }
    else
    {
        fprintf(stream, "%15d ", dir.st_gid);
    }

    if (isFile)
    {
        fprintf(stream, "%5zu ", dir.st_size);
    }
    else
    {
        fprintf(stream, "      ");
    }
    time_t t = dir.st_mtime;
    fprintf(stream, "%20.20s %s\n", ctime(&t), dirName);
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
    {
        printDirInfo(stdout, ".");
        return 0;
    }
    for (int i = 1; i < argc; i++)
    {
        printDirInfo(stdout, argv[i]);
    }
    return 0;
}
