#ifndef __RSTP_SERVER_H_
#define __RSTP_SERVER_H_

#include "rk_mpi.h"
#include "list.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <netinet/if_ether.h>
#include <net/if.h>

#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define nalu_sent_len 1400
// #define nalu_sent_len        1400
#define RTP_H264 96
#define RTP_AUDIO 97
#define MAX_RTSP_CLIENT 1
#define RTSP_SERVER_PORT 554
#define RTSP_RECV_SIZE 1024
#define RTSP_MAX_VID (640 * 1024)
#define RTSP_MAX_AUD (15 * 1024)

#define AU_HEADER_SIZE 4
#define PARAM_STRING_MAX 100

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1

typedef long long _int64;

typedef enum
{
    RTSP_VIDEO = 0,
    RTSP_VIDEOSUB = 1,
    RTSP_AUDIO = 2,
    RTSP_YUV422 = 3,
    RTSP_RGB = 4,
    RTSP_VIDEOPS = 5,
    RTSP_VIDEOSUBPS = 6
} enRTSP_MonBlockType;

struct _RTP_FIXED_HEADER
{
    /**/                              /* byte 0 */
    unsigned char csrc_len : 4; /**/  /* expect 0 */
    unsigned char extension : 1; /**/ /* expect 1, see RTP_OP below */
    unsigned char padding : 1; /**/   /* expect 0 */
    unsigned char version : 2; /**/   /* expect 2 */
    /**/                              /* byte 1 */
    unsigned char payload : 7; /**/   /* RTP_PAYLOAD_RTSP */
    unsigned char marker : 1; /**/    /* expect 1 */
    /**/                              /* bytes 2, 3 */
    unsigned short seq_no;
    /**/ /* bytes 4-7 */
    unsigned long timestamp;
    /**/                     /* bytes 8-11 */
    unsigned long ssrc; /**/ /* stream number is used here. */
} __PACKED__;

typedef struct _RTP_FIXED_HEADER RTP_FIXED_HEADER;

struct _NALU_HEADER
{
    // byte 0
    unsigned char TYPE : 5;
    unsigned char NRI : 2;
    unsigned char F : 1;

} __PACKED__; /**/ /* 1 BYTES */

typedef struct _NALU_HEADER NALU_HEADER;

struct _FU_INDICATOR
{
    // byte 0
    unsigned char TYPE : 5;
    unsigned char NRI : 2;
    unsigned char F : 1;

} __PACKED__; /**/ /* 1 BYTES */

typedef struct _FU_INDICATOR FU_INDICATOR;

struct _FU_HEADER
{
    // byte 0
    unsigned char TYPE : 5;
    unsigned char R : 1;
    unsigned char E : 1;
    unsigned char S : 1;
} __PACKED__; /**/ /* 1 BYTES */
typedef struct _FU_HEADER FU_HEADER;

struct _AU_HEADER
{
    // byte 0, 1
    unsigned short au_len;
    // byte 2,3
    unsigned short frm_len : 13;
    unsigned char au_index : 3;
} __PACKED__; /**/ /* 1 BYTES */
typedef struct _AU_HEADER AU_HEADER;

typedef struct
{
    int startblock;   // 代表开始文件块�?
    int endblock;     // 代表结束文件块号
    int BlockFileNum; // 代表录像段数

} IDXFILEHEAD_INFO; //.IDX文件的头信息

typedef struct
{
    _int64 starttime; // 代表开始shijian
    _int64 endtime;   // 代表结束shijian
    int startblock;   // 代表开始文件块�?
    int endblock;     // 代表结束文件块号
    int stampnum;     // 代表时间戳数�?
} IDXFILEBLOCK_INFO;  //.IDX文件段信�?

typedef struct
{
    int blockindex; // 代表所在文件块�?
    int pos;        // 代表该时间戳在文件块的位�?
    _int64 time;    // 代表时间戳时间戳的时间点
} IDXSTAMP_INFO;    //.IDX文件的时间戳信息

typedef struct
{
    char filename[150]; // 代表所在文件块�?
    int pos;            // 代表该时间戳在文件块的位�?
    _int64 time;        // 代表时间戳时间戳的时间点
} FILESTAMP_INFO;       //.IDX文件的时间戳信息

typedef struct
{
    char channelid[9];
    _int64 starttime; // 代表开始shijian
    _int64 endtime;   // 代表结束shijian
    _int64 session;
    int type;       // 类型
    int encodetype; // 编码格式;
} FIND_INFO;        //.IDX文件的时间戳信息

typedef enum
{
    RTP_UDP,
    RTP_TCP,
    RAW_UDP
} StreamingMode;

RTP_FIXED_HEADER *rtp_hdr;
NALU_HEADER *nalu_hdr;
FU_INDICATOR *fu_ind;
FU_HEADER *fu_hdr;
AU_HEADER *au_hdr;

extern char g_rtp_playload[20];
extern int g_audio_rate;

typedef enum
{
    RTSP_IDLE = 0,
    RTSP_CONNECTED = 1,
    RTSP_SENDING = 2,
} RTSP_STATUS;

typedef struct
{
    int nVidLen;
    int nAudLen;
    int bIsIFrm;
    int bWaitIFrm;
    int bIsFree;
    char vidBuf[RTSP_MAX_VID];
    char audBuf[RTSP_MAX_AUD];
} RTSP_PACK;

typedef struct
{
    int index;
    int socket;
    int reqchn;
    int seqnum;
    int seqnum2;
    unsigned int tsvid;
    unsigned int tsaud;
    int status;
    int sessionid;
    int rtpport[2];
    int rtcpport;
    char IP[20];
    char urlPre[PARAM_STRING_MAX];
} RTSP_CLIENT;

typedef struct
{
    int vidLen;
    int audLen;
    int nFrameID;
    char vidBuf[RTSP_MAX_VID];
    char audBuf[RTSP_MAX_AUD];
} FRAME_PACK;

typedef struct
{
    int startcodeprefix_len;     //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    unsigned len;                //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
    unsigned max_size;           //! Nal Unit Buffer size
    int forbidden_bit;           //! should be always FALSE
    int nal_reference_idc;       //! NALU_PRIORITY_xxxx
    int nal_unit_type;           //! NALU_TYPE_xxxx
    char *buf;                   //! contains the first byte followed by the EBSP
    unsigned short lost_packets; //! true, if packet loss is detected
} NALU_t;
extern int udpfd;
extern RTSP_CLIENT g_rtspClients[MAX_RTSP_CLIENT];
typedef struct _rtpbuf
{
    struct list_head list;
    RK_S32 len;
    char *buf;
} RTPbuf_s;

extern struct list_head RTPbuf_head;

extern void RtspServer_init(void);
extern void RtspServer_exit(void);

int AddFrameToRtspBuf(int nChanNum, enRTSP_MonBlockType eType, char *pData, unsigned int nSize, unsigned int nVidFrmNum, int bIFrm);

#endif