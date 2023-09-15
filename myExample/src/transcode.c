/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 * Copyright (c) 2023 Andrei Cherniaev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file demuxing, decoding, filtering, encoding and muxing API usage example
 * @example transcode.c
 *
 * Convert input to output file, applying some hard-coded filter-graph on video stream.
 * This code based on official example https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcode.c
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;

    AVPacket *enc_pkt;
    AVFrame *filtered_frame;
} FilteringContext;
static FilteringContext *filter_ctx;

typedef struct StreamContext {
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;

    AVFrame *dec_frame;
} StreamContext;
static StreamContext *stream_ctx;
StreamContext *stream;
AVPacket *packet = NULL;

static int open_input_file(const char *filename)
{
    int ret;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    const int stream_mapping_size = ifmt_ctx->nb_streams; //should be 1
    if(stream_mapping_size!= 1){
        fprintf(stderr, "err Too much streams in inFile\n");
        ret = AVERROR_UNKNOWN;
        return AVERROR(ret);
    }

    stream_ctx = av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    AVStream *stream = ifmt_ctx->streams[0];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext *codec_ctx;
    if (!dec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", 0);
        return AVERROR_DECODER_NOT_FOUND;
    }
    codec_ctx = avcodec_alloc_context3(dec);
    if (!codec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", 0);
        return AVERROR(ENOMEM);
    }
    ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                                   "for stream #%u\n", 0);
        return ret;
    }

    /* Inform the decoder about the timebase for the packet timestamps.
     * This is highly recommended, but not mandatory. */
    codec_ctx->pkt_timebase = stream->time_base;

    /* Reencode video & audio and remux subtitles etc. */
    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
        || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
        /* Open decoder */
        ret = avcodec_open2(codec_ctx, dec, NULL);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", 0);
            return ret;
        }
    }
    stream_ctx[0].dec_ctx = codec_ctx;

    stream_ctx[0].dec_frame = av_frame_alloc();
    if (!stream_ctx[0].dec_frame)
        return AVERROR(ENOMEM);

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    const AVCodec *encoder;
    int ret;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    out_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!out_stream) {
        av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
        return AVERROR_UNKNOWN;
    }

    in_stream = ifmt_ctx->streams[0];
    dec_ctx = stream_ctx[0].dec_ctx;

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
        || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        /* in this example, we choose transcoding to same codec */
        encoder = avcodec_find_encoder(AV_CODEC_ID_VP9);
        if (!encoder) {
            av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
            return AVERROR_INVALIDDATA;
        }
        enc_ctx = avcodec_alloc_context3(encoder);
        if (!enc_ctx) {
            av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
            return AVERROR(ENOMEM);
        }

        /* In this example, we transcode to same properties (picture size,
         * sample rate etc.). These properties can be changed for output
         * streams easily using filters */
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            enc_ctx->height = dec_ctx->height;
            enc_ctx->width = dec_ctx->width;
            enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
            /* take first format from list of supported formats */
            if (encoder->pix_fmts)
                enc_ctx->pix_fmt = encoder->pix_fmts[0];
            else
                enc_ctx->pix_fmt = dec_ctx->pix_fmt;
            /* video time_base can be set to whatever is handy and supported by encoder */
            enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
            //enc_ctx->color_primaries= AVCOL_PRI_BT709;
            //enc_ctx->bit_rate= 2000000; //means
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        AVDictionary* opt = NULL;
        av_dict_set(&opt, "crf", "20", 0);
        /* Third parameter can be used to pass settings to encoder */
        ret = avcodec_open2(enc_ctx, encoder, &opt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", 0);
            return ret;
        }
        ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", 0);
            return ret;
        }

        out_stream->time_base = enc_ctx->time_base;
        stream_ctx[0].enc_ctx = enc_ctx;
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
        av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", 0);
        return AVERROR_INVALIDDATA;
    } else {
        /* if this stream must be remuxed */
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", 0);
            return ret;
        }
        out_stream->time_base = in_stream->time_base;
    }

    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                       AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                 dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
                 dec_ctx->sample_aspect_ratio.num,
                 dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                             (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    }else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(void)
{
    const char *filter_spec;
    int ret;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);

    filter_ctx[0].buffersrc_ctx  = NULL;
    filter_ctx[0].buffersink_ctx = NULL;
    filter_ctx[0].filter_graph   = NULL;

    if (ifmt_ctx->streams[0]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        filter_spec = "null"; /* passthrough (dummy) filter for video */
    else
        filter_spec = "anull"; /* passthrough (dummy) filter for audio */
    ret = init_filter(&filter_ctx[0], stream_ctx[0].dec_ctx,
                      stream_ctx[0].enc_ctx, filter_spec);
    if (ret)
        return ret;

    filter_ctx[0].enc_pkt = av_packet_alloc();
    if (!filter_ctx[0].enc_pkt)
        return AVERROR(ENOMEM);

    filter_ctx[0].filtered_frame = av_frame_alloc();
    if (!filter_ctx[0].filtered_frame)
        return AVERROR(ENOMEM);
    return 0;
}

