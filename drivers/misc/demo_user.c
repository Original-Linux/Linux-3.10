#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int fd;
    char *buffer;

    fd = open("/dev/BiscuitOS", O_RDWR);
    if (fd == 0) {
        printf("ERROR: Can't open /dev/BiscuitOS\n");
        return -1;
    }

    buffer = (char *)malloc(sizeof(char) * 40);
    if (!buffer) {
        printf("Can't allocate memory!\n");
        return -1;    
    }

    read(fd, buffer, 3);
    printf("Buffer: %s\n", buffer);

    return 0;    
}
