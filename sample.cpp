#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

int main()
{
    int fd;
    if( (fd = open("/dev/irrecv0", O_RDWR)) < 0 )
    {
        perror("open");
        return 1;
    } 
    while( true )
    {
        usleep(500000);
        uint32_t buf;
        int ret = ioctl(fd, 0, &buf);
        if( ret < 0 ) 
        {
            perror("ioctl");
            return 2;
        }
        if( buf > 0 )
        {
            printf("code: 0x%08X\n", buf);
        }
    }
    if( close(fd) != 0 ) 
    {
        perror("close");
        return 3;
    }
    
    return 0;
}
