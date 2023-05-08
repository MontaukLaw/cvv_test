#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"

#include <opencv2/videoio/videoio.hpp>
#include <opencv2/video.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "rknn_api.h"
#include "postprocess.h"
#include "queue.hpp"
#include "rga.h"

#include "RgaUtils.h"
#include "im2d_type.h"
#include "im2d.h"
#include <dlfcn.h>
#include <string.h>

#define _BASETSD_H
#define PERF_WITH_POST 1
#define VIDEO_HEIGHT 800
#define VIDEO_WIDTH 1280

using namespace std;
using namespace cv;

/*-------------------------------------------
                  Functions
-------------------------------------------*/
static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz);
static double __get_us(struct timeval t);
static unsigned char *load_model(const char *filename, int *model_size);
static void dump_tensor_attr(rknn_tensor_attr *attr);
static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz);
/*-------------------------------------------
                  Variables
-------------------------------------------*/

// conf 跟nms, 默认为0.45, 0.25
const float nms_threshold = NMS_THRESH;
const float box_conf_threshold = BOX_THRESH;

Queue<cv::Mat *> _idleimgbuf;
Queue<cv::Mat *> _imgdata;

int init_model_file(rknn_context *ctx, char *model_name, unsigned char *model_data)
{
    int ret;
    int model_data_size = 0;
    // Step 1: 读取模型文件
    model_data = load_model(model_name, &model_data_size);

    // 使用模型数据初始化rknn_context
    ret = rknn_init(ctx, model_data, model_data_size, 0, NULL);
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }

    // Step 2: 获取rknn的版本, 理论上应该是1.4.0
    rknn_sdk_version version;
    ret = rknn_query(*ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    return ret;
}