static int encode_write_frame(int flush)
{
    FilteringContext *filter = &filter_ctx[0];
    AVFrame *filt_frame = flush ? NULL : filter->filtered_frame;
    AVPacket *enc_pkt = filter->enc_pkt;
    int ret;

    av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    av_packet_unref(enc_pkt);

    if (filt_frame && filt_frame->pts != AV_NOPTS_VALUE)
        filt_frame->pts = av_rescale_q(filt_frame->pts, filt_frame->time_base,
                                       stream->enc_ctx->time_base);

    ret = avcodec_send_frame(stream->enc_ctx, filt_frame);

    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        /* prepare packet for muxing */
        enc_pkt->stream_index = 0;
        av_packet_rescale_ts(enc_pkt,
                             stream->enc_ctx->time_base,
                             ofmt_ctx->streams[0]->time_base);

        av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
        /* mux encoded frame */
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame *frame)
{
    FilteringContext *filter = &filter_ctx[0];
    int ret;

    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx,
                                       frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter->buffersink_ctx,
                                      filter->filtered_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            break;
        }

        filter->filtered_frame->time_base = av_buffersink_get_time_base(filter->buffersink_ctx);;
        filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(0);
        av_frame_unref(filter->filtered_frame);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder()
{
    if (!(stream_ctx[0].enc_ctx->codec->capabilities &
          AV_CODEC_CAP_DELAY))
        return 0;

    av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", 0);
    return encode_write_frame(1);
}

int main(int argc, char **argv)
{
    int ret;
    unsigned int stream_index;

    //    if (argc != 3) {
    //        av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
    //        return 1;
    //    }
    const char * const in_filename= "input.yuvj422p";
    const char * const out_filename = "VideoOut.webm";

    if ((ret = open_input_file(in_filename)) < 0)
        goto end;
    if ((ret = open_output_file(out_filename)) < 0)
        goto end;
    if ((ret = init_filters()) < 0)
        goto end;
    if (!(packet = av_packet_alloc()))
        goto end;

    stream = &stream_ctx[0];

    /* read all packets */
    while (1) {
        ret = av_read_frame(ifmt_ctx, packet);
        if (ret < 0){
            break;
        }
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
               stream_index);

        av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");

        ret = avcodec_send_packet(stream->dec_ctx, packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
            break;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                break;
            else if (ret < 0)
                goto end;

            stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
            ret = filter_encode_write_frame(stream->dec_frame);
            if (ret < 0)
                goto end;
        }
        av_packet_unref(packet);
    }

    /* flush decoders, filters and encoders */
    av_log(NULL, AV_LOG_INFO, "Flushing stream %u decoder\n", 0);

    /* flush decoder */
    ret = avcodec_send_packet(stream->dec_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Flushing decoding failed\n");
        goto end;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
        if (ret == AVERROR_EOF)
            break;
        else if (ret < 0)
            goto end;

        stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
        ret = filter_encode_write_frame(stream->dec_frame);
        if (ret < 0)
            goto end;
    }

    /* flush filter */
    ret = filter_encode_write_frame(NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
        goto end;
    }

    /* flush encoder */
    ret = flush_encoder();
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
        goto end;
    }

    av_write_trailer(ofmt_ctx);
end:
    av_packet_free(&packet);
    avcodec_free_context(&stream_ctx[0].dec_ctx);
    if (ofmt_ctx && ofmt_ctx->nb_streams > 0 && ofmt_ctx->streams[0] && stream_ctx[0].enc_ctx)
        avcodec_free_context(&stream_ctx[0].enc_ctx);
    if (filter_ctx && filter_ctx[0].filter_graph) {
        avfilter_graph_free(&filter_ctx[0].filter_graph);
        av_packet_free(&filter_ctx[0].enc_pkt);
        av_frame_free(&filter_ctx[0].filtered_frame);
    }
    av_frame_free(&stream_ctx[0].dec_frame);

    av_free(filter_ctx);
    av_free(stream_ctx);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
