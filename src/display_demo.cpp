#include "drm_display.h"
// static drmModeModeInfoPtr m;
// static struct sp_crtc *cr,
int main__()
{
    static sp_dev *mDev;
    static sp_plane *mTestPlane;
    static sp_plane **mPlanes;
    static sp_crtc *mCrtc;
    int ret = 0;
    int i = 0;
    uint32_t handles[4], pitches[4], offsets[4];

    cxx_log("init desplay\n");
    // 创建显示设备
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
        return -10;
    }

    // 初始化屏幕
    ret = initialize_screens(mDev);
    if (ret != 0)
    {
        cxx_log("failed to exec initialize_screens.\n");
        return -11;
    }
    mPlanes = (sp_plane **)calloc(mDev->num_planes, sizeof(*mPlanes));
    if (!mPlanes)
    {
        cxx_log("failed to calloc mPlanes.\n");
        return -12;
    }

    for (i = 0; i < mDev->num_crtcs; i++)
    {
        drmModeCrtcPtr tCrtc = (&mDev->crtcs[i])->crtc;
        uint32_t id = tCrtc->crtc_id;
        cxx_log("mDev->crtc[%d]->crtc_d:%d\n", i, id);
        // lcd-0的crtc
        if (id = LCD_0_CRTC_ID)
        {
            mCrtc = &mDev->crtcs[i];
        }
    }

    cxx_log("mCrtc->num_planes:%d\n", mCrtc->num_planes);
    for (i = 0; i < mCrtc->num_planes; i++)
    {
        mPlanes[i] = get_sp_plane(mDev, mCrtc);
        uint32_t plane_id = mPlanes[i]->plane->plane_id;
        cxx_log("mTestPlane->plane->plane_id:%d\n", plane_id);
        // lcd-0的plane
        if (plane_id == LCD_0_PLANE_ID)
        {
            mTestPlane = mPlanes[i];
        }
    }

    if (!mTestPlane)
    {
        cxx_log("failed to get mTestPlane.\n");
        return -13;
    }

    sp_bo *bo;
    bo = (struct sp_bo *)calloc(1, sizeof(struct sp_bo));
    if (!bo)
    {
        cxx_log("failed to calloc bo.\n");
        return -2;
    }
    bo->dev = mDev;
    bo->width = 800;
    bo->height = 1280;
    bo->depth = 16;
    bo->bpp = 32;

    bo->format = DRM_FORMAT_NV12;
    bo->flags = 0;

    add_fb_sp_bo(bo, DRM_FORMAT_NV12);

    // uint32_t plane_id = 197;
    // uint32_t crtc_id = 184;
    uint16_t hdisplay = 800;
    uint16_t vdisplay = 1280;

    cxx_log("plane id:%d \n", mTestPlane->plane->plane_id);
    cxx_log("crtc_id :%d \n", mCrtc->crtc->crtc_id);
    cxx_log("crtc h:%d v:%d\n", mCrtc->crtc->mode.hdisplay,
            mCrtc->crtc->mode.vdisplay);
    ret = drmModeSetPlane(mDev->fd, mTestPlane->plane->plane_id, mCrtc->crtc->crtc_id, bo->fb_id,
                          0, 0, 0, mCrtc->crtc->mode.hdisplay, mCrtc->crtc->mode.vdisplay, 0, 0,
                          bo->width << 16, bo->height << 16);

    fill_bo(bo, 128, 0xff, 0xff, 0x00);

    getchar();

    return 0;
}

static drmModeConnectorPtr connectorPtr = nullptr;
static drmModeEncoderPtr encoderPtr = nullptr;
static sp_dev *mDev;
static struct sp_crtc *cr;
static drmModeModeInfoPtr mPtr;
static uint32_t crtcId = 0;