int main(int argc, char **argv)
{

    int status = 0;
    // 模型名称
    char *model_name = "./model/yolov5s-640-640.rknn";
    unsigned char *model_data = nullptr;
    rknn_context ctx;
    size_t actual_size = 0;
    int img_width = VIDEO_WIDTH;
    int img_height = VIDEO_HEIGHT;
    int img_channel = 3;

    struct timeval start_time, stop_time;
    int ret;

    // 初始化RGA会话,后面用于缩放图像
    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect src_rect;
    im_rect dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    if (argc != 2)
    {
        printf("Usage: %s <rknn model> \n", argv[0]);
        return -1;
    }
    model_name = (char *)argv[1];

    // 打开/dev/video22
    // VideoCapture cap(22);
    VideoCapture cap("/dev/video40");
    // VideoCapture cap("./Tennis1080p.h264");

    // 检查是否能打开设备
    if (!cap.isOpened())
    {
        cout << "Error opening video stream or file" << endl;
        return -1;
    }
    // 设置取出的画面的长宽
    cap.set(3, VIDEO_WIDTH);
    cap.set(4, VIDEO_HEIGHT);

    // 加载网络
    printf("Loading mode...\n");
    ret = init_model_file(&ctx, model_name, model_data);
    if (ret < 0)
    {
        return -1;
    }

    // Step 3: 获取模型的输入输出通道数
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0)
    {
        printf("rknn_init error ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    // Step 4: 获取输入的张量属性
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++)
    {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0)
        {
            printf("rknn_init error ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    // Step 5: 获取输出的张量属性
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++)
    {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        dump_tensor_attr(&(output_attrs[i]));
    }

    // Step 6: 对model格式进行判断
    int channel = 3;
    int model_width = 0;
    int model_height = 0;
    // 这里的判断是因为模型的输入格式有两种, 一种是NCHW, 一种是NHWC
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW)
    {
        printf("model is NCHW input fmt\n");
        channel = input_attrs[0].dims[1];
        model_height = input_attrs[0].dims[2];
        model_width = input_attrs[0].dims[3];
    }
    else
    {
        printf("model is NHWC input fmt\n");
        model_height = input_attrs[0].dims[1];
        model_width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }

    printf("model input height=%d, width=%d, channel=%d\n", model_height, model_width, channel);
    // 模型要求的输入宽高为640*640

    // 准备好输入数据
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = model_width * model_height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].pass_through = 0;

    void *resize_buf = nullptr;
    Mat frame;
    resize_buf = malloc(model_height * model_width * channel);

    while (1)
    {
        // Capture frame-by-frame
        cap >> frame;

        // If the frame is empty, break immediately
        if (frame.empty())
            break;

        // 使用RGA硬件方式修改frame尺寸
        printf("resize with RGA!\n");
        memset(resize_buf, 0x00, model_height * model_width * channel);

        src = wrapbuffer_virtualaddr((void *)frame.data, img_width, img_height, RK_FORMAT_RGB_888);
        dst = wrapbuffer_virtualaddr((void *)resize_buf, model_width, model_height, RK_FORMAT_RGB_888);
        ret = imcheck(src, dst, src_rect, dst_rect);
        if (IM_STATUS_NOERROR != ret)
        {
            printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
            return -1;
        }
        // IM_STATUS STATUS = imresize(src, dst);
        imresize_t(src, dst, 0, 0, 1, 1);

        // for debug
        // cv::Mat resize_img(cv::Size(model_width, model_height), CV_8UC3, resize_buf);
        // cv::imwrite("resize_input.jpg", resize_img);

        inputs[0].buf = resize_buf;

        gettimeofday(&start_time, NULL);
        rknn_inputs_set(ctx, io_num.n_input, inputs);

        rknn_output outputs[io_num.n_output];
        memset(outputs, 0, sizeof(outputs));
        for (int i = 0; i < io_num.n_output; i++)
        {
            outputs[i].want_float = 0;
        }

        ret = rknn_run(ctx, NULL);
        ret = rknn_outputs_get(ctx, io_num.n_output, outputs, NULL);
        // gettimeofday(&stop_time, NULL);
        // printf("once run use %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000);

        // post process
        float scale_w = (float)model_width / img_width;
        float scale_h = (float)model_height / img_height;

        detect_result_group_t detect_result_group;
        std::vector<float> out_scales;
        std::vector<int32_t> out_zps;
        for (int i = 0; i < io_num.n_output; ++i)
        {
            out_scales.push_back(output_attrs[i].scale);
            out_zps.push_back(output_attrs[i].zp);
        }
        post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf, model_height, model_width,
                     box_conf_threshold, nms_threshold, scale_w, scale_h, out_zps, out_scales, &detect_result_group);

        // 画框框
        char text[256];
        for (int i = 0; i < detect_result_group.count; i++)
        {
            detect_result_t *det_result = &(detect_result_group.results[i]);
            sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
            printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
                   det_result->box.right, det_result->box.bottom, det_result->prop);
            int x1 = det_result->box.left;
            int y1 = det_result->box.top;
            int x2 = det_result->box.right;
            int y2 = det_result->box.bottom;
            // 画框
            rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(255, 0, 0, 255), 2);
            // 红色字体
            putText(frame, text, cv::Point(x1, y1 + 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255));
        }

        // 使用cv的imshow显示
        imshow("Frame", frame);
        ret = rknn_outputs_release(ctx, io_num.n_output, outputs);

        gettimeofday(&stop_time, NULL);
        printf("cost %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000.0);

        // Press  ESC on keyboard to exit
        char c = (char)waitKey(25);
        if (c == 27)
        {
            break;
        }
    }

    deinitPostProcess();

    // release
    ret = rknn_destroy(ctx);

    if (model_data)
    {
        free(model_data);
    }

    if (resize_buf)
    {
        free(resize_buf);
    }

    // When everything done, release the video capture object
    cap.release();

    // Closes all the frames
    destroyAllWindows();

    return 0;
}

static double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

// 读取模型文件
static unsigned char *load_model(const char *filename, int *model_size)
{
    FILE *fp;
    unsigned char *data;

    fp = fopen(filename, "rb");
    if (NULL == fp)
    {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}

static void dump_tensor_attr(rknn_tensor_attr *attr)
{
    printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

static unsigned char *load_data(FILE *fp, size_t ofst, size_t sz)
{
    unsigned char *data;
    int ret;

    data = NULL;

    if (NULL == fp)
    {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0)
    {
        printf("blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char *)malloc(sz);
    if (data == NULL)
    {
        printf("buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

// 保存模型文件, 没有用到
static int saveFloat(const char *file_name, float *output, int element_size)
{
    FILE *fp;
    fp = fopen(file_name, "w");
    for (int i = 0; i < element_size; i++)
    {
        fprintf(fp, "%.6f\n", output[i]);
    }
    fclose(fp);
    return 0;
}
