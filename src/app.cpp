#include "model_utils.h"
#include "encoder_user_comm.h"

// 是否连接摄像头
#define IF_USING_SCREENS 0

using namespace std;
using namespace cv;

bool ifCam1Connected = false;
bool ifCam2Connected = false;

const float nms_threshold = NMS_THRESH;
const float box_conf_threshold = BOX_THRESH;

extern bool newFrameArrived;

RK_U8 yuvFrameData[CAM1_VIDEO_WIDTH * CAM1_VIDEO_WIDTH * 3 / 2];

// 22是855
// VideoCapture cam1Cap(22, CAP_V4L);
VideoCapture cam1Cap(22, CAP_GSTREAMER);

// 31是850
VideoCapture cam2Cap(31);

static double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

static void test_cam()
{

    if (!cam1Cap.isOpened())
    {
        cout << "Error opening cam 1" << endl;
    }
    else
    {
        ifCam1Connected = true;
    }

    if (!cam2Cap.isOpened())
    {
        cout << "Error opening cam 2" << endl;
    }
    else
    {
        ifCam2Connected = true;
    }
}

static void get_image_test()
{
    Mat frame;

    if (ifCam1Connected)
    {
        cam1Cap >> frame;
        imwrite("cam1.jpg", frame);
    }

    if (ifCam2Connected)
    {
        cam2Cap >> frame;
        imwrite("cam2.jpg", frame);
    }
}

static void resize_frame_to_model(Mat frame, void *model_input_buf, int model_width, int model_height, int channel, int video_input_width, int video_input_height)
{
    rga_buffer_t src;
    rga_buffer_t dst;
    int ret = 0;
    im_rect src_rect;
    im_rect dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    // printf("resize with RGA!\n");
    memset(model_input_buf, 0x00, model_height * model_width * channel);
    // 将图片缩放到模型输入大小
    // src = wrapbuffer_virtualaddr((void *)frame.data, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT, RK_FORMAT_RGB_888);
    src = wrapbuffer_virtualaddr((void *)frame.data, video_input_width, video_input_height, RK_FORMAT_RGB_888);
    dst = wrapbuffer_virtualaddr(model_input_buf, model_width, model_height, RK_FORMAT_RGB_888);
    ret = imcheck(src, dst, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret)
    {
        printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        // return -1;
    }

    imresize_t(src, dst, 0, 0, 1, 1);
}

void get_frame_type(Mat frame)
{
    if (frame.type() == CV_8UC3)
    {
        std::cout << "Color format: BGR" << std::endl;
    }
    else if (frame.type() == CV_8UC4)
    {
        std::cout << "Color format: BGRA" << std::endl;
    }
    else
    {
        std::cout << "Color format: Unknown" << std::endl;
    }
}

void convertBGRtoYUV420SP(const cv::Mat &bgrImage, cv::Mat &yuv420spImage)
{
    int width = bgrImage.cols;
    int height = bgrImage.rows;

    // 分配YUV420SP格式的图像内存
    yuv420spImage.create(height * 3 / 2, width, CV_8UC1);

    // 分离BGR通道
    std::vector<cv::Mat> channels;
    cv::split(bgrImage, channels);

    // 将BGR数据转换为YUV数据
    cv::Mat yChannel = 0.299 * channels[2] + 0.587 * channels[1] + 0.114 * channels[0];
    cv::Mat uChannel, vChannel;
    cv::resize(channels[1], uChannel, cv::Size(width / 2, height / 2), 0, 0, cv::INTER_LINEAR);
    cv::resize(channels[0], vChannel, cv::Size(width / 2, height / 2), 0, 0, cv::INTER_LINEAR);

    // 将Y通道数据拷贝到YUV420SP格式的图像中
    cv::Mat yROI(yuv420spImage, cv::Rect(0, 0, width, height));
    yChannel.copyTo(yROI);

    // 将U和V通道数据交错存储到YUV420SP格式的图像中
    cv::Mat uvROI(yuv420spImage, cv::Rect(0, height, width, height / 2));
    for (int i = 0; i < uChannel.rows; i++)
    {
        for (int j = 0; j < uChannel.cols; j++)
        {
            uchar u = uChannel.at<uchar>(i, j);
            uchar v = vChannel.at<uchar>(i, j);
            uvROI.at<uchar>(i * 2, j * 2) = u;
            uvROI.at<uchar>(i * 2, j * 2 + 1) = v;
            uvROI.at<uchar>(i * 2 + 1, j * 2) = u;
            uvROI.at<uchar>(i * 2 + 1, j * 2 + 1) = v;
        }
    }
}

