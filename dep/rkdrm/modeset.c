//
// Created by root on 17-11-7.
//

/*
 * Copyright 2016 Rockchip Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "modeset.h"

// 让屏幕全黑
int initialize_screens(struct sp_dev *dev)
{
    int ret, i, j;

    printf("dev->num_connectors :%d\n", dev->num_connectors);
    // 就设置了一个连接器, 且已经连接了
    for (i = 0; i < dev->num_connectors; i++)
    {
        drmModeConnectorPtr c = dev->connectors[i];
        drmModeModeInfoPtr m = NULL;
        drmModeEncoderPtr e = NULL;
        struct sp_crtc *cr = NULL;

        printf("connector_id:%d\n", c->connector_id);
        if (c->connection != DRM_MODE_CONNECTED)
            continue;
        printf("connected connector_id:%d\n", c->connector_id);

        if (!c->count_modes)
        {
            printf("connector has no modes, skipping\n");
            continue;
        }

        /* Take the first unless there's a preferred mode */
        printf("c->count_modes:%d\n", c->count_modes);
        // 输出也就一个
        m = &c->modes[0];
        for (j = 0; j < c->count_modes; j++)
        {
            drmModeModeInfoPtr tmp_m = &c->modes[j];

            if (!(tmp_m->type & DRM_MODE_TYPE_PREFERRED))
                continue;

            m = tmp_m;
            break;
        }

        printf("c->encoder_id :%d\n", c->encoder_id);
        if (!c->encoder_id)
        {
            /*
             * default drm encoder not attached connector, just
             * select the first one.
             */
            if (dev->num_encoders)
            {
                e = dev->encoders[0];
                c->encoder_id = e->encoder_id;
            }
            else
            {
                printf("no encoder attached to the connector\n");
                continue;
            }
        }

        printf("c->encoder_id :%d\n", c->encoder_id);

        printf("dev->num_encoders:%d\n", dev->num_encoders);
        for (j = 0; j < dev->num_encoders; j++)
        {
            e = dev->encoders[j];
            printf("encoder is:%d\n", e->encoder_id);
            if (e->encoder_id == c->encoder_id)
            {
                printf("break encoder is:%d\n", e->encoder_id);
                break;
            }
        }
        if (j == dev->num_encoders)
        {
            printf("could not find encoder for the connector\n");
            continue;
        }
        // 打印encoder id
        printf("final encoder is:%d, crtc id is %d\n", e->encoder_id, e->crtc_id);
        printf("j:%d\n", j);


        if (!e->crtc_id)
        {
            /*
             * default drm crtc not attached encoder, just
             * select the first one.
             */
            if (dev->num_crtcs)
            {
                cr = &dev->crtcs[j];
                e->crtc_id = cr->crtc->crtc_id;
            }
            else
            {
                printf("no crtc attached to the encoder\n");
                continue;
            }
        }

        for (j = 0; j < dev->num_crtcs; j++)
        {
            cr = &dev->crtcs[j];

            if (cr->crtc->crtc_id == e->crtc_id)
                break;
        }
        if (j == dev->num_crtcs)
        {
            printf("could not find crtc for the encoder\n");
            continue;
        }
        if (cr->scanout)
        {
            printf("crtc already in use\n");
            continue;
        }

        /* XXX: Hardcoding the format here... :| */
        cr->scanout = create_sp_bo(dev, m->hdisplay, m->vdisplay,
                                   24, 32, DRM_FORMAT_XRGB8888, 0);
        if (!cr->scanout)
        {
            printf("failed to create new scanout bo\n");
            continue;
        }

        fill_bo(cr->scanout, 0xff, 0xff, 0x0, 0x0);

        ret = drmModeSetCrtc(dev->fd, cr->crtc->crtc_id,
                             cr->scanout->fb_id, 0, 0, &c->connector_id,
                             1, m);
        if (ret)
        {
            printf("failed to set crtc mode ret=%d\n", ret);
            continue;
        cr->crtc = drmModeGetCrtc(dev->fd, cr->crtc->crtc_id);
        }
        /*
         * Todo:
         * I don't know why crtc mode is empty, just copy PREFERRED mode
         * for it.
         */
        memcpy(&cr->crtc->mode, m, sizeof(*m));
    }
    return 0;
}

struct sp_plane *get_sp_plane(struct sp_dev *dev, struct sp_crtc *crtc)
{
    int i;

    for (i = 0; i < dev->num_planes; i++)
    {
        struct sp_plane *p = &dev->planes[i];

        if (p->in_use)
            continue;

        if (!(p->plane->possible_crtcs & (1 << crtc->pipe)))
            continue;

        p->in_use = 1;
        return p;
    }
    return NULL;
}

void put_sp_plane(struct sp_plane *plane)
{
    drmModePlanePtr p;

    /* Get the latest plane information (most notably the crtc_id) */
    p = drmModeGetPlane(plane->dev->fd, plane->plane->plane_id);
    if (p)
        plane->plane = p;

    if (plane->plane->crtc_id)
        drmModeSetPlane(plane->dev->fd, plane->plane->plane_id,
                        plane->plane->crtc_id, 0, 0,
                        0, 0, 0, 0, 0, 0, 0, 0);

    if (plane->bo)
    {
        free_sp_bo(plane->bo);
        plane->bo = NULL;
    }
    plane->in_use = 0;
}

int set_sp_plane(struct sp_dev *dev, struct sp_plane *plane,
                 struct sp_crtc *crtc, int x, int y)
{
    int ret;
    uint32_t w, h;

    w = plane->bo->width;
    h = plane->bo->height;

    if ((w + x) > crtc->crtc->mode.hdisplay)
        w = crtc->crtc->mode.hdisplay - x;
    if ((h + y) > crtc->crtc->mode.vdisplay)
        h = crtc->crtc->mode.vdisplay - y;

    ret = drmModeSetPlane(dev->fd, plane->plane->plane_id,
                          crtc->crtc->crtc_id, plane->bo->fb_id, 0, x, y, w, h,
                          0, 0, w << 16, h << 16);
    if (ret)
    {
        printf("failed to set plane to crtc ret=%d\n", ret);
        return ret;
    }

    return ret;
}

#ifdef USE_ATOMIC_API
int set_sp_plane_pset(struct sp_dev *dev, struct sp_plane *plane,
                      drmModePropertySetPtr pset, struct sp_crtc *crtc, int x, int y)
{
    int ret;
    uint32_t w, h;

    w = plane->bo->width;
    h = plane->bo->height;

    if ((w + x) > crtc->crtc->mode.hdisplay)
        w = crtc->crtc->mode.hdisplay - x;
    if ((h + y) > crtc->crtc->mode.vdisplay)
        h = crtc->crtc->mode.vdisplay - y;

    ret = drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->crtc_pid, crtc->crtc->crtc_id) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->fb_pid, plane->bo->fb_id) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->crtc_x_pid, x) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->crtc_y_pid, y) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->crtc_w_pid, w) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->crtc_h_pid, h) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->src_x_pid, 0) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->src_y_pid, 0) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->src_w_pid, w << 16) ||
          drmModePropertySetAdd(pset, plane->plane->plane_id,
                                plane->src_h_pid, h << 16);
    if (ret)
    {
        printf("failed to add properties to the set\n");
        return -1;
    }

    return ret;
}
#endif