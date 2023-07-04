#include <linux/videodev2.h>
#include <libv4l2.h>
#include <iostream>
#include <fcntl.h>

int main()
{
    // 打开视频设备
    int fd = v4l2_open("/dev/video22", O_RDWR);
    if (fd < 0)
    {
        std::cerr << "无法打开视频设备" << std::endl;
        return -1;
    }

    // 获取设备信息
    struct v4l2_capability cap;
    if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        std::cerr << "无法获取设备信息" << std::endl;
        v4l2_close(fd);
        return -1;
    }

    // 打印设备信息
    std::cout << "驱动程序: " << cap.driver << std::endl;
    std::cout << "设备名称: " << cap.card << std::endl;
    std::cout << "设备版本: " << (cap.version >> 16) << "." << ((cap.version >> 8) & 0xFF) << "." << (cap.version & 0xFF) << std::endl;

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 320;
    fmt.fmt.pix.height = 240;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt))
    {
        perror("Setting Pixel Format");
        return 1;
    }

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == v4l2_ioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (-1 == v4l2_ioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Querying Buffer");
        return 1;
    }

    if (-1 == v4l2_ioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        perror("Start Capture");
        return 1;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r)
    {
        perror("Waiting for Frame");
        return 1;
    }

    if (-1 == v4l2_ioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Frame");
        return 1;
    }

    // buffer = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    // 关闭视频设备
    v4l2_close(fd);

    return 0;
}