void convertBGRtoNV12(const cv::Mat &bgrImage, cv::Mat &nv12Image)
{
    int width = bgrImage.cols;
    int height = bgrImage.rows;

    // 分配NV12格式的图像内存
    nv12Image.create(height * 3 / 2, width, CV_8UC1);

    // 分离BGR通道
    std::vector<cv::Mat> channels;
    cv::split(bgrImage, channels);

    // 将BGR数据转换为YUV数据
    cv::Mat yChannel = 0.299 * channels[2] + 0.587 * channels[1] + 0.114 * channels[0];
    cv::Mat uChannel = -0.169 * channels[2] - 0.331 * channels[1] + 0.5 * channels[0] + 128;
    cv::Mat vChannel = 0.5 * channels[2] - 0.419 * channels[1] - 0.081 * channels[0] + 128;

    // 将Y、U和V通道数据拷贝到NV12格式的图像中
    cv::Mat yROI(nv12Image, cv::Rect(0, 0, width, height));
    yChannel.copyTo(yROI);

    cv::Mat uvROI(nv12Image, cv::Rect(0, height, width, height / 2));
    cv::Mat uvChannels[] = {uChannel, vChannel};
    cv::merge(uvChannels, 2, uvROI);
}

void test()
{
    cv::Mat bgr_mat = cv::imread("cam1.jpg", cv::IMREAD_COLOR);

    int height = bgr_mat.rows;
    int width = bgr_mat.cols;

    cv::Mat img_nv12;
    cv::Mat yuv_mat;
    cv::cvtColor(bgr_mat, yuv_mat, cv::COLOR_BGR2YUV_I420);

    uint8_t *yuv = yuv_mat.ptr<uint8_t>();
    img_nv12 = cv::Mat(height * 3 / 2, width, CV_8UC1);
    uint8_t *ynv12 = img_nv12.ptr<uint8_t>();

    int32_t uv_height = height / 2;
    int32_t uv_width = width / 2;

    // copy y data
    int32_t y_size = height * width;
    memcpy(ynv12, yuv, y_size);

    // copy uv data
    uint8_t *nv12 = ynv12 + y_size;
    uint8_t *u_data = yuv + y_size;
    uint8_t *v_data = u_data + uv_height * uv_width;

    for (int32_t i = 0; i < uv_width * uv_height; i++)
    {
        *nv12++ = *u_data++;
        *nv12++ = *v_data++;
    }

    int32_t yuv_size = y_size + 2 * uv_height * uv_width;
    FILE *yuvFd = fopen("1.yuv", "w+");
    fwrite(img_nv12.ptr<uint8_t>(), 1, yuv_size, yuvFd);
    fclose(yuvFd);
}

void save_yuv(cv::Mat &yuv_mat, int height, int width)
{
    cv::Mat img_nv12;
    uint8_t *yuv = yuv_mat.ptr<uint8_t>();
    img_nv12 = cv::Mat(height * 3 / 2, width, CV_8UC1);
    uint8_t *ynv12 = img_nv12.ptr<uint8_t>();

    int32_t uv_height = height / 2;
    int32_t uv_width = width / 2;

    // copy y data
    int32_t y_size = height * width;
    memcpy(ynv12, yuv, y_size);

    // copy uv data
    uint8_t *nv12 = ynv12 + y_size;
    uint8_t *u_data = yuv + y_size;
    uint8_t *v_data = u_data + uv_height * uv_width;

    for (int32_t i = 0; i < uv_width * uv_height; i++)
    {
        *nv12++ = *u_data++;
        *nv12++ = *v_data++;
    }

    int32_t yuv_size = y_size + 2 * uv_height * uv_width;
    FILE *yuvFd = fopen("1.yuv", "w+");
    fwrite(img_nv12.ptr<uint8_t>(), 1, yuv_size, yuvFd);
    fclose(yuvFd);
}