static void get_connector(uint8_t outpuDevice)
{
    int i, j = 0;
    int ret = 0;

    // connectorPtr, encoderPtr赋值
    // 打印connector的名字
    // connector_type: DRM_MODE_CONNECTOR_DSI : 16
    // connector_type: DRM_MODE_CONNECTOR_HDMIA : 11
    for (j = 0; j < mDev->num_connectors; j++)
    {
        // name 是分辨率信息
        // printf("connector name:%s\n", mDev->connectors[j]->modes->name);
        // printf("connector_type:%d\n", mDev->connectors[j]->connector_type);
        // printf("connector_type_id:%d\n", mDev->connectors[j]->connector_type_id);
        printf("connector status:%d\n", mDev->connectors[j]->connection);
        // 对应不同的输出设备, 指定不同的connector跟encoder
        if (outpuDevice == OUTPUT_DEVICE_LCD)
        {
            if (mDev->connectors[j]->connector_type == DRM_MODE_CONNECTOR_DSI &&
                mDev->connectors[j]->connection == DRM_MODE_CONNECTED)
            {
                connectorPtr = mDev->connectors[j];
            }
        }
        else if (outpuDevice == OUTPUT_DEVICE_HDMI &&
                 mDev->connectors[j]->connection == DRM_MODE_CONNECTED)
        {
            if (mDev->connectors[j]->connector_type == DRM_MODE_CONNECTOR_HDMIA)
            {
                connectorPtr = mDev->connectors[j];
            }
        }
    }
}

static void get_encoder(uint8_t outpuDevice)
{
    int i;
    for (i = 0; i < mDev->num_encoders; i++)
    {
        if (outpuDevice == OUTPUT_DEVICE_LCD)
        {
            if (mDev->encoders[i]->encoder_type == DRM_MODE_ENCODER_DSI)
            {
                encoderPtr = mDev->encoders[i];
                crtcId = encoderPtr->crtc_id;
            }
        }
        else if (outpuDevice == OUTPUT_DEVICE_HDMI)
        {
            if (mDev->encoders[i]->encoder_type == DRM_MODE_ENCODER_TMDS)
            {
                encoderPtr = mDev->encoders[i];
                crtcId = encoderPtr->crtc_id;
            }
        }
    }
}

static void get_crtc()
{
    int j;

    for (j = 0; j < mDev->num_crtcs; j++)
    {

        // printf("encoderPtr->crtc_id:%d\n", mDev->crtcs[j].crtc->crtc_id);
        // printf("mode_valid:%d\n", mDev->crtcs[j].crtc->mode_valid);
        // printf("mode_name:%s\n", mDev->crtcs[j].crtc->mode.name);
        if (mDev->crtcs[j].crtc->crtc_id == crtcId)
        {
            cr = &mDev->crtcs[j];
        }
    }
}

// 这里是将mdev的信息传递给bo, 对bo进行设置
int init_screen(uint8_t outpuDevice)
{
    int ret = 0;
    // 获取connector
    get_connector(outpuDevice);
    if (!connectorPtr)
    {
        cxx_log("failed to get connector or encoder.\n");
        return -1;
    }

    cxx_log("connector id:%d\n", connectorPtr->connector_id);

    // 获取encoder
    get_encoder(outpuDevice);
    if (!encoderPtr)
    {
        cxx_log("failed to get encoder.\n");
        return -2;
    }

    cxx_log("encoder id:%d\n", encoderPtr->encoder_id);
    cxx_log("crtc id:%d\n", crtcId);

    // 获取一下显示分辨率之类
    mPtr = &connectorPtr->modes[0];

    // 把connector的encoder id赋值为encoder的id
    connectorPtr->encoder_id = encoderPtr->encoder_id;
    // 获取crtc
    get_crtc();
    if (!cr)
    {
        cxx_log("failed to get crtc.\n");
        return -3;
    }
    // 打印一下所有信息
    cxx_log("connectord id:%d encoder id:%d crtc_id :%d\n", connectorPtr->connector_id, encoderPtr->encoder_id, encoderPtr->crtc_id);
    cxx_log("m->hdisplay: %d m->vdisplay: %d\n", mPtr->hdisplay, mPtr->vdisplay);
    if (cr->scanout)
    {
        printf("crtc already in use\n");
        return -4;
    }

    // allset
    // 获取bo, 只需要输入分辨率即可.
    cr->scanout = create_sp_bo(mDev, mPtr->hdisplay, mPtr->vdisplay, 24, 32, DRM_FORMAT_XRGB8888, 0);
    if (!cr->scanout)
    {
        printf("failed to create new scanout bo\n");
        return -5;
    }

    printf("fill init color\n");

    fill_bo(cr->scanout, 0xff, 0xff, 0x0, 0x0);

    ret = drmModeSetCrtc(mDev->fd, encoderPtr->crtc_id, cr->scanout->fb_id, 0, 0, &connectorPtr->connector_id, 1, mPtr);
    if (ret)
    {
        printf("failed to set crtc mode ret=%d\n", ret);
        return -6;
    }

    cr->crtc = drmModeGetCrtc(mDev->fd, cr->crtc->crtc_id);
    /*
     * Todo:
     * I don't know why crtc mode is empty, just copy PREFERRED mode
     * for it.
     */
    memcpy(&cr->crtc->mode, mPtr, sizeof(*mPtr));
    return 0;
}

