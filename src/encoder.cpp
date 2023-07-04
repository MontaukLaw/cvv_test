#include "encoder_user_comm.h"

bool newFrameArrived = false;
extern RK_U8 yuvFrameData[];

MPP_RET test_mpp_run(MpiEncMultiCtxInfo *info)
{
    MpiEncTestArgs *cmd = info->cmd;
    MpiEncTestData *p = &info->ctx;
    MppApi *mpi = p->mpi;
    MppCtx ctx = p->ctx;
    RK_U32 quiet = cmd->quiet;
    RK_S32 chn = info->chn;
    RK_U32 cap_num = 0;
    DataCrc checkcrc;
    MPP_RET ret = MPP_OK;

    memset(&checkcrc, 0, sizeof(checkcrc));
    checkcrc.sum = mpp_malloc(RK_ULONG, 512);

    if (p->type == MPP_VIDEO_CodingAVC || p->type == MPP_VIDEO_CodingHEVC)
    {
        printf("h264 coding\n");
        MppPacket packet = NULL;

        /*
         * Can use packet with normal malloc buffer as input not pkt_buf.
         * Please refer to vpu_api_legacy.cpp for normal buffer case.
         * Using pkt_buf buffer here is just for simplifing demo.
         */
        mpp_packet_init_with_buffer(&packet, p->pkt_buf);
        /* NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);

        ret = mpi->control(ctx, MPP_ENC_GET_HDR_SYNC, packet);
        if (ret)
        {
            mpp_err("mpi control enc get extra info failed\n");
            goto RET;
        }
        else
        {
            /* get and write sps/pps for H.264 */
            void *ptr = mpp_packet_get_pos(packet);
            size_t len = mpp_packet_get_length(packet);

            if (p->fp_output)
                fwrite(ptr, 1, len, p->fp_output);
        }

        mpp_packet_deinit(&packet);
    }

    // while (!p->pkt_eos)
    while (1)
    {
        // printf("while loop\n");
        if (newFrameArrived)
        {
            printf("encoding 1 frame start\n");

            MppMeta meta = NULL;
            MppFrame frame = NULL;
            MppPacket packet = NULL;
            void *buf = mpp_buffer_get_ptr(p->frm_buf);
            RK_S32 cam_frm_idx = -1;
            MppBuffer cam_buf = NULL;
            RK_U32 eoi = 1;
            // step 1. copy mem
            printf("copy mem size:%ld\n", p->frame_size);
            memcpy(buf, yuvFrameData, p->frame_size);

            // step 2. frame init
            printf("mpp_frame_init\n");
            ret = mpp_frame_init(&frame);
            if (ret)
            {
                mpp_err_f("mpp_frame_init failed\n");
                return ret;
            }

            printf("set frame properties\n");
            // step 3. 设置帧的属性
            mpp_frame_set_width(frame, p->width);
            mpp_frame_set_height(frame, p->height);
            mpp_frame_set_hor_stride(frame, p->hor_stride);
            mpp_frame_set_ver_stride(frame, p->ver_stride);
            mpp_frame_set_fmt(frame, p->fmt);
            mpp_frame_set_eos(frame, p->frm_eos);

            // step 4. set buffer
            printf("set frame buffer\n");
            mpp_frame_set_buffer(frame, p->frm_buf);

            // step 5. get meta
            meta = mpp_frame_get_meta(frame);
            mpp_packet_init_with_buffer(&packet, p->pkt_buf);

            /* NOTE: It is important to clear output packet length!! */
            mpp_packet_set_length(packet, 0);
            mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
            mpp_meta_set_buffer(meta, KEY_MOTION_INFO, p->md_info);

            if (!p->first_frm)
                p->first_frm = mpp_time();

            // step 6. put frame into ctx
            ret = mpi->encode_put_frame(ctx, frame);
            if (ret)
            {
                mpp_err("chn %d encode put frame failed\n", chn);
                mpp_frame_deinit(&frame);
                goto RET;
            }
            // destroy frame, no need frame any more
            mpp_frame_deinit(&frame);

            // step 7. encode
            do
            {
                // get packet first
                ret = mpi->encode_get_packet(ctx, &packet);
                if (ret)
                {
                    mpp_err("chn %d encode get packet failed\n", chn);
                    goto RET;
                }

                mpp_assert(packet);

                // if get packet
                if (packet)
                {
                    // write packet to file here
                    // 把packet写入文件就在这里
                    // packet的概念应该来自ffmpeg
                    void *ptr = mpp_packet_get_pos(packet);
                    size_t len = mpp_packet_get_length(packet);
                    printf("packet len:%ld\n", len);

                    char log_buf[256];
                    RK_S32 log_size = sizeof(log_buf) - 1;
                    RK_S32 log_len = 0;

                    if (!p->first_pkt)
                        p->first_pkt = mpp_time();

                    p->pkt_eos = mpp_packet_get_eos(packet);

                    if (p->fp_output)
                    {
                        // write stream data to file
                        // fwrite(ptr, 1, len, p->fp_output);
                    }

                    if (p->fp_verify && !p->pkt_eos)
                    {
                        calc_data_crc((RK_U8 *)ptr, (RK_U32)len, &checkcrc);
                        mpp_log("p->frame_count=%d, len=%d\n", p->frame_count, len);
                        write_data_crc(p->fp_verify, &checkcrc);
                    }

                    log_len += snprintf(log_buf + log_len, log_size - log_len, "encoded frame %-4d", p->frame_count);

                    /* for low delay partition encoding */
                    if (mpp_packet_is_partition(packet))
                    {
                        eoi = mpp_packet_is_eoi(packet);

                        log_len += snprintf(log_buf + log_len, log_size - log_len, " pkt %d", p->frm_pkt_cnt);
                        p->frm_pkt_cnt = (eoi) ? (0) : (p->frm_pkt_cnt + 1);
                    }

                    log_len += snprintf(log_buf + log_len, log_size - log_len, " size %-7zu", len);

                    if (mpp_packet_has_meta(packet))
                    {
                        meta = mpp_packet_get_meta(packet);
                        RK_S32 temporal_id = 0;
                        RK_S32 lt_idx = -1;
                        RK_S32 avg_qp = -1;

                        if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id))
                            log_len += snprintf(log_buf + log_len, log_size - log_len, " tid %d", temporal_id);

                        if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx))
                            log_len += snprintf(log_buf + log_len, log_size - log_len, " lt %d", lt_idx);

                        if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp))
                            log_len += snprintf(log_buf + log_len, log_size - log_len, " qp %d", avg_qp);
                    }

                    mpp_log_q(quiet, "chn %d %s\n", chn, log_buf);
                    printf("chn %d %s\n", chn, log_buf);

                    mpp_packet_deinit(&packet);
                    fps_calc_inc(cmd->fps);

                    p->stream_size += len;
                    p->frame_count += eoi;

                    if (p->pkt_eos)
                    {
                        mpp_log_q(quiet, "chn %d found last packet\n", chn);
                        mpp_assert(p->frm_eos);
                    }
                }
            } while (!eoi);

            if (p->frame_num > 0 && p->frame_count >= p->frame_num)
                break;

            if (p->loop_end)
                break;

            if (p->frm_eos && p->pkt_eos)
                break;

            newFrameArrived = 0;
        }
        usleep(1000);
        continue;