void trans_data_2_yuv_for_encoder(cv::Mat &bgrMat, int width, int height)
{

    cv::Mat yuv_mat;
    cv::cvtColor(bgrMat, yuv_mat, cv::COLOR_BGR2YUV_I420);
    cv::Mat img_nv12;
    uint8_t *yuv = yuv_mat.ptr<uint8_t>();
    img_nv12 = cv::Mat(height * 3 / 2, width, CV_8UC1);
    uint8_t *ynv12 = img_nv12.ptr<uint8_t>();

    int32_t uv_height = height / 2;
    int32_t uv_width = width / 2;

    // copy y data
    int32_t y_size = height * width;
    memcpy(ynv12, yuv, y_size);

    // copy uv data
    uint8_t *nv12 = ynv12 + y_size;
    uint8_t *u_data = yuv + y_size;
    uint8_t *v_data = u_data + uv_height * uv_width;

    for (int32_t i = 0; i < uv_width * uv_height; i++)
    {
        *nv12++ = *u_data++;
        *nv12++ = *v_data++;
    }

    int32_t yuv_size = y_size + 2 * uv_height * uv_width;
    // FILE *yuvFd = fopen("1.yuv", "w+");
    // fwrite(img_nv12.ptr<uint8_t>(), 1, yuv_size, yuvFd);
    // fclose(yuvFd);

    memset((RK_U8 *)yuvFrameData, 0, yuv_size);
    memcpy((RK_U8 *)yuvFrameData, img_nv12.ptr<uint8_t>(), yuv_size);

    newFrameArrived = true;
}

void *thread_func(void *args)
{
    while (1)
    {
        if (!newFrameArrived)
        {
            usleep(1000);
            continue;
        }

        printf("thread_func\n");
        newFrameArrived = false;
    }
}

