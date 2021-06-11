#include <ctype.h>
#include <stdio.h>

int main()
{
    char buf[BUFSIZ];
    while (fgets(buf, BUFSIZ, stdin) != NULL)
    {
        for (size_t i = 0; BUFSIZ; i++)
        {
            if (buf[i] == 0)
            {
                break;
            }
            else
            {
                printf("%c", toupper(buf[i]));
            }
        }
    }
    return 0;
}