#if 0
        MppMeta meta = NULL;
        MppFrame frame = NULL;
        MppPacket packet = NULL;
        void *buf = mpp_buffer_get_ptr(p->frm_buf);
        RK_S32 cam_frm_idx = -1;
        MppBuffer cam_buf = NULL;
        RK_U32 eoi = 1;

        // 文件输入
        printf("file input read one frame\n");
        printf("width:%d, height:%d, hor_stride:%d, ver_stride:%d\n", p->width, p->height, p->hor_stride, p->ver_stride);
        printf("fmt:%d\n", p->fmt);

        // 读取一帧图像
        ret = read_image((RK_U8 *)buf, p->fp_input, p->width, p->height, p->hor_stride, p->ver_stride, p->fmt);
        if (ret == MPP_NOK || feof(p->fp_input))
        {
            p->frm_eos = 1;

            if (p->frame_num < 0 || p->frame_count < p->frame_num)
            {

                clearerr(p->fp_input);
                rewind(p->fp_input);
                p->frm_eos = 0;
                mpp_log_q(quiet, "chn %d loop times %d\n", chn, ++p->loop_times);
                printf("continue\n");
                continue;
            }
            mpp_log_q(quiet, "chn %d found last frame. feof %d\n", chn, feof(p->fp_input));
        }
        else if (ret == MPP_ERR_VALUE)
            goto RET;

        printf("mpp_frame_init\n");
        ret = mpp_frame_init(&frame);
        if (ret)
        {
            mpp_err_f("mpp_frame_init failed\n");
            goto RET;
        }

        printf("set frame properties\n");
        // 打印帧属性:
        printf("width:%d, height:%d, hor_stride:%d, ver_stride:%d\n", p->width, p->height, p->hor_stride, p->ver_stride);
        // 设置帧的属性
        mpp_frame_set_width(frame, p->width);
        mpp_frame_set_height(frame, p->height);
        mpp_frame_set_hor_stride(frame, p->hor_stride);
        mpp_frame_set_ver_stride(frame, p->ver_stride);
        mpp_frame_set_fmt(frame, p->fmt);
        mpp_frame_set_eos(frame, p->frm_eos);

        printf("set frame buffer\n");
        // 设置帧的buffer
        if (p->fp_input && feof(p->fp_input))
            mpp_frame_set_buffer(frame, NULL);
        else if (cam_buf)
            mpp_frame_set_buffer(frame, cam_buf);
        else
            mpp_frame_set_buffer(frame, p->frm_buf);

        meta = mpp_frame_get_meta(frame);
        mpp_packet_init_with_buffer(&packet, p->pkt_buf);

        /* NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);
        mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);
        mpp_meta_set_buffer(meta, KEY_MOTION_INFO, p->md_info);

        if (!p->first_frm)
            p->first_frm = mpp_time();

        printf("start encode\n");
        /*
         * NOTE: in non-block mode the frame can be resent.
         * The default input timeout mode is block.
         *
         * User should release the input frame to meet the requirements of
         * resource creator must be the resource destroyer.
         */
        ret = mpi->encode_put_frame(ctx, frame);
        if (ret)
        {
            mpp_err("chn %d encode put frame failed\n", chn);
            mpp_frame_deinit(&frame);
            goto RET;
        }

        mpp_frame_deinit(&frame);

        do
        {
            ret = mpi->encode_get_packet(ctx, &packet);
            if (ret)
            {
                mpp_err("chn %d encode get packet failed\n", chn);
                goto RET;
            }

            mpp_assert(packet);

            if (packet)
            {
                // write packet to file here
                // 把packet写入文件就在这里
                // packet的概念应该来自ffmpeg
                void *ptr = mpp_packet_get_pos(packet);
                size_t len = mpp_packet_get_length(packet);
                printf("packet len:%ld\n", len);

                char log_buf[256];
                RK_S32 log_size = sizeof(log_buf) - 1;
                RK_S32 log_len = 0;

                if (!p->first_pkt)
                    p->first_pkt = mpp_time();

                p->pkt_eos = mpp_packet_get_eos(packet);

                if (p->fp_output)
                    fwrite(ptr, 1, len, p->fp_output);

                if (p->fp_verify && !p->pkt_eos)
                {
                    calc_data_crc((RK_U8 *)ptr, (RK_U32)len, &checkcrc);
                    mpp_log("p->frame_count=%d, len=%d\n", p->frame_count, len);
                    write_data_crc(p->fp_verify, &checkcrc);
                }

                log_len += snprintf(log_buf + log_len, log_size - log_len, "encoded frame %-4d", p->frame_count);

                /* for low delay partition encoding */
                if (mpp_packet_is_partition(packet))
                {
                    eoi = mpp_packet_is_eoi(packet);

                    log_len += snprintf(log_buf + log_len, log_size - log_len, " pkt %d", p->frm_pkt_cnt);
                    p->frm_pkt_cnt = (eoi) ? (0) : (p->frm_pkt_cnt + 1);
                }

                log_len += snprintf(log_buf + log_len, log_size - log_len, " size %-7zu", len);

                if (mpp_packet_has_meta(packet))
                {
                    meta = mpp_packet_get_meta(packet);
                    RK_S32 temporal_id = 0;
                    RK_S32 lt_idx = -1;
                    RK_S32 avg_qp = -1;

                    if (MPP_OK == mpp_meta_get_s32(meta, KEY_TEMPORAL_ID, &temporal_id))
                        log_len += snprintf(log_buf + log_len, log_size - log_len, " tid %d", temporal_id);

                    if (MPP_OK == mpp_meta_get_s32(meta, KEY_LONG_REF_IDX, &lt_idx))
                        log_len += snprintf(log_buf + log_len, log_size - log_len, " lt %d", lt_idx);

                    if (MPP_OK == mpp_meta_get_s32(meta, KEY_ENC_AVERAGE_QP, &avg_qp))
                        log_len += snprintf(log_buf + log_len, log_size - log_len, " qp %d", avg_qp);
                }

                mpp_log_q(quiet, "chn %d %s\n", chn, log_buf);
                printf("chn %d %s\n", chn, log_buf);

                mpp_packet_deinit(&packet);
                fps_calc_inc(cmd->fps);

                p->stream_size += len;
                p->frame_count += eoi;

                if (p->pkt_eos)
                {
                    mpp_log_q(quiet, "chn %d found last packet\n", chn);
                    mpp_assert(p->frm_eos);
                }
            }
        } while (!eoi);

        if (cam_frm_idx >= 0)
            camera_source_put_frame(p->cam_ctx, cam_frm_idx);

        if (p->frame_num > 0 && p->frame_count >= p->frame_num)
            break;

        if (p->loop_end)
            break;

        if (p->frm_eos && p->pkt_eos)
            break;

