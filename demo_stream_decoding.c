#include <iostream>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <sstream>
#include <zmq.h>
#include <vector>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>
#include <signal.h>
#include <chrono>
#include <atomic>
#include <fstream>
#include <thread>
#include <semaphore.h>
#include <fcntl.h>

extern "C"{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "ams_api_codec.h"
#include "ams_api_decode.h"
#include "ams_api_board.h"

static void sprintf_time(char *curTime, int timeLen) {
    struct timeval tv_now;       // 保存时间 到毫秒级
    gettimeofday(&tv_now, NULL); // 取的当期秒数 毫秒级
    struct tm tm_now;
    localtime_r(&tv_now.tv_sec, &tm_now);

    snprintf(curTime, timeLen, "%02d/%02d %02d:%02d:%02d.%03d", tm_now.tm_mon + 1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min,
             tm_now.tm_sec, (int32_t)(tv_now.tv_usec / 1000));
    return;
}

static uint64_t get_mstime() {
    struct timeval time_cur;
    gettimeofday(&time_cur, NULL);
    return time_cur.tv_sec*1000 + time_cur.tv_usec/1000;
}

typedef struct {
    int  pid;
    int  width;
    int  height;
    int  keyframe;
    int  skip;
    char src_filename[1024];
    char out_filename[1024];
    int  save_data;
    uint8_t sync_yuv_data_flag;
    int  dev_id;
    int  coreidx;
    int  recv_frame_cnt;
    ams_codec_context_t *avctx;
    std::atomic<int> exit;
    char         zmq_bind[512];
    void        *zmq_ctx;
    void        *zmq_sck;
}DemuxDecCtx;

DemuxDecCtx decctx[1];

static int open_codec_context(DemuxDecCtx *ctx, int *stream_idx,
        AVCodecContext **codectx, AVFormatContext *fmtctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;

    ret = av_find_best_stream(fmtctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), ctx->src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmtctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *codectx = avcodec_alloc_context3(dec);
        if (!*codectx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*codectx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*codectx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

static void save_yuv(DemuxDecCtx *ctx, ams_codec_context_t* avctx, ams_frame_t* f, int _frame_idx) {
    char ofn[256] = {0};
    sprintf(ofn, "%s_%dx%d_%d.yuv", ctx->out_filename, ctx->width, ctx->height, _frame_idx);
    if(f->last_frame_flag)
        return;

    if (ams_frame_is_hwframe(f)) {
        f = ams_decode_retrieve_frame(avctx, f);
    }

    FILE *fp = fopen(ofn, "wb");
    if(fp != NULL) {
        ams_frame_save_to_file(f, fp);
        fclose(fp);
        char msg[1024] = {0};
        int msgsize = sprintf(msg, "{\"w\": %d, \"h\": %d, \"format\": %d, \"data\": [\"%s\"]}", 
                        ctx->width, ctx->height, 1, ofn);
        zmq_send(ctx->zmq_sck, msg, msgsize, 0);
        char buffer[8];
        zmq_recv(ctx->zmq_sck, buffer, 8, 0);
    }

    ams_free_frame(f);
}

static void *recv_thread(void *args) {
    DemuxDecCtx *ctx = (DemuxDecCtx*)args;
    ams_codec_context_t *avctx = ctx->avctx;
    int recv_frame_cnt = 0;
    //recv yuv from codec
    ams_frame_t* vpu_frame;
    bool last_frame = false;
    int retry = 0;
    do{
        vpu_frame = ams_decode_receive_frame(avctx);
        if(vpu_frame) {
            retry = 0;
            if (vpu_frame->last_frame_flag) {
                last_frame = true;
            } else {
                ++recv_frame_cnt;
            }

            if (ctx->save_data) {
                if (ctx->keyframe == 1) {
                   save_yuv(ctx, avctx, vpu_frame, recv_frame_cnt);
                } else {
                    if (((recv_frame_cnt % ctx->skip) == 0) || ((vpu_frame->frame_type == FRAME_TYPE_I) || (vpu_frame->frame_type == FRAME_TYPE_IDR))) {
                        save_yuv(ctx, avctx, vpu_frame, recv_frame_cnt);
                    } else {
                        ams_free_frame(vpu_frame);                        
                    }
                }
            } else {
                ams_free_frame(vpu_frame);
            }
        }else{
            usleep(2000);
            ++retry;
            if (retry < 10000) {
                continue;
            }
            ctx->exit = 1;
        }
    }while(ctx->exit == 0 && !last_frame);
    char szTime[32] = {0};
    sprintf_time(szTime, 32);
    printf("%s <<------ pid: %d, session_id: %d decode over, recv frame_cnt: %d\n", szTime, ctx->pid, avctx->session_id, recv_frame_cnt);
    ctx->recv_frame_cnt = recv_frame_cnt;
    return NULL;
}

static void store_2_file(ams_packet_t *pkt, int pid, int id, uint32_t s_id){
    char file_name[256];
    sprintf(file_name, "file_%d_%d_%u.h264", pid, id, s_id);
    FILE *fp = fopen(file_name, "a");
    if(fp != NULL){
        fwrite(pkt->data, 1, pkt->size, fp);
    }
    fclose(fp);
}

int demo_decode_file(DemuxDecCtx *ctx) {
    AVFormatContext         *formatCtx;
    AVCodecContext          *codeCtx;
    AVCodec                 *videoCodec = NULL;
    AVStream                *videoStream;
    int                      videoStreamIdx;
    AVPacket                *pkt;
    const AVBitStreamFilter *bitStreamFilter;
    AVBSFContext            *bsfCtx;
    AVDictionary            *options = NULL;                // ffmpeg数据字典，用于配置一些编码器属性等
    int ret = 0;
    int send_pkt_cnt = 0;
    ams_packet_t packet;
    uint64_t lbegin = 0, lend = 0;
    pthread_t rtids[1] = {0};
    char szTime[32] = {0};
    sprintf_time(szTime, 32);
    printf("%s pid:%d Demuxing video from file '%s' into '%s'\n", szTime, ctx->pid, ctx->src_filename, ctx->out_filename);
    ctx->exit = 0;

    //init for avctx
    ams_codec_context_t avctx;
    ams_init_codec_context(&avctx, AMS_CODEC_ID_H264);
    avctx.board_id = ctx->dev_id;
    avctx.bitstreamMode = 2;
    avctx.decode_return_yuv_flag = ctx->sync_yuv_data_flag;
    avctx.transcode_flag = 0;
    avctx.coreidx = ctx->coreidx;

   // 步骤六：打开解码器
    // 设置缓存大小 8196000byte
    av_dict_set(&options, "buffer_size", "8196000", 0);
    // 设置超时时间 3s
    av_dict_set(&options, "stimeout", "3000000", 0);
    // 设置最大延时 1s
    av_dict_set(&options, "max_delay", "1000000", 0);
    // 设置打开方式 tcp/udp
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    // 设置打开方式 tcp/udp
//    av_dict_set(&options, "rtsp_transport", "tcp", 0);

   /* open input file, and allocate format context */
    if ((ret = avformat_open_input(&formatCtx, ctx->src_filename, NULL, &options)) < 0) {
        char buf[1024] = {0};
        av_strerror(ret, buf, 1024);
        fprintf(stderr, "Could not open source file %s, ret=%d\n", ctx->src_filename, ret);
        return -1;//        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -2;//        exit(1);
    }

    videoStreamIdx = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (videoStreamIdx < 0) {
        fprintf(stderr, "Could not find %s stream index in input file '%s'\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO), ctx->src_filename);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -3;
    }

    videoStream = formatCtx->streams[videoStreamIdx];
    if (videoStream == NULL) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO), ctx->src_filename);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -3;
    }

    codeCtx = avcodec_alloc_context3(videoCodec);
    if (codeCtx == NULL) {
        fprintf(stderr, "Failed to allocate the %s codec context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -4;
    }

    ret = avcodec_parameters_to_context(codeCtx, videoStream->codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        avcodec_free_context(&codeCtx);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -5;
    }

    ret = avcodec_open2(codeCtx, videoCodec, &options);
    if (ret < 0) {
        fprintf(stderr, "Failed to open %s codec\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        avcodec_free_context(&codeCtx);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -6;
    }
    ctx->width  = codeCtx->width;
    ctx->height = codeCtx->height;
    int32_t codec_id = codeCtx->codec_id;
    if (codec_id == AV_CODEC_ID_H264){
        bitStreamFilter = av_bsf_get_by_name("h264_mp4toannexb");
        avctx.codec_id = AMS_CODEC_ID_H264;
    }else if (codec_id == AV_CODEC_ID_HEVC){
        bitStreamFilter =  av_bsf_get_by_name("hevc_mp4toannexb");
        avctx.codec_id = AMS_CODEC_ID_HEVC;
    }else{
        printf("invalid video format, should be h264 or h265.");
        avcodec_free_context(&codeCtx);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -5;
    }

    av_bsf_alloc(bitStreamFilter, &bsfCtx);
    avcodec_parameters_copy(bsfCtx->par_in, videoStream->codecpar);
    av_bsf_init(bsfCtx);

    sprintf_time(szTime, 32);
    printf("%s ------>>> pid:%d, width: %d, height: %d, create\n", szTime, ctx->pid, ctx->width, ctx->height);
    ret = ams_decode_create(&avctx);
    sprintf_time(szTime, 32);
    printf("%s ------>>> pid:%d, session:%u, width: %d, height: %d, create over\n", szTime, ctx->pid, avctx.session_id, ctx->width, ctx->height);

    if(ret < 0){
        printf("error when create one decoder, %s,%d\n", __FILE__, __LINE__);
        avcodec_free_context(&codeCtx);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -6;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "error Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        avcodec_free_context(&codeCtx);
        avformat_close_input(&formatCtx);
        av_dict_free(&options);
        return -7;
    }
    ctx->avctx = &avctx;
    ctx->recv_frame_cnt = 0;
    pthread_create(rtids, NULL, recv_thread, ctx);
    /* read frames from the file */
    lbegin = get_mstime();
    int retry = 0;
    while (ctx->exit == 0 && av_read_frame(formatCtx, pkt) >= 0) {
        packet.data = NULL;
        packet.size = 0;
        retry = 0;
        if (pkt->stream_index != videoStreamIdx) {
            av_packet_unref(pkt);
            continue;
        }
        
        if (ctx->keyframe == 1) {
           if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
                av_packet_unref(pkt);
                continue;               
           }
           packet.data = pkt->data;
           packet.size = pkt->size;
           //store_2_file(&packet, ctx->pid, 0, avctx.session_id);
        }

        av_bsf_send_packet(bsfCtx, pkt);
        av_packet_unref(pkt);

        while (ctx->exit == 0 && av_bsf_receive_packet(bsfCtx, pkt) == 0) {
            packet.data = pkt->data;
            packet.size = pkt->size;
            //store_2_file(&packet, ctx->pid, 1, avctx.session_id);
            retry = 0;
            do{
                ret = ams_decode_send_packet(&avctx, &packet);
                if(ret < 0){
                    usleep(2000);
                    ++retry;
                    if (retry < 10000) {
                        continue;
                    }
                    ctx->exit = 1;
                }
            }while(ret < 0 && ctx->exit == 0);

            av_packet_unref(pkt);
            if (ret < 0){
                printf("%s,%d:Error demoid:%d, session_id:%u retry:%d pkt size:%d ret:%d, send_pkt_cnt:%d\n", __func__, __LINE__, ctx->pid, avctx.session_id, retry, pkt->size, ret, send_pkt_cnt);
                break;
            }
            ++send_pkt_cnt;
       }
       //usleep(40000);
    }

    av_bsf_free(&bsfCtx);
    avcodec_free_context(&codeCtx);
    avformat_close_input(&formatCtx);
    av_dict_free(&options);
    av_packet_free(&pkt);

    // send a empty frame for ending of stream
    memset(&packet, 0, sizeof(ams_packet_t));
    do{
        ret = ams_decode_send_packet(&avctx, &packet);
        if(ret < 0){
            usleep(1000);
        }
    }while(ret < 0 && ctx->exit == 0);

    pthread_join(rtids[0], NULL);
    //close one way
    lend = get_mstime();
    sprintf_time(szTime, 32);
    printf("%s <<------ pid:%d will close session:%u use time: %ld seconds send_pkt_cnt:%d recv_frame_cnt:%d\n", szTime, ctx->pid, avctx.session_id, (lend - lbegin)/1000 , send_pkt_cnt, ctx->recv_frame_cnt);
    ret = ams_decode_close(&avctx);
    return 0;
}

typedef struct {
    char         input[1024];
    char         output[1024];
    int          active;
    int          index;
    int          pid;
    int          keyframe;
    int          skip;
    int          exit;
} demo_ctx;
static demo_ctx demctx[1];

int init(demo_ctx *dctx, DemuxDecCtx *ctx) {
    memset(ctx, 0, sizeof(DemuxDecCtx));
    ctx->keyframe = dctx->keyframe;
    ctx->skip     = dctx->skip;
    printf("Connecting to algo server...\n");
    ctx->zmq_ctx = zmq_ctx_new();
    ctx->zmq_sck = zmq_socket(ctx->zmq_ctx, ZMQ_REQ);
    sprintf(ctx->zmq_bind, "tcp://localhost:8003");
    zmq_connect(ctx->zmq_sck, ctx->zmq_bind);
    
    int ret = -1;
    av_register_all();
    avformat_network_init();
    ctx->save_data = 1;
    //init for decoder
    ret = ams_codec_dev_init(-1);
    ctx->pid     = dctx->pid;
    ctx->dev_id  = dctx->index % ams_codec_dev_get_num();
    ctx->coreidx = -1;
    ctx->sync_yuv_data_flag = 0;    
    return 0;
}

void requester_process(demo_ctx *dctx, const char *url) {
    char szTime[32] = {0};
    dctx->pid = getpid();
    init(dctx, decctx);
    sprintf(decctx->src_filename, "%s", url);
    sprintf(decctx->out_filename, "%s/stream_%d", dctx->output, dctx->pid);
    demo_decode_file(decctx);
    sprintf_time(szTime, 32);
    printf("%s pid:%d exit:%s\n", szTime, dctx->pid, url);
}

void replier_process(demo_ctx *dctx) {
    FILE* fp = fopen(dctx->input, "r");
    char szTime[32] = {0};
    if (fp == NULL) {
        printf("Failed to open file:%s\n", dctx->input);
        return;
    }

    sprintf_time(szTime, 32);
    printf("%s pid:%d read video stream address\n", szTime, dctx->pid);
    char url[1024] = {0};
    while (fgets(url, 1020, fp) != NULL) {
        url[strcspn(url, "\n")] = '\0';
        {
            pid_t pid = fork();
            if (pid == -1) {
                printf("Failed to fork.");
            } else if (pid == 0) { // 子进程
                requester_process(dctx, url);
                exit(EXIT_SUCCESS);
            } else { // 父进程
                ++dctx->active;
                ++dctx->index;
            }        
        }
    }
    fclose(fp);
    sprintf_time(szTime, 32);
    printf("%s pid:%d total video stream process:%d\n", szTime, dctx->pid, dctx->active);
}

void check_thread(demo_ctx *dctx) {
    char szTime[32] = {0};
    while (dctx->exit == 0)  {
        sprintf_time(szTime, 32);
        printf("%s decode stream video active:%d total:%d\n", szTime, dctx->active, dctx->index);
        if(dctx->active <= 0) {
            break;
        }
        int status;
//        pid_t pid = waitpid(-1, &status, 0);
        pid_t pid = waitpid(-1, &status, WNOHANG);
//        printf("pid:%d %d \n", pid,(status >> 8) & 0xFF);
        if (pid != 0) {
            --dctx->active;
            continue;
        }
        sleep(5);
    }
    dctx->exit = 0;
}

/*
 * g++ -fPIC -std=c++11  -I../include -L../lib demo_stream_decoding.c -o demo_stream_decoding  -lpthread -lzmq -lavformat -lavcodec -lavutil -lswscale -lswresample -lavfilter -lams_codec 
 * mkdir -p /tmp/data/;mount -t ramfs -o size=10M ramfs /tmp/data/
 * ./demo_stream_decoding -i ./rtsp.txt -o /tmp/data/ -k
 * ./demo_stream_decoding -i ./rtsp.txt -o /tmp/data/ -p 8 -k
 */
void usage(char* cmd) {
//    printf("Usage: %s [-h] -i input -o output -p maxprocess -s skip -t maxtime -k -d\n", cmd);
    printf("Usage: %s [-h] -i input -o output -s skip -k \n", cmd);
    printf("Options:\n");
    printf("  -h              Print this help message.\n");
    printf("  -i input        input file include stream address.\n");
    printf("  -o output       decode *.yuv save in output dir.\n");
    printf("  -s skip         save one frame by every skip frames, when there is I frame or IDR frame in the frames, it is saved, default 30.\n");
    printf("  -k              enable, only process and save key frame, default disable.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  shell>  %s -i ./rtsp.txt -o /mnt/ramfs/ -s 30\n", cmd); 
    printf("  shell>  %s -i ./rtsp.txt -o /mnt/ramfs/ -k\n", cmd); 
}

int parse_opt(int argc, char* argv[], demo_ctx *dctx) {
//    const char* optstr = "hkdi:o:p:s:t:";
    const char* optstr = "hki:o:s:";
    uint64_t val = 0;
    int opt;
    while( (opt = getopt(argc, argv, optstr)) != -1 ) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'k':// 0x01
            val |= (0x1);
            dctx->keyframe = 1;
            break;
        case 'i':// 0x04
            val |= (0x1 << 2);
            sprintf(dctx->input, "%s", optarg);
            break;
        case 'o':// 0x08
            val |= (0x1 << 3);
            sprintf(dctx->output, "%s", optarg);
            break;
        case 's':// 0x20
            val |= (0x1 << 5);
            dctx->skip = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return 0;
        }
    }
    
    if ((val & 0xC) == 0xC) {
        return 1;
    }

    usage(argv[0]);
    return 0;
}

int main(int argc, char *argv[]) {
    demo_ctx *dctx = demctx;
    memset(dctx, 0, sizeof(demo_ctx));
    dctx->skip     = 30;
    if (parse_opt(argc, argv, dctx) < 1) {
        return 1;
    }

    int ret = ams_codec_dev_init(-1);
    sleep(1);
    ret = ams_codec_dev_deinit(-1);
    
    auto start = std::chrono::system_clock::now();
    replier_process(dctx); // 父进程是 Replier
    // 等待所有子进程退出
    check_thread(dctx);
    char szTime[32] = {0};
    sprintf_time(szTime, 32);
    printf("%s decode stream video active:%d total:%d\n", szTime, dctx->active, dctx->index);
    dctx->exit = 1;
    while (dctx->active > 0)  {
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid != 0) {
            --dctx->active;
        }
    }
    auto end = std::chrono::system_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " s" << std::endl;
    return 0;
}