int main(int argc, char **argv)
{
    // test();
    // return 1;

    struct timeval start_time, stop_time;
    if (argc != 2)
    {
        printf("Usage: %s <rknn model> \n", argv[0]);
        return -1;
    }

    // char *model_name = "./model/yolov5s-640-640.rknn";
    char *model_name = (char *)argv[1];

    Mat frame;
    int ret = 0;
    // rknn上下文句柄
    rknn_context ctx;
    char *model_data = nullptr;
    int channel = 3;
    int model_width = 0;
    int model_height = 0;

    // 初始化RGA会话,后面用于缩放图像
    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect src_rect;
    im_rect dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    // 判断是否连接屏幕
    if (IF_USING_SCREENS)
    {
        double_dis_init();
    }

    // 测试摄像头
    test_cam();

    if (ifCam1Connected == false && ifCam2Connected == false)
    {
        printf("No cam found\n");
        return -1;
    }

    cam1Cap.set(CAP_PROP_FRAME_WIDTH, CAM1_VIDEO_WIDTH);
    cam1Cap.set(CAP_PROP_FRAME_HEIGHT, CAM1_VIDEO_HEIGHT);

    cam2Cap.set(CAP_PROP_FRAME_WIDTH, CAM2_VIDEO_WIDTH);
    cam2Cap.set(CAP_PROP_FRAME_HEIGHT, CAM2_VIDEO_HEIGHT);

    // cam1Cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));
    // cam2Cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G'));

    // cam1Cap.set(CAP_PROP_FPS, 20.0f);
    // cam2Cap.set(CAP_PROP_FPS, 20.0f);

    printf("cam1 fps is:%d\n", (int)cam1Cap.get(CAP_PROP_FPS));
    printf("cam1 CAP_PROP_MODE:%d\n", (int)cam1Cap.get(CAP_PROP_MODE));
    printf("cam1 CAP_PROP_CONVERT_RGB:%d\n", (int)cam1Cap.get(CAP_PROP_CONVERT_RGB));
    printf("cam1 CAP_PROP_CODEC_PIXEL_FORMAT:%d\n", (int)cam1Cap.get(CAP_PROP_CODEC_PIXEL_FORMAT));

    printf("cam2 fps is:%d\n", (int)cam2Cap.get(CAP_PROP_FPS));

    // 获取一帧测试用的图像
    get_image_test();

    printf("Loading mode...\n");
    ret = init_model_file(&ctx, model_name, model_data);
    if (ret < 0)
    {
        return -1;
    }

    rknn_tensor_attr output_attrs[MODLE_OUTPUT_NUM];
    ret = init_model(ctx, &model_width, &model_height, &channel, output_attrs);
    if (ret < 0)
    {
        return -1;
    }

    // 准备好输入数据
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = model_width * model_height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].pass_through = 0;

    void *model_input_buf = malloc(model_height * model_width * channel);
    void *lcd_data_buf = malloc(LCD_DATA_HEIGHT * LCD_DATA_WIDTH * channel);

    int counter = 0;

    rknn_output outputs[MODLE_OUTPUT_NUM];

    // 打开编码器
    start_encode(argv[0]);

    // 初始化rtsp服务
    RtspServer_init();

    // 新建一个线程测试
    // pthread_t thread;
    // pthread_create(&thread, NULL, thread_func, NULL);

    while (1)
    {
        // cam1
        if (ifCam1Connected)
        {
            cam1Cap >> frame;
            if (frame.empty())
                break;

            static int startOnce = 0;
            if (startOnce == 0)
            {

                cv::Mat yuv_mat;
                cv::cvtColor(frame, yuv_mat, cv::COLOR_BGR2YUV_I420);
                // save_yuv(yuv_mat, frame.rows, frame.cols);
            }

#if 0
            // printf("resize with RGA!\n");
            memset(model_input_buf, 0x00, model_height * model_width * channel);
            // 将图片缩放到模型输入大小
            src = wrapbuffer_virtualaddr((void *)frame.data, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT, RK_FORMAT_RGB_888);
            dst = wrapbuffer_virtualaddr(model_input_buf, model_width, model_height, RK_FORMAT_RGB_888);
            ret = imcheck(src, dst, src_rect, dst_rect);
            if (IM_STATUS_NOERROR != ret)
            {
                printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
                return -1;
            }

            imresize_t(src, dst, 0, 0, 1, 1);

#endif

            gettimeofday(&start_time, NULL);

            resize_frame_to_model(frame, model_input_buf, model_width, model_height, channel, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT);

            // 测试了这里的模型输入数据, 没大问题
            // cv::Mat model_img(cv::Size(model_width, model_width), CV_8UC3, model_input_buf);
            // imwrite("model_input_img.jpg", model_img);

            // 将detect数据放入模型的input里面
            inputs[0].buf = model_input_buf;

            // 设置 input
            rknn_inputs_set(ctx, 1, inputs);
            // 准备 outputs
            memset(outputs, 0, sizeof(outputs));
            for (int i = 0; i < MODLE_OUTPUT_NUM; i++)
            {
                outputs[i].want_float = 0;
            }

            // 启动模型
            ret = rknn_run(ctx, NULL);
            if (ret < 0)
            {
                printf("rknn_run fail! ret=%d\n", ret);
                return -1;
            }
            ret = rknn_outputs_get(ctx, MODLE_OUTPUT_NUM, outputs, NULL);
            if (ret < 0)
            {
                printf("rknn_outputs_get fail! ret=%d\n", ret);
                return -1;
            }

            // 后处理, 将3个维度的结果组合起来
            float scale_w = (float)model_width / CAM1_VIDEO_WIDTH;
            float scale_h = (float)model_height / CAM1_VIDEO_HEIGHT;

            // printf("scale_w: %f, scale_h: %f\n", scale_w, scale_h);

            detect_result_group_t detect_result_group;
            std::vector<float> out_scales;
            std::vector<int32_t> out_zps;
            for (int i = 0; i < MODLE_OUTPUT_NUM; ++i)
            {
                out_scales.push_back(output_attrs[i].scale);
                out_zps.push_back(output_attrs[i].zp);
            }

            // printf("start post process\n");

            // 后处理
            post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf, model_height, model_width,
                         box_conf_threshold, nms_threshold, scale_w, scale_h, out_zps, out_scales, &detect_result_group);

            // 画框框
            char text[256];
            for (int i = 0; i < detect_result_group.count; i++)
            {
                detect_result_t *det_result = &(detect_result_group.results[i]);
                sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
                printf("CAM1: %s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
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

            // 将结果转成yuv格式, 准备送去解码.
            trans_data_2_yuv_for_encoder(frame, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT);

            int resize_buf_size = LCD_DATA_WIDTH * LCD_DATA_HEIGHT * channel;
            // memset(rotate_buf, 0x00, 1280 * 800 * channel);
            memset(lcd_data_buf, 0x80, resize_buf_size);

            // 因为屏幕是需要旋转的, 而图像本身是正的, 尝试保存看看.
            cv::Mat img(cv::Size(CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT), CV_8UC3, frame.data);

            // 旋转一下
            src = wrapbuffer_virtualaddr((void *)frame.data, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT, RK_FORMAT_RGB_888);
            dst = wrapbuffer_virtualaddr((void *)lcd_data_buf, LCD_DATA_WIDTH, LCD_DATA_HEIGHT, RK_FORMAT_RGB_888);
            ret = imcheck(src, dst, src_rect, dst_rect);
            if (IM_STATUS_NOERROR != ret)
            {
                printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
                return -1;
            }

            // 为了适配屏幕, 旋转90度
            ret = imrotate(src, dst, IM_HAL_TRANSFORM_ROT_90);
            if (ret == IM_STATUS_SUCCESS)
            {
                // printf("imrotate running success!\n");
            }
            else
            {
                printf("running failed, %s\n", imStrError((IM_STATUS)ret));
                break;
                // release_buf(rga_process);
            }

            // 960
            if (IF_USING_SCREENS)
            {
                draw_lcd_screen_rgb_960((uint8_t *)lcd_data_buf, resize_buf_size);
            }

            // 写文件通过了
            if (counter % 100 == 0)
            {
                printf("output\n");
                // cv::Mat resize_img(cv::Size(1280, 800), CV_8UC3, resize_buf);
                cv::Mat resize_img(cv::Size(LCD_DATA_WIDTH, LCD_DATA_HEIGHT), CV_8UC3, lcd_data_buf);
                // char *filename;
                // sprintf(filename, "resize%d.jpg", counter);
                // imwrite("output.jpg", resize_img);
            }
            counter++;
            // 这个frame是cv的frame，不是rga的frame
            // 使用cv的imshow显示
            // imshow("Frame", frame);
            ret = rknn_outputs_release(ctx, MODLE_OUTPUT_NUM, outputs);

            gettimeofday(&stop_time, NULL);
            printf("cam1 inference cost %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000.0);
        }

        // cam2
        if (ifCam2Connected)
        {
            cam2Cap >> frame;
            if (frame.empty())
                break;

            gettimeofday(&start_time, NULL);
            resize_frame_to_model(frame, model_input_buf, model_width, model_height, channel, CAM2_VIDEO_WIDTH, CAM2_VIDEO_HEIGHT);

            cv::Mat model_img(cv::Size(model_width, model_width), CV_8UC3, model_input_buf);
            imwrite("model_input_img.jpg", model_img);

            // 将detect数据放入模型的input里面
            inputs[0].buf = model_input_buf;

            // 设置 input
            rknn_inputs_set(ctx, 1, inputs);
            // 准备 outputs
            memset(outputs, 0, sizeof(outputs));
            for (int i = 0; i < MODLE_OUTPUT_NUM; i++)
            {
                outputs[i].want_float = 0;
            }

            // 启动模型
            ret = rknn_run(ctx, NULL);
            if (ret < 0)
            {
                printf("rknn_run fail! ret=%d\n", ret);
                return -1;
            }
            ret = rknn_outputs_get(ctx, MODLE_OUTPUT_NUM, outputs, NULL);
            if (ret < 0)
            {
                printf("rknn_outputs_get fail! ret=%d\n", ret);
                return -1;
            }

            // 后处理, 将3个维度的结果组合起来
            float scale_w = (float)model_width / CAM2_VIDEO_WIDTH;
            float scale_h = (float)model_height / CAM2_VIDEO_HEIGHT;

            // printf("scale_w: %f, scale_h: %f\n", scale_w, scale_h);

            detect_result_group_t detect_result_group;
            std::vector<float> out_scales;
            std::vector<int32_t> out_zps;
            for (int i = 0; i < MODLE_OUTPUT_NUM; ++i)
            {
                out_scales.push_back(output_attrs[i].scale);
                out_zps.push_back(output_attrs[i].zp);
            }

            // printf("start post process\n");

            // 后处理
            post_process((int8_t *)outputs[0].buf, (int8_t *)outputs[1].buf, (int8_t *)outputs[2].buf, model_height, model_width,
                         box_conf_threshold, nms_threshold, scale_w, scale_h, out_zps, out_scales, &detect_result_group);

            // 画框框
            char text[256];
            for (int i = 0; i < detect_result_group.count; i++)
            {
                detect_result_t *det_result = &(detect_result_group.results[i]);
                sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
                printf("CAM2: %s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
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

            int resize_buf_size = HDMI_DATA_WIDTH * HDMI_DATA_HEIGHT * channel;
            // memset(rotate_buf, 0x00, 1280 * 800 * channel);
            // memset(lcd_data_buf, 0x80, resize_buf_size);
            if (IF_USING_SCREENS)
            {
                draw_hdmi_screen_rgb(frame.data, resize_buf_size);
            }
            // hdmi_draw_test();

            // cv::Mat output_img(cv::Size(HDMI_DATA_HEIGHT, HDMI_DATA_WIDTH), CV_8UC3, frame.data);
            // imwrite("hdmioutput.jpg", output_img);

            ret = rknn_outputs_release(ctx, MODLE_OUTPUT_NUM, outputs);

            gettimeofday(&stop_time, NULL);
            printf("cam 2 inference cost %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000.0);
        }

        char c = (char)waitKey(1);
        if (c == 'q')
        {
            break;
        }
    }

    if (model_data)
    {
        free(model_data);
    }

    cam2Cap.release();
    cam1Cap.release();

    return 0;
}