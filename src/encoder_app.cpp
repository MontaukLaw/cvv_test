#include "encoder_user_comm.h"

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 960
#define INPUT_FILE_NAME "/home/orangepi/1.yuv"
#define OUTPUT_FILE_NAME "/home/orangepi/mpi_enc_test_demo/frame1.h264"

static char *input_file_name = (char *)INPUT_FILE_NAME;
static char *output_file_name = (char *)OUTPUT_FILE_NAME;

void init_params(MpiEncTestArgs *cmd)
{
    memset(cmd, 0, sizeof(MpiEncTestArgs));

    cmd->width = VIDEO_WIDTH;
    cmd->height = VIDEO_HEIGHT;
    cmd->type = MPP_VIDEO_CodingAVC;
    cmd->format = MPP_FMT_YUV420SP; // MPP_FMT_RGB888
    cmd->file_input = input_file_name;
    cmd->file_output = output_file_name;
    cmd->nthreads = 1;
}

static void release(MpiEncTestArgs *cmd)
{
    if (NULL == cmd)
        return;

    if (cmd->cfg_ini)
    {
        iniparser_freedict(cmd->cfg_ini);
        cmd->cfg_ini = NULL;
    }

    if (cmd->fps)
    {
        fps_calc_deinit(cmd->fps);
        cmd->fps = NULL;
    }

    // MPP_FREE(cmd->file_input);
    // MPP_FREE(cmd->file_output);
    MPP_FREE(cmd->file_cfg);
    MPP_FREE(cmd->file_slt);
    MPP_FREE(cmd);
}

// 起另一个线程来编码
int enc_test_multi(MpiEncTestArgs *cmd, const char *name)
{
    MpiEncMultiCtxInfo *ctxs = NULL;
    float total_rate = 0.0;
    RK_S32 ret = MPP_NOK;
    RK_S32 i = 0;

    ctxs = mpp_calloc(MpiEncMultiCtxInfo, cmd->nthreads);
    if (NULL == ctxs)
    {
        mpp_err("failed to alloc context for instances\n");
        return -1;
    }

    for (i = 0; i < cmd->nthreads; i++)
    {
        ctxs[i].cmd = cmd;
        ctxs[i].name = name;
        ctxs[i].chn = i;

        ret = pthread_create(&ctxs[i].thd, NULL, enc_test_process, &ctxs[i]);
        if (ret)
        {
            mpp_err("failed to create thread %d\n", i);
            return ret;
        }
    }

    if (cmd->frame_num < 0)
    {
        // wait for input then quit encoding
        mpp_log("*******************************************\n");
        mpp_log("**** Press Enter to stop loop encoding ****\n");
        mpp_log("*******************************************\n");

        getc(stdin);
        for (i = 0; i < cmd->nthreads; i++)
            ctxs[i].ctx.loop_end = 1;
    }

    return ret;

    for (i = 0; i < cmd->nthreads; i++)
    {
        pthread_join(ctxs[i].thd, NULL);
    }

    for (i = 0; i < cmd->nthreads; i++)
    {
        MpiEncMultiCtxRet *enc_ret = &ctxs[i].ret;

        mpp_log("chn %d encode %d frames time %lld ms delay %3d ms fps %3.2f bps %lld\n",
                i, enc_ret->frame_count, (RK_S64)(enc_ret->elapsed_time / 1000),
                (RK_S32)(enc_ret->delay / 1000), enc_ret->frame_rate, enc_ret->bit_rate);

        total_rate += enc_ret->frame_rate;
    }

    MPP_FREE(ctxs);

    total_rate /= cmd->nthreads;
    mpp_log("%s average frame rate %.2f\n", name, total_rate);

    return ret;
}

void start_encode(const char *name)
{
    MpiEncTestArgs *cmd = mpi_enc_test_cmd_get();

    init_params(cmd);

    enc_test_multi(cmd, name);

    // release(cmd);
}