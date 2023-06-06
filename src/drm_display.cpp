#include "drm_display.h"


using namespace std;

static sp_dev *mDev;
static sp_plane *mTestPlane;
static sp_plane **mPlanes;
static sp_crtc *mCrtc;

void init_display()
{
    int ret = 0;

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

    return 0;
}

void drm_display_fresh()
{
    sp_bo *bo;
    uint32_t handles[4], pitches[4], offsets[4];
    int width, height;
    int frm_size, ret, fd, err;

    width = 800;
    height = 1280;
    cxx_log("frame width: %d height: %d\n", width, height);
    width = CODEC_ALIGN(width, 16);
    height = CODEC_ALIGN(height, 16);

    // frm_size = width * height * 3 / 2;

    // fd = mpp_buffer_get_fd(mpp_frame_get_buffer(frame));
    // cxx_log("frame fd:%d\n", fd);

    bo = (struct sp_bo *)calloc(1, sizeof(struct sp_bo));
    if (!bo)
    {
        cxx_log("failed to calloc bo.\n");
        return -2;
    }

    drmPrimeFDToHandle(mDev->fd, fd, &bo->handle);
    bo->dev = mDev;
    bo->width = width;
    bo->height = height;
    bo->depth = 16;
    bo->bpp = 32;
    // DRM_FORMAT_RGBX8888_A8;
    bo->format = DRM_FORMAT_NV12;
    // bo->format = DRM_FORMAT_RGBX8888_A8;
    bo->flags = 0;

    handles[0] = bo->handle;
    pitches[0] = width;
    offsets[0] = 0;
    handles[1] = bo->handle;
    pitches[1] = width;
    offsets[1] = width * height;
    ret = drmModeAddFB2(mDev->fd, bo->width, bo->height,
                        bo->format, handles, pitches, offsets,
                        &bo->fb_id, bo->flags);
    if (ret != 0)
    {
        cxx_log("failed to exec drmModeAddFb2.\n");
        return -3;
    }

    

    cxx_log("plane id:%d \n", mTestPlane->plane->plane_id);
    cxx_log("crtc_id :%d \n", mCrtc->crtc->crtc_id);
    cxx_log("crtc h:%d v:%d\n", mCrtc->crtc->mode.hdisplay,
            mCrtc->crtc->mode.vdisplay);

    uint32_t plane_id = 197;
    uint32_t crtc_id = 184;
    uint16_t hdisplay = 800;
    uint16_t vdisplay = 1280;

    // ret = drmModeSetPlane(mDev->fd, plane_id,
    //                       crtc_id, bo->fb_id, 0, 0, 0,
    //                       hdisplay,
    //                       vdisplay,
    //                       0, 0, bo->width << 16, bo->height << 16);

    cxx_log("bo->width :%d bo->height:%d\n", bo->width, bo->height);
    // ret = drmModeSetPlane(mDev->fd, mTestPlane->plane->plane_id, mCrtc->crtc->crtc_id, bo->fb_id,
    //                       0, 0, 0, 800, 1280, 0, 0,
    //                       bo->width << 16, bo->height << 16);
    ret = drmModeSetPlane(mDev->fd, mTestPlane->plane->plane_id, mCrtc->crtc->crtc_id, bo->fb_id,
                          0, 0, 0, 800, 1280, 0, 0,
                          bo->width << 16, bo->height << 16);
    if (ret)
    {
        cxx_log("failed to exec drmModeSetPlane.\n");
        return -3;
    }

    if (mTestPlane->bo)
    {
        if (mTestPlane->bo->fb_id)
        {
            ret = drmModeRmFB(mDev->fd, mTestPlane->bo->fb_id);
            if (ret)
                cxx_log("failed to exec drmModeRmFB.\n");
        }
        if (mTestPlane->bo->handle)
        {
            struct drm_gem_close req = {
                .handle = mTestPlane->bo->handle,
            };

            drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
        }
        free(mTestPlane->bo);
    }
    mTestPlane->bo = bo;

    return 0;
}