int init_screen__(uint8_t outpuDevice)
{
    int j = 0;
    int ret = 0;
    // step 1: 打印mDev的所有信息:
    printf("mDev->num_connectors:%d\n", mDev->num_connectors); // connector 有2个
    printf("mDev->num_encoders:%d\n", mDev->num_encoders);     // encoder   有3个
    printf("mDev->num_crtcs:%d\n", mDev->num_crtcs);           // crtc      有4个
    printf("mDev->num_planes:%d\n", mDev->num_planes);         // plane     有8个

    if (outpuDevice = OUTPUT_DEVICE_LCD)
    {
        connectorPtr = mDev->connectors[1];
        encoderPtr = mDev->encoders[0];
    }
    else
    {
        connectorPtr = mDev->connectors[0];
        encoderPtr = mDev->encoders[0];
    }

    mPtr = &connectorPtr->modes[0];

    // 打印encoderid
    for (j = 0; j < mDev->num_encoders; j++)
    {
        printf("encoder id:%d\n", mDev->encoders[j]->encoder_id);
    }

    connectorPtr->encoder_id = encoderPtr->encoder_id;

    for (j = 0; j < mDev->num_encoders; j++)
    {
        encoderPtr = mDev->encoders[j];
        if (encoderPtr->crtc_id == LCD_0_CRTC_ID)
        {
            cr = &mDev->crtcs[j];
            // e->crtc_id = cr->crtc->crtc_id;
            break;
        }
    }

    // 获取了指定的connect id 184
    printf("encoder id:%d crtc_id :%d\n", encoderPtr->encoder_id, encoderPtr->crtc_id);

    if (cr->scanout)
    {
        printf("crtc already in use\n");
        return -1;
    }
    printf("create_sp_bo\n");
    printf("m->hdisplay%d m->vdisplay:%d\n", mPtr->hdisplay, mPtr->vdisplay);

    // allset
    // 获取bo, 只需要输入分辨率即可.
    cr->scanout = create_sp_bo(mDev, mPtr->hdisplay, mPtr->vdisplay, 24, 32, DRM_FORMAT_XRGB8888, 0);
    if (!cr->scanout)
    {
        printf("failed to create new scanout bo\n");
        return -2;
    }

    printf("fill_bo\n");

    fill_bo(cr->scanout, 0xff, 0xff, 0x0, 0x0);

    ret = drmModeSetCrtc(mDev->fd, encoderPtr->crtc_id, cr->scanout->fb_id, 0, 0, &connectorPtr->connector_id, 1, mPtr);
    if (ret)
    {
        printf("failed to set crtc mode ret=%d\n", ret);
        return -3;
    }
    cr->crtc = drmModeGetCrtc(mDev->fd, cr->crtc->crtc_id);
    /*
     * Todo:
     * I don't know why crtc mode is empty, just copy PREFERRED mode
     * for it.
     */
    memcpy(&cr->crtc->mode, mPtr, sizeof(*mPtr));

#if 0
    getchar();
    fill_bo(cr->scanout, 0xff, 0x00, 0xff, 0x0);
    memcpy(&cr->crtc->mode, m, sizeof(*m));

    getchar();
    fill_bo(cr->scanout, 0xff, 0x00, 0x0, 0xff);
    memcpy(&cr->crtc->mode, m, sizeof(*m));

    getchar();
    fill_bo(cr->scanout, 0xff, 0xff, 0xff, 0x0);
    memcpy(&cr->crtc->mode, m, sizeof(*m));

    getchar();
#endif
    return 0;
}

void fill_bo_data(uint8_t *data)
{
    uint32_t colIdx = 0;
    uint32_t rowIdx = 0;
    // uint32_t i, j, xmax = SCREEN_WIDTH, ymax = SCREEN_HEIGHT;
    uint8_t *dataPtr = data;
    for (rowIdx = 0; rowIdx < SCREEN_HEIGHT; rowIdx++)
    {
        uint8_t *rowPtr = (uint8_t *)cr->scanout->map_addr + rowIdx * cr->scanout->pitch;
        for (colIdx = 0; colIdx < SCREEN_WIDTH; colIdx++)
        {
            uint8_t *pixel = rowPtr + colIdx * 4;
            pixel[0] = *dataPtr;
            dataPtr++;
            pixel[1] = *dataPtr;
            dataPtr++;
            pixel[2] = *dataPtr;
            dataPtr++;
            pixel[3] = 0xff;
        }
    }
}

