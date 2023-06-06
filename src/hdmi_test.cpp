#include "drm_display.h"

int main()
{
    static drmModeConnectorPtr c = nullptr;
    static drmModeEncoderPtr e = nullptr;
    static sp_dev *mDev;
    static struct sp_crtc *cr;
    static drmModeModeInfoPtr m;
    cxx_log("init desplay\n");

    // 创建显示设备
    mDev = create_sp_dev();
    if (!mDev)
    {
        cxx_log("failed to exec create_sp_dev.\n");
        return -10;
    }

    int j = 0;
    int ret = 0;
    c = mDev->connectors[0];
    m = &c->modes[0];
    e = mDev->encoders[0];
    printf("dev->num_connectors :%d\n", mDev->num_connectors);
    c->encoder_id = e->encoder_id;

    for (j = 0; j < mDev->num_encoders; j++)
    {
        e = mDev->encoders[j];
        cxx_log("e->crtc_id:%d\n", e->crtc_id);
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
    /*
     * Todo:
     * I don't know why crtc mode is empty, just copy PREFERRED mode
     * for it.
     */
    memcpy(&cr->crtc->mode, m, sizeof(*m));

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