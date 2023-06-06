#include "display.h"

static drmModeConnectorPtr c = nullptr;
static drmModeEncoderPtr e = nullptr;
static sp_dev *mDev = nullptr;
static struct sp_crtc *cr = nullptr;
static drmModeModeInfoPtr m = nullptr;

int create_display_device()
{
    cxx_log("init desplay\n");
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
    }

    int j = 0;
    int ret = 0;
    c = mDev->connectors[0];
    m = &c->modes[0];
    e = mDev->encoders[0];

    c->encoder_id = e->encoder_id;

    for (j = 0; j < mDev->num_encoders; j++)
    {
        e = mDev->encoders[j];
        if (e->crtc_id == LCD_0_CRTC_ID)
        {
            cr = &mDev->crtcs[j];
            // e->crtc_id = cr->crtc->crtc_id;
            break;
        }
    }

    printf("encoder id:%d crtc_id :%d\n", e->encoder_id, e->crtc_id);

    if (cr->scanout)
    {
        printf("crtc already in use\n");
        return -1;
    }

    // allset
    cr->scanout = create_sp_bo(mDev, m->hdisplay, m->vdisplay,
                               24, 32, DRM_FORMAT_XRGB8888, 0);
    if (!cr->scanout)
    {
        printf("failed to create new scanout bo\n");
        return -2;
    }

    fill_bo(cr->scanout, 0xff, 0xff, 0x0, 0x0);

    ret = drmModeSetCrtc(mDev->fd, e->crtc_id, cr->scanout->fb_id, 0, 0, &c->connector_id, 1, m);
    if (ret)
    {
        printf("failed to set crtc mode ret=%d\n", ret);
        return -3;
    }
    cr->crtc = drmModeGetCrtc(mDev->fd, cr->crtc->crtc_id);
    memcpy(&cr->crtc->mode, m, sizeof(*m));
    return 0;
}

void draw_screen_rgba(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != SCREEN_WIDTH * SCREEN_HEIGHT * 4)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 4:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        return;
    }

    if (cr == nullptr)
    {
        cxx_log("cr ==nullptr\n");
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

void draw_screen_rgb(uint8_t *data, uint32_t dataSize)
{
    if (dataSize != SCREEN_WIDTH * SCREEN_HEIGHT * 3)
    {
        cxx_log("dataSize:%d != SCREEN_WIDTH * SCREEN_HEIGHT * 3:%d\n", dataSize, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
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
            // dataPtr++;
        }
    }
}
