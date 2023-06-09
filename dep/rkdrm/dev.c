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
#include "dev.h"

#if 0 // def USE_ATOMIC_API
static uint32_t get_prop_id(struct sp_dev *dev,
			    drmModeObjectPropertiesPtr props, const char *name){
	drmModePropertyPtr p;
	uint32_t i, prop_id = 0; /* Property ID should always be > 0 */

	for (i = 0; !prop_id && i < props->count_props; i++) {
		p = drmModeGetProperty(dev->fd, props->props[i]);
		if (!strcmp(p->name, name))
		prop_id = p->prop_id;
		drmModeFreeProperty(p);
	}
	if (!prop_id)
	printf("Could not find %s property\n", name);
	return prop_id;
}
#endif

int is_supported_format(struct sp_plane *plane, uint32_t format)
{
    uint32_t i;

    for (i = 0; i < plane->plane->count_formats; i++)
    {
        if (plane->plane->formats[i] == format)
            return 1;
    }
    return 0;
}

static int get_supported_format(struct sp_plane *plane, uint32_t *format)
{
    uint32_t i;

    for (i = 0; i < plane->plane->count_formats; i++)
    {
        if (plane->plane->formats[i] == DRM_FORMAT_XRGB8888 || plane->plane->formats[i] == DRM_FORMAT_ARGB8888 || plane->plane->formats[i] == DRM_FORMAT_RGBA8888)
        {
            *format = plane->plane->formats[i];
            return 0;
        }
    }
    printf("No suitable formats found!\n");
    return -ENOENT;
}

// 创建显示设备
struct sp_dev *create_sp_dev(void)
{
    struct sp_dev *dev;
    int ret, fd, i, j;
    drmModeRes *r = NULL;
    drmModePlaneRes *pr = NULL;

    // 打开设备/dev/dri/card0
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        printf("failed to open card0\n");
        return NULL;
    }

    // 申请内存
    dev = (struct sp_dev *)calloc(1, sizeof(*dev));
    if (!dev)
    {
        printf("failed to allocate dev\n");
        return NULL;
    }

    // 记录显示设备的fd
    dev->fd = fd;

    // 设置显示
    ret = drmSetClientCap(dev->fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret)
    {
        printf("failed to set client cap atomic\n");
        goto err;
    }
    ret = drmSetClientCap(dev->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    if (ret)
    {
        printf("failed to set client cap\n");
        goto err;
    }

    r = drmModeGetResources(dev->fd);
    if (!r)
    {
        printf("failed to get r\n");
        goto err;
    }

    // 获取显示设备总共的connector数量
    dev->num_connectors = r->count_connectors;
    dev->connectors = (drmModeConnectorPtr *)calloc(dev->num_connectors, sizeof(*dev->connectors));
    if (!dev->connectors)
    {
        printf("failed to allocate connectors\n");
        goto err;
    }

    // 分别获取connector, 就是hdmi/dsi等硬件连接
    for (i = 0; i < dev->num_connectors; i++)
    {
        dev->connectors[i] = drmModeGetConnector(dev->fd, r->connectors[i]);
        if (!dev->connectors[i])
        {
            printf("failed to get connector %d\n", i);
            goto err;
        }
    }

    printf("encoders number:%d\n", r->count_encoders);

    // 获取编码器(这个编码器是硬件的协议编码器, 跟视频编解码器两回事)
    dev->num_encoders = r->count_encoders;
    dev->encoders = (drmModeEncoderPtr *)calloc(dev->num_encoders, sizeof(*dev->encoders));
    if (!dev->encoders)
    {
        printf("failed to allocate encoders\n");
        goto err;
    }

    for (i = 0; i < dev->num_encoders; i++)
    {
        dev->encoders[i] = drmModeGetEncoder(dev->fd, r->encoders[i]);
        if (!dev->encoders[i])
        {
            printf("failed to get encoder %d\n", i);
            goto err;
        }
    }

    dev->num_crtcs = r->count_crtcs;
    dev->crtcs = (struct sp_crtc *)calloc(dev->num_crtcs, sizeof(struct sp_crtc));
    if (!dev->crtcs)
    {
        printf("failed to allocate crtcs\n");
        goto err;
    }
    
    for (i = 0; i < dev->num_crtcs; i++)
    {
        dev->crtcs[i].crtc = drmModeGetCrtc(dev->fd, r->crtcs[i]);
        if (!dev->crtcs[i].crtc)
        {
            printf("failed to get crtc %d\n", i);
            goto err;
        }
        dev->crtcs[i].scanout = NULL;
        dev->crtcs[i].pipe = i;
        dev->crtcs[i].num_planes = 0;
    }

    pr = drmModeGetPlaneResources(dev->fd);
    if (!pr)
    {
        printf("failed to get plane resources\n");
        goto err;
    }
    dev->num_planes = pr->count_planes;
    dev->planes = (struct sp_plane *)calloc(dev->num_planes, sizeof(struct sp_plane));
    for (i = 0; i < dev->num_planes; i++)
    {
        drmModeObjectPropertiesPtr props;
        struct sp_plane *plane = &dev->planes[i];

        plane->dev = dev;
        plane->plane = drmModeGetPlane(dev->fd, pr->planes[i]);
        if (!plane->plane)
        {
            printf("failed to get plane %d\n", i);
            goto err;
        }
        plane->bo = NULL;
        plane->in_use = 0;

        ret = get_supported_format(plane, &plane->format);
        if (ret)
        {
            printf("failed to get supported format: %d\n", ret);
            goto err;
        }

        for (j = 0; j < dev->num_crtcs; j++)
        {
            if (plane->plane->possible_crtcs & (1 << j))
                dev->crtcs[j].num_planes++;
        }
    }

    if (pr)
        drmModeFreePlaneResources(pr);
    if (r)
        drmModeFreeResources(r);

    return dev;
err:
    if (pr)
        drmModeFreePlaneResources(pr);
    if (r)
        drmModeFreeResources(r);
    destroy_sp_dev(dev);
    return NULL;
}

void destroy_sp_dev(struct sp_dev *dev)
{
    int i;

    if (dev->planes)
    {
        for (i = 0; i < dev->num_planes; i++)
        {
            if (dev->planes[i].in_use)
                put_sp_plane(&dev->planes[i]);
            if (dev->planes[i].plane)
                drmModeFreePlane(dev->planes[i].plane);
            if (dev->planes[i].bo)
                free_sp_bo(dev->planes[i].bo);
        }
        free(dev->planes);
    }
    if (dev->crtcs)
    {
        for (i = 0; i < dev->num_crtcs; i++)
        {
            if (dev->crtcs[i].crtc)
                drmModeFreeCrtc(dev->crtcs[i].crtc);
            if (dev->crtcs[i].scanout)
                free_sp_bo(dev->crtcs[i].scanout);
        }
        free(dev->crtcs);
    }
    if (dev->encoders)
    {
        for (i = 0; i < dev->num_encoders; i++)
        {
            if (dev->encoders[i])
                drmModeFreeEncoder(dev->encoders[i]);
        }
        free(dev->encoders);
    }
    if (dev->connectors)
    {
        for (i = 0; i < dev->num_connectors; i++)
        {
            if (dev->connectors[i])
                drmModeFreeConnector(dev->connectors[i]);
        }
        free(dev->connectors);
    }

    close(dev->fd);
    free(dev);
}