// 需要切去屏幕右边的部分, 横置的时候就是上面的部分.
void draw_screen_rgb_960(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != VIDEO_HEIGHT * VIDEO_WIDTH * 3)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 4:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }
    uint32_t colIdx = 0;
    uint32_t rowIdx = 0;
    uint8_t *dataPtr = data;
    for (rowIdx = 0; rowIdx < SCREEN_HEIGHT; rowIdx++)
    {
        uint8_t *rowPtr = (uint8_t *)cr->scanout->map_addr + rowIdx * cr->scanout->pitch;
        for (colIdx = 0; colIdx < SCREEN_WIDTH; colIdx++)
        {
            uint8_t *pixel = rowPtr + colIdx * 4;
            pixel[0] = *dataPtr;
            dataPtr++;
            pixel[1] = *dataPtr;
            dataPtr++;
            pixel[2] = *dataPtr;
            dataPtr++;
            pixel[3] = 0xff;
        }
        dataPtr = dataPtr + (VIDEO_HEIGHT - SCREEN_WIDTH) * 3;
    }
}

// 削掉了后面的一部分数据, 约1440-1280, 160行
void draw_screen_rgb_1440(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != VIDEO_HEIGHT * VIDEO_WIDTH * 3)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 4:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }
    uint32_t colIdx = 0;
    uint32_t rowIdx = 0;
    // uint32_t i, j, xmax = SCREEN_WIDTH, ymax = SCREEN_HEIGHT;
    uint8_t *dataPtr = data;
    uint32_t rowCorpBothSide = CORP_HEIGHT / 2;
    for (rowIdx = 0; rowIdx < SCREEN_HEIGHT; rowIdx++)
    // for (rowIdx = rowCorpBothSide; rowIdx < SCREEN_HEIGHT + rowCorpBothSide; rowIdx++)
    {
        uint8_t *rowPtr = (uint8_t *)cr->scanout->map_addr + rowIdx * cr->scanout->pitch;
        for (colIdx = 0; colIdx < SCREEN_WIDTH; colIdx++)
        {
            uint8_t *pixel = rowPtr + colIdx * 4;
            pixel[0] = *dataPtr;
            dataPtr++;
            pixel[1] = *dataPtr;
            dataPtr++;
            pixel[2] = *dataPtr;
            dataPtr++;
            pixel[3] = 0xff;
        }
    }
}

void draw_screen_rgb_888(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != SCREEN_WIDTH * SCREEN_HEIGHT * 3)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 4:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }
    fill_bo_data(data);
}

void draw_screen(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != SCREEN_WIDTH * SCREEN_HEIGHT * 4)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 4:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }
    uint32_t colIdx = 0;
    uint32_t rowIdx = 0;
    // uint32_t i, j, xmax = SCREEN_WIDTH, ymax = SCREEN_HEIGHT;
    uint8_t *dataPtr = data;
    for (rowIdx = 0; rowIdx < SCREEN_HEIGHT; rowIdx++)
    {
        uint8_t *rowPtr = (uint8_t *)cr->scanout->map_addr + rowIdx * cr->scanout->pitch;
        for (colIdx = 0; colIdx < SCREEN_WIDTH; colIdx++)
        {
            uint8_t *pixel = rowPtr + colIdx * 4;
            pixel[0] = *dataPtr;
            dataPtr++;
            pixel[1] = *dataPtr;
            dataPtr++;
            pixel[2] = *dataPtr;
            dataPtr++;
            pixel[3] = 0xff;
            dataPtr++;
        }
    }
}

// 刷新过程就是不停的更换bo->map_addr的数据, 一行行的
void draw_test(struct sp_bo *bo, int r, int g, int b)
{
    int x = 0;
    int y = 0;
    uint32_t i, j, xmax = x + 800, ymax = y + 1280;

    if (xmax > bo->width)
        xmax = bo->width;
    if (ymax > bo->height)
        ymax = bo->height;

    ymax = 1;
    for (i = y; i < ymax; i++)
    {
        uint8_t *row = (uint8_t *)bo->map_addr + i * bo->pitch;

        for (j = x; j < xmax; j++)
        {
            uint8_t *pixel = row + j * 4;

            if (bo->format == DRM_FORMAT_ARGB8888 || bo->format == DRM_FORMAT_XRGB8888)
            {
                pixel[0] = b;
                pixel[1] = g;
                pixel[2] = r;
                pixel[3] = 0xff;
            }
            else if (bo->format == DRM_FORMAT_RGBA8888)
            {
                pixel[0] = r;
                pixel[1] = g;
                pixel[2] = b;
                pixel[3] = 0xff;
            }
        }
    }
}

