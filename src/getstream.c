#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/libavformat/avformat.h"
#include "../include/libavutil/time.h"
#include "../include/libavutil/mathematics.h"

#include "../include/alsa/asoundlib.h"

#define RTMP_ADDR "rtmp://192.168.2.122/live/password"

#define LOOP_RUN 1
#define LOOP_STOP 0

#define CHANNELS 1
#define SAMPLE_RATE 48000
#define ALLOW_RESAMPLING 1
#define LATENCY 500000 /* 0.5s others all set like this */

int loop_state_flag;

void signal_handler(int sig) {
    if(sig == SIGINT) { /* no need? */
        loop_state_flag = LOOP_STOP;
    }
}

void receive_rtmp() {

    snd_pcm_t *handle;

    /* open audio device */
    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        printf("Error: Open audio device failed!\r\n");
        goto  fail_open_audiodevice;
    }
    printf("Open audio device succeed!\r\n");

    if (snd_pcm_set_params(handle, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 
            CHANNELS, SAMPLE_RATE, ALLOW_RESAMPLING, LATENCY) < 0) {
        printf("Error: Set para failed!\r\n");
        goto  fail_set_para;
    }
    printf("Set para succeed!\r\n");

    /* open stream and get head info */
    AVFormatContext *ifmt_ctx = NULL;
    if (avformat_open_input(&ifmt_ctx, RTMP_ADDR, NULL, NULL) < 0) {
        printf("Error: Connect to the server failed!Please check the url!\r\n");
        goto fail_open_stream;
    }
    printf("Connect to the server succeed!\r\n");

    /* get extra stream info */
    if (avformat_find_stream_info(ifmt_ctx, NULL) < 0) {
        printf("Error: Get extra info failed!\r\n");
        goto fail_get_streaminfo;
    }
    printf("Get extra info succeed!\r\n");

    /* print some input info */
    av_dump_format(ifmt_ctx, 0, RTMP_ADDR, 0);

    /*
     * please match the input audio stream and output audio stream format
     * or use swr_convert() function to convert
     * but we only use swr_convert() to change sample rate
     */

    /* find best audio stream */
    int audio_idx = -1; 
    audio_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_idx < 0) {
        printf("Error: Find audio stream failed!\r\n");
        goto fail_find_audiostream;
    }else {
        printf("Find audio stream succeed!\r\n");
    }

    /* set audio decoder */
    const AVCodec *codec;
    codec = avcodec_find_decoder(ifmt_ctx->streams[audio_idx]->codecpar->codec_id);
    if(!codec){
        printf("Error: Find codec failed!\r\n");
        goto fail_find_codec;
    }
    printf("Find codec succeed!\r\n");

    AVCodecContext  *codec_ctx;
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("Error: Allocate audio codec context failed!\r\n");
        goto fail_find_codec_ctx;
    }
    printf("Allocate audio codec context succeed!\r\n");

    if(avcodec_parameters_to_context(codec_ctx, ifmt_ctx->streams[audio_idx]->codecpar) < 0) {
        printf("Error: Par 2 ctx failed!\r\n");
        goto fail_par_ctx;
    }
    printf("Par 2 ctx succeed!\r\n");

    if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Error: Open codec failed!\r\n");
        goto fail_open_codec;
    }
    printf("Open codec succeed!\r\n");

    AVFrame *frame = NULL;
    frame = av_frame_alloc();
    if(!frame)
    {
        printf("Error: Init frame failed!\r\n");
        goto fail_init_frame;
    }
    printf("Allocate frame succeed!\r\n");

    AVPacket *packet = av_packet_alloc();

    int ret;
    loop_state_flag = LOOP_RUN;

    printf("Start play!\r\n");

    while (loop_state_flag == LOOP_RUN) {
        if(av_read_frame(ifmt_ctx, packet) < 0) {
            printf("Error: Read packet failed!\r\n");
            break;
        }
        /* decode */
        if (packet->stream_index == audio_idx) {
            /* is a audio frame */
            ret = avcodec_send_packet(codec_ctx, packet);
            if(ret < 0) {
                printf("Error: Send packet to the decoder failed!\r\n");
                av_packet_unref(packet); /* free memory */
                break;
            }

            /* send to decoder success */
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // av_packet_unref(packet); /* free memory */
                    break;
                }else if(ret < 0){
                    printf("Error: Send packet to the decoder failed!\r\n");
                    av_packet_unref(packet); /* free memory */
                    goto loop_error;
                }

                if (ret >= 0) {
                    snd_pcm_writei(handle, frame->data[0], frame->nb_samples); /* dont forget [0] */
                    av_packet_unref(packet); /* free memory */
                }
            }
        }else {
            /* is not a audio frame */
            av_packet_unref(packet); /* free memory */
        }
    }

loop_error:
    av_packet_free(&packet);
    av_frame_free(&frame);
    frame = NULL;
fail_init_frame:
fail_open_codec:
    avcodec_free_context(&codec_ctx);
fail_par_ctx:
fail_find_codec_ctx:
fail_find_codec:
fail_find_audiostream:
fail_get_streaminfo:
    avformat_close_input(&ifmt_ctx);
    ifmt_ctx = NULL;
fail_open_stream:
fail_set_para:
fail_open_audiodevice:
    /* pass the remaining samples, otherwise they're dropped in close */
    if (snd_pcm_drain(handle) < 0) {
        printf("Error: snd_pcm_drain failed!\r\n");
    }
    snd_pcm_close(handle);
    printf ("Normal exit succeed!\r\n");
}
