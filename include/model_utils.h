#ifndef __MODEL_UTILS_H__
#define __MODEL_UTILS_H__

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

#include "display.h"

// int init_model(rknn_context ctx);

void dump_tensor_attr(rknn_tensor_attr *attr);

int init_model_file(rknn_context *ctx, char *model_name, char *model_data);

char *load_model(const char *filename, int *model_size);

// int init_model(rknn_context ctx, int *model_width, int *model_height, int* channel );
int init_model(rknn_context ctx, int *model_width, int *model_height, int *channel, rknn_tensor_attr* output_attrs);

#endif