void refresh_screen(int r, int g, int b)
{
    // fill_bo(cr->scanout, 0xff, r, g, b);
    draw_test(cr->scanout, r, g, b);
    // printf("size of *m %ld\n", sizeof(*m));
    // memcpy(&cr->crtc->mode, m, sizeof(*m));
}

// unit test
// 事实证明, 这个lib是可以的
void row_test()
{
    int i = 0;
    // refresh_screen(0x00, 0xff, 0xff);
    uint8_t data[SCREEN_WIDTH * SCREEN_HEIGHT * 4] = {0};
    memset(data, 0x00, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    for (i < 0; i < SCREEN_WIDTH * 4; i++)
    {
        data[i] = 0xff;
    }
    draw_screen(data, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    getchar();

    for (i < 0; i < SCREEN_WIDTH * 4 * 2; i++)
    {
        data[i] = 0xff;
    }
    draw_screen(data, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    getchar();

    for (i < 0; i < SCREEN_WIDTH * 4 * 3; i++)
    {
        data[i] = 0xff;
    }
    draw_screen(data, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    getchar();
}

// 显示logo成功
void read_rgba8888_pic_test()
{
    char *file_name = "/usr/data/out0w720-h1280-rgba8888.bin";

    FILE *mFin = fopen(file_name, "rb");
    if (!mFin)
    {
        cxx_log("failed to open input file %s.\n", file_name);
        return;
    }
    // 建一个4096KB的读写缓存
    uint8_t *file_data_buffer = new uint8_t[SCREEN_WIDTH * SCREEN_HEIGHT * 4];
    if (!file_data_buffer)
    {
        cxx_log("failed to malloc file_data_buffer.\n");
        return;
    }
    int len = 0;
    uint8_t *file_data_ptr = file_data_buffer;
    // len = fread(file_data_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4, 1, mFin);
    while (!feof(mFin)) // 判断文件指针是否已指向文件的末尾
    {
        len += fread(file_data_ptr, 720 * 4, 1, mFin);
        file_data_ptr = file_data_ptr + 720 * 4;
        // 填充一下
        for (int i = 0; i < 80 * 4; i++)
        {
            *file_data_ptr = 0xff;
            file_data_ptr++;
        }
        len += 80 * 4;
        // file_data_buffer += 80*4;
        // draw_screen(file_data_buffer, 4096);
        // len += 4096;
    }

    draw_screen(file_data_buffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    fclose(mFin); // 关闭文件，释放资源
    printf("读取的实际大小为：%d", len);
    // unsigned char edid_data[1024];
    // size = 3686400;
}

void create_display_device()
{
    cxx_log("init desplay\n");
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
    }
}

int main_test()
{
    // static sp_dev *mDev;
    int ret = 0;
    int i = 0;
    // struct sp_crtc *cr = NULL;
    // drmModeModeInfoPtr mPtr = NULL;
    cxx_log("init desplay\n");
    // 创建显示设备
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
        return -10;
    }

    // 初始化屏幕
    // ret = initialize_screens(mDev);
    ret = init_screen(OUTPUT_DEVICE_LCD);
    if (ret != 0)
    {
        cxx_log("failed to exec initialize_screens.\n");
        return -11;
    }
    getchar();
    cxx_log("change\n");
    read_rgba8888_pic_test();
    getchar();
    free_sp_bo(cr->scanout);
    return 0;
}

void release_bo()
{
    free_sp_bo(cr->scanout);
}

// 20230524
// 主要的显示初始化
// 包括创建设备 create_sp_dev
// 初始化屏幕 init_screen
int dis_init()
{
    int ret = 0;
    int i = 0;
    cxx_log("create sp dev\n");
    // 创建显示设备
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
        return -10;
    }

    cxx_log("init_screen\n");

    // 初始化屏幕
    // ret = initialize_screens(mDev);
    ret = init_screen(OUTPUT_DEVICE_LCD);
    // ret = init_screen(OUTPUT_DEVICE_HDMI);
    if (ret != 0)
    {
        cxx_log("failed to exec initialize_screens.\n");
        return -11;
    }
    return 0;
}