#ifndef __CTX_MANAGER_H__
#define __CTX_MANAGER_H__

MPP_RET test_ctx_deinit(MpiEncTestData *p);

MPP_RET test_ctx_init(MpiEncMultiCtxInfo *info);

MPP_RET test_mpp_enc_cfg_setup(MpiEncMultiCtxInfo *info);

#endif