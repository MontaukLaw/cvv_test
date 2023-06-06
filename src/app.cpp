#include "model_utils.h"

using namespace std;
using namespace cv;

bool ifCam1Connected = false;
bool ifCam2Connected = false;

const float nms_threshold = NMS_THRESH;
const float box_conf_threshold = BOX_THRESH;

// 22是855
VideoCapture cam1Cap(22);
// 31是850
VideoCapture cam2Cap(31);

static double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

static void test_cam()
{

    if (!cam1Cap.isOpened())
    {
        cout << "Error opening video stream or file" << endl;
    }
    else
    {
        ifCam1Connected = true;
    }

    if (!cam2Cap.isOpened())
    {
        cout << "Error opening video stream or file" << endl;
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

int main(int argc, char **argv)
{
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

    // 待修改
    double_dis_init();

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
    while (1)
    {
        // cam1
        if (ifCam1Connected)
        {
            cam1Cap >> frame;
            if (frame.empty())
                break;

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

            int resize_buf_size = LCD_DATA_WIDTH * LCD_DATA_HEIGHT * channel;
            // memset(rotate_buf, 0x00, 1280 * 800 * channel);
            memset(lcd_data_buf, 0x80, resize_buf_size);

            // 旋转一下
            src = wrapbuffer_virtualaddr((void *)frame.data, CAM1_VIDEO_WIDTH, CAM1_VIDEO_HEIGHT, RK_FORMAT_RGB_888);
            dst = wrapbuffer_virtualaddr((void *)lcd_data_buf, LCD_DATA_WIDTH, LCD_DATA_HEIGHT, RK_FORMAT_RGB_888);
            ret = imcheck(src, dst, src_rect, dst_rect);
            if (IM_STATUS_NOERROR != ret)
            {
                printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
                return -1;
            }

            ret = imrotate(src, dst, IM_HAL_TRANSFORM_ROT_90);
            if (ret == IM_STATUS_SUCCESS)
            {
                printf("imrotate running success!\n");
            }
            else
            {
                printf("running failed, %s\n", imStrError((IM_STATUS)ret));
                break;
                // release_buf(rga_process);
            }

            // 960
            draw_lcd_screen_rgb_960((uint8_t *)lcd_data_buf, resize_buf_size);

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
            printf("cost %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000.0);
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

            draw_hdmi_screen_rgb(frame.data, resize_buf_size);
            // hdmi_draw_test();
           
            // cv::Mat output_img(cv::Size(HDMI_DATA_HEIGHT, HDMI_DATA_WIDTH), CV_8UC3, frame.data);
            // imwrite("hdmioutput.jpg", output_img);

            ret = rknn_outputs_release(ctx, MODLE_OUTPUT_NUM, outputs);

            gettimeofday(&stop_time, NULL);
            printf("cost %f ms\n", (__get_us(stop_time) - __get_us(start_time)) / 1000.0);
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