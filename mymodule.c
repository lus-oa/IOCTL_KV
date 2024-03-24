#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/time.h>

#define DEVICE_NAME "mychardev"
#define WRITE_IOCTL_CMD _IOW('a', 1, char *)
#define READ_IOCTL_CMD _IOR('a', 2, char *)

static int major;
static struct cdev my_cdev;
static struct class *my_class = NULL;
struct timespec64 ts;
struct tm tm;
int flag = 0; // 0:控制路径； 1：数据路径；2：等待用户程序询问读写是否完成
size_t ack_size = 7;
int segnum = 0;
char *ack_buf;

struct header
{
    uint32_t total_length; //
    uint64_t key;
    uint8_t num_segments;
};

struct data
{
    uint32_t length; //
    uint8_t seq_num;
    char data[1024 * 1000 * 2 - sizeof(uint32_t) - sizeof(uint8_t)]; // 去掉头部分还剩 2047995 Byte
};

void gettime(void)
{
    ktime_get_real_ts64(&ts);        // 获取当前时间
    time64_to_tm(ts.tv_sec, 0, &tm); // 将秒数转换为tm结构体
    printk("Current time: %04ld-%02d-%02d %02d:%02d:%02d.%09lu\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    char *rdma_buf;
    // char *ack_buf;
    size_t buffer_size = 1024 * 1000 * 2; // 1 MB buffer
    struct header *hdr;

    rdma_buf = vmalloc(buffer_size);
    if (!rdma_buf)
    {
        printk("Kernel buffer malloc failed");
        return -ENOMEM;
    }

    switch (cmd)
    {
    case WRITE_IOCTL_CMD:
        if (flag == 0) // 控制路径
        {              //

            if (copy_from_user(rdma_buf, (char __user *)arg, buffer_size))
            {
                vfree(rdma_buf);
                return -EFAULT;
            }
            // 判断是否是header
            hdr = (struct header *)rdma_buf;
            if (hdr->total_length & (1 << 31))
            {
                printk(KERN_INFO "Received header:\n");
                printk(KERN_INFO "Total Length: %u\n", hdr->total_length );
                printk(KERN_INFO "Key: 0x%llX\n", hdr->key);
                printk(KERN_INFO "Number of Segments: %hhu\n", hdr->num_segments);
            }
            segnum = hdr->num_segments;
            flag = 1; // 将flag置1，下次就进入数据路径
        }
        else if (flag == 1)
        {
            while (segnum--)
            {
                if (copy_from_user(rdma_buf, (char __user *)arg, buffer_size))
                {
                    vfree(rdma_buf);
                    return -EFAULT;
                }
                arg += 1024 * 1000 * 2;
                printk(KERN_INFO "Data: first data:%c ,last data:%c\n", rdma_buf[0], rdma_buf[buffer_size - 1]);
            }
            flag = 2; // 等待用户程序询问读写是否完成
            printk(KERN_INFO "received data\n");
            strcpy(ack_buf, "success");
            printk(KERN_INFO "ack_buf: %s\n", ack_buf);
        }
        gettime();
        break;
    case READ_IOCTL_CMD:
        if (flag == 2) // 返回ack
        {
            printk(KERN_INFO "into ack\n");
            if (copy_to_user((char __user *)arg, ack_buf, ack_size))
            {
                printk(KERN_INFO "ACK failed");
                vfree(ack_buf);
                return -EFAULT;
            }
            flag = 0;
        }
        else
        {
            printk("Successfully Get the read cmd from user");
            rdma_buf[0] = 'z';
            rdma_buf[buffer_size / 2] = 'z';
            rdma_buf[buffer_size - 1] = 'z';
            if (copy_to_user((char __user *)arg, rdma_buf, buffer_size))
            {
                vfree(rdma_buf);
                return -EFAULT;
            }
        }
        break;

    default:
        vfree(rdma_buf);
        return -EINVAL;
    }

    vfree(rdma_buf);
    return 0;
}

static struct file_operations my_fops = {
    .unlocked_ioctl = my_ioctl,
    .owner = THIS_MODULE,
};

static int __init my_module_init(void)
{
    ack_buf = kmalloc(ack_size, GFP_KERNEL);
    major = register_chrdev(0, DEVICE_NAME, &my_fops);
    if (major < 0)
    {
        printk("Failed to register character device\n");
        return major;
    }

    my_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(my_class))
    {
        unregister_chrdev(major, DEVICE_NAME);
        printk("Failed to create class\n");
        return PTR_ERR(my_class);
    }

    if (!device_create(my_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME))
    {
        class_destroy(my_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk("Failed to create device\n");
        return -1;
    }

    pr_info("Module loaded\n");
    return 0;
}

static void __exit my_module_exit(void)
{
    device_destroy(my_class, MKDEV(major, 0));
    class_destroy(my_class);
    unregister_chrdev(major, DEVICE_NAME);
    printk("Module unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
