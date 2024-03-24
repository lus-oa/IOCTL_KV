#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

// #define MY_IOCTL_CMD _IOWR('M', 1, char *)
#define CMD_IOC_MAGIC 'a'
#define WRITE_IOCTL_CMD _IOW(CMD_IOC_MAGIC, 1, char *) // Write Buffer Address to Linux Kernel
#define READ_IOCTL_CMD _IOR(CMD_IOC_MAGIC, 2, char *)  // Read data from kernel to buffer

void time_statistic()
{
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME, &currentTime);
    struct tm *timeinfo;
    time_t seconds = currentTime.tv_sec;
    timeinfo = localtime(&seconds);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
    printf("User Start Time: %s.%09ld\n", buf, currentTime.tv_nsec);
}

int main()
{
    int fd;
    char *buffer;
    size_t buffer_size = 1024 * 1024 * 10; // 1 MB buffer

    buffer = malloc(buffer_size);
    buffer[0] = 'a';
    buffer[buffer_size / 2] = 'b';
    buffer[buffer_size - 1] = 'c';
    if (!buffer)
    {
        perror("Failed to allocate buffer");
        return 1;
    }

    fd = open("/dev/mychardev", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open device");
        free(buffer);
        return 1;
    }
    time_statistic(); // Record the time when write data to kernel
    if (ioctl(fd, WRITE_IOCTL_CMD, buffer) < 0)
    { // First Write
        perror("ioctl failed");
        close(fd);
        free(buffer);
        return 1;
    }
    else
    {
        if (ioctl(fd, READ_IOCTL_CMD, buffer) < 0)
        { // After Write successfully, then Read
            perror("ioctl failed");
            close(fd);
            free(buffer);
            return 1;
        }
        else
        {
            printf("USER Receive the Data From kernel: %c %c %c", buffer[0], buffer[buffer_size / 2], buffer[buffer_size - 1]);
        }
    }

    close(fd);
    free(buffer);
    return 0;
}
