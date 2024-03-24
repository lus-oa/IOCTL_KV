#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// #define MY_IOCTL_CMD _IOWR('M', 1, char *)
#define CMD_IOC_MAGIC 'a'
#define WRITE_IOCTL_CMD _IOW(CMD_IOC_MAGIC, 1, char *) // Write Buffer Address to Linux Kernel
#define READ_IOCTL_CMD _IOR(CMD_IOC_MAGIC, 2, char *)  // Read data from kernel to buffer

// int PUT(int64_t key,)

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

struct header
{
    uint32_t total_length; // total_length 放在最前面
    uint64_t key;
    uint8_t num_segments;
};

struct data
{
    uint32_t length; // length 放在最前面
    uint8_t seq_num;
    char data[1024 * 1000 * 2 - sizeof(uint32_t) - sizeof(uint8_t)]; // 去掉头部分还剩 2047995 Byte
};

void print_header(struct header *hdr)
{
    printf("===========Header Information===========\n");
    uint32_t total_length = hdr->total_length & ~(1 << 31);
    printf("Header Information:\n");
    printf("Total Length: %.2f KB, %u Byte\n", (float)total_length / 1024, total_length);
    printf("Key: 0x%lX\n", hdr->key);
    printf("Number of Segments: %hhu\n", hdr->num_segments);
}

void print_data(struct data *data_ptr)
{
    printf("===========Data Information===========\n");
    printf("Length: %.2f KB, %u Byte\n", (float)data_ptr->length / 1024, data_ptr->length);
    printf("Sequence Number: %hhu\n", data_ptr->seq_num);
    printf("Data: first data:%c ,last data:%c\n", data_ptr->data[0], data_ptr->data[data_ptr->length - 1]);
}
int main(int argc, char *argv[])
{
    char command[5];      // 存储命令（put/get）
    char file_path[1024]; // 存储文件路径
    uint64_t key;

    // 接收三个参数
    int num_scanned = scanf("%4s %llX %1023s", command, &key, file_path);

    // 检查是否成功接收了三个参数
    if (num_scanned != 3)
    {
        printf("Usage: put/get [key] [file_name]\n");
        return 1; // 返回非零值表示程序异常退出
    }

    // 判断命令是 put 还是 get
    if (strcmp(command, "put") == 0)
    {
        printf("Command: put\n");
    }
    else if (strcmp(command, "get") == 0)
    {
        printf("Command: get\n");
    }
    else
    {
        printf("Invalid command: %s\n", command);
        return 1; // 返回非零值表示程序异常退出
    }

    // 获取参数值
    // uint64_t key = strtoull(argv[1], NULL, 16); // 将第一个参数解析为 64 位无符号整数
    // char *file_path = argv[2];                  // 第二个参数为文件名

    int fd;
    char *buffer, *databuf, *ack_buf;
    int max_data = 1024 * 1000 * 2;
    size_t buffer_size = 1024 * 1000 * 2 * 32;
    size_t ack_size = 7;
    buffer = malloc(buffer_size);
    ack_buf = malloc(ack_size);
    struct header *hdr = (struct header *)buffer;

    // 打开文件
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    uint32_t total_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Total Length: %.2f KB, %u Byte\n", (float)total_length / 1024, total_length);
    // 计算需要分成多少个段，向上取整
    uint8_t num_segments = (total_length + max_data - 1) / max_data;

    // 填充头部信息
    hdr->total_length = total_length | (1 << 31); // 将 total_length 的最高位设置为 1，表示这是一个头部
    hdr->key = key;                               // 这里填写你自己指定的KEY值
    hdr->num_segments = num_segments;
    print_header(hdr);

    // 循环读取文件并发送头部信息
    fd = open("/dev/mychardev", O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open device");
        fclose(file);
        return -1;
    }
    if (ioctl(fd, WRITE_IOCTL_CMD, buffer) == -1) // 发送头部信息
    {
        perror("ioctl failed");
        close(fd);
        fclose(file);
        return -1;
    }

    time_statistic();
    // 循环读取文件并发送数据
    databuf = (char *)malloc(buffer_size); // 将数据路径的buf设置成文件大小

    // 读取文件内容到缓冲区
    size_t bytes_left = total_length;
    int cnt = (int)(total_length + buffer_size - 1) / buffer_size;
    while (cnt--)
    {
        if (bytes_left >= buffer_size)
        {
            if (fread(databuf, 1, buffer_size, file) <= 0)
            {
                fprintf(stderr, "读取文件失败\n");
                fclose(file);
                free(databuf);
                return 1;
            }
            // printf("bytes_read: %u\n", bytes_read);
            printf("first data: %c,last data:%c\n", databuf[0], databuf[buffer_size - 1]);
            //     // 发送数据
            if (ioctl(fd, WRITE_IOCTL_CMD, databuf) == -1)
            {
                perror("ioctl failed");
                close(fd);
                fclose(file);
                return -1;
            }
            bytes_left -= buffer_size;
        }
        else
        {
            if (fread(databuf, 1, bytes_left, file) <= 0)
            {
                fprintf(stderr, "读取文件失败\n");
                fclose(file);
                free(databuf);
                return 1;
            }
            // printf("bytes_read: %u\n", bytes_read);
            printf("first data: %c,last data:%c\n", databuf[0], databuf[bytes_left - 1]);
            //     // 发送数据
            if (ioctl(fd, WRITE_IOCTL_CMD, databuf) == -1)
            {
                perror("ioctl failed");
                close(fd);
                fclose(file);
                return -1;
            }
        }
    }

    if (ioctl(fd, READ_IOCTL_CMD, ack_buf) < 0)
    { // After Write successfully, then Read
        perror("ioctl failed");
        close(fd);
        free(ack_buf);
        return 1;
    }

    // 使用strcmp函数来比较ack_buf是否等于"success"
    if (strcmp(ack_buf, "success") == 0)
    {
        printf("WRITE successfully\n");
    }
    else
    {
        printf("WRITE failed\n");
    }

    //     seq_num++;
    // }

    // 关闭文件和设备
    fclose(file);
    close(fd);

    return 0;
}
