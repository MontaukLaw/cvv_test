#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "drm_display.h"

int create_display_device();

void draw_screen_rgba(uint8_t *data, uint32_t dataSize);

void draw_screen_rgb(uint8_t *data, uint32_t dataSize);

int dis_init();

void read_rgba8888_pic_test();

void draw_screen(uint8_t *data, uint32_t dataSize);
void draw_screen_rgb_888(uint8_t *data, uint32_t dataSize);

void draw_screen_rgb_1440(uint8_t *data, uint32_t dataSize);

void draw_screen_rgb_960(uint8_t *data, uint32_t dataSize);

void release_bo();

int double_dis_init();

void draw_lcd_screen_rgb_960(uint8_t *data, uint32_t dataSize);

void draw_hdmi_screen_rgb(uint8_t *data, uint32_t dataSize);

void hdmi_draw_test();

#endif