#endif
    }
RET:
    MPP_FREE(checkcrc.sum);

    return ret;
}

// 主线程
void *enc_test_process(void *arg)
{
    MpiEncMultiCtxInfo *info = (MpiEncMultiCtxInfo *)arg;
    MpiEncTestArgs *cmd = info->cmd;
    MpiEncTestData *p = &info->ctx;
    MpiEncMultiCtxRet *enc_ret = &info->ret;
    MppPollType timeout = MPP_POLL_BLOCK;
    RK_U32 quiet = cmd->quiet;
    MPP_RET ret = MPP_OK;
    RK_S64 t_s = 0;
    RK_S64 t_e = 0;

    mpp_log_q(quiet, "%s start\n", info->name);

    // 初始化会话
    ret = test_ctx_init(info);
    if (ret)
    {
        mpp_err_f("test data init failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    ret = mpp_buffer_group_get_internal(&p->buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret)
    {
        mpp_err_f("failed to get mpp buffer group ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->frm_buf, p->frame_size + p->header_size);
    if (ret)
    {
        mpp_err_f("failed to get buffer for input frame ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->pkt_buf, p->frame_size);
    if (ret)
    {
        mpp_err_f("failed to get buffer for output packet ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    ret = mpp_buffer_get(p->buf_grp, &p->md_info, p->mdinfo_size);
    if (ret)
    {
        mpp_err_f("failed to get buffer for motion info output packet ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    // encoder demo
    // 创建编码器
    ret = mpp_create(&p->ctx, &p->mpi);
    if (ret)
    {
        mpp_err("mpp_create failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    mpp_log_q(quiet, "%p encoder test start w %d h %d type %d\n",
              p->ctx, p->width, p->height, p->type);

    ret = p->mpi->control(p->ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);
    if (MPP_OK != ret)
    {
        mpp_err("mpi control set output timeout %d ret %d\n", timeout, ret);
        goto MPP_TEST_OUT;
    }

    // 初始化mpp为编码
    ret = mpp_init(p->ctx, MPP_CTX_ENC, p->type);
    if (ret)
    {
        mpp_err("mpp_init failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    // 编码器配置初始化
    ret = mpp_enc_cfg_init(&p->cfg);
    if (ret)
    {
        mpp_err_f("mpp_enc_cfg_init failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    // 获取编码器配置
    ret = p->mpi->control(p->ctx, MPP_ENC_GET_CFG, p->cfg);
    if (ret)
    {
        mpp_err_f("get enc cfg failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    // 根据输入参数进行编码器的具体配置
    ret = test_mpp_enc_cfg_setup(info);
    if (ret)
    {
        mpp_err_f("test mpp setup failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    t_s = mpp_time();
    // 开始编码
    ret = test_mpp_run(info);

    t_e = mpp_time();
    if (ret)
    {
        mpp_err_f("test mpp run failed ret %d\n", ret);
        goto MPP_TEST_OUT;
    }

    ret = p->mpi->reset(p->ctx);
    if (ret)
    {
        mpp_err("mpi->reset failed\n");
        goto MPP_TEST_OUT;
    }

    enc_ret->elapsed_time = t_e - t_s;
    enc_ret->frame_count = p->frame_count;
    enc_ret->stream_size = p->stream_size;
    enc_ret->frame_rate = (float)p->frame_count * 1000000 / enc_ret->elapsed_time;
    enc_ret->bit_rate = (p->stream_size * 8 * (p->fps_out_num / p->fps_out_den)) / p->frame_count;
    enc_ret->delay = p->first_pkt - p->first_frm;

MPP_TEST_OUT:
    if (p->ctx)
    {
        mpp_destroy(p->ctx);
        p->ctx = NULL;
    }

    if (p->cfg)
    {
        mpp_enc_cfg_deinit(p->cfg);
        p->cfg = NULL;
    }

    if (p->frm_buf)
    {
        mpp_buffer_put(p->frm_buf);
        p->frm_buf = NULL;
    }

    if (p->pkt_buf)
    {
        mpp_buffer_put(p->pkt_buf);
        p->pkt_buf = NULL;
    }

    if (p->md_info)
    {
        mpp_buffer_put(p->md_info);
        p->md_info = NULL;
    }

    if (p->osd_data.buf)
    {
        mpp_buffer_put(p->osd_data.buf);
        p->osd_data.buf = NULL;
    }

    if (p->buf_grp)
    {
        mpp_buffer_group_put(p->buf_grp);
        p->buf_grp = NULL;
    }

    if (p->roi_ctx)
    {
        mpp_enc_roi_deinit(p->roi_ctx);
        p->roi_ctx = NULL;
    }

    test_ctx_deinit(p);

    return NULL;
}
