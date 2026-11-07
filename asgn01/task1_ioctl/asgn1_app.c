#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "asgn1_ioctl.h"

#define AS01_DEVICE_PATH "/dev/asgn1"

int main(int argc, char *argv[])
{
    int fd;
    int ret = 0;
    int mode;
    struct as01_stats stats;
    char version[AS01_BUFFER_VERSION_LEN];

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <command> [args]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  reset_buffer\n");
        fprintf(stderr, "  get_stats\n");
        fprintf(stderr, "  set_mode <0|1>\n");
        fprintf(stderr, "  get_version\n");
        fprintf(stderr, "  bad_command\n");
        return 1;
    }
    fd = open(AS01_DEVICE_PATH, O_RDWR);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }

    if (strcmp(argv[1], "reset_buffer") == 0)
    {
        if (ioctl(fd, AS01_RESET_BUFFER) < 0)
        {
            perror("ioctl RESET_BUFFER");
            ret = 1;
        }
        else
        {
            printf("Buffer reset successfully.\n");
        }
    }
    else if (strcmp(argv[1], "get_stats") == 0)
    {
        if (ioctl(fd, AS01_GET_STATS, &stats) < 0)
        {
            perror("ioctl GET_STATS");
            ret = 1;
        }
        else
        {
            printf("Open count: %d\n", stats.open_count);
            printf("Write count: %d\n", stats.write_count);
            printf("Read count: %d\n", stats.read_count);
            printf("Buffer length: %d\n", stats.buffer_len);
            printf("Last write size: %d\n", stats.last_write_size);
        }
    }
    else if (strcmp(argv[1], "set_mode") == 0){
        if (argc < 3)
        {
            fprintf(stderr, "Usage: %s set_mode <0|1>\n", argv[0]);
            ret = 1;
        }
        else
        {
            mode = atoi(argv[2]);
            if (ioctl(fd, AS01_SET_MODE, &mode) < 0)
            {
                perror("ioctl SET_MODE");
                ret = 1;
            }
            else
            {
                printf("Mode set to %d.\n", mode);
            }
        }
    }
    else if (strcmp(argv[1], "get_version") == 0)
    {
        if (ioctl(fd, AS01_GET_VERSION, version) < 0)
        {
            perror("ioctl GET_VERSION");
            ret = 1;
        }
        else
        {
            printf("Device version: %s\n", version);
        }
    }
    else if (strcmp(argv[1], "bad_command") == 0)
    {
        if (ioctl(fd, _IO(AS01_MAGIC_NUMBER, 99)) < 0 && errno == ENOTTY)
        {
            perror("ioctl BAD_COMMAND");
            printf("Bad command test passed: ioctl returned ENOTTY as expected.\n");
        }
     else {
        fprintf(stderr, "Error: Bad command test failed");
        ret =1;
    }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        ret = 1;
    }

    close(fd);
    return ret;
}
