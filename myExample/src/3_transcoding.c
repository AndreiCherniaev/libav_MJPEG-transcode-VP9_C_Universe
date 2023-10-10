#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libavutil/opt.h>
#include <string.h>
#include <inttypes.h>
#include "video_debugging.h"

typedef struct StreamingParams {
    char *output_extension;
    char *muxer_opt_key;
    char *muxer_opt_value;
    //char *video_codec;
    //char *audio_codec;
    char *codec_priv_key;
    char *codec_priv_value;
} StreamingParams;

typedef struct StreamingContext {
    AVFormatContext *avfc;
    AVCodec *video_avc;
    AVStream *video_avs;
    AVCodecContext *video_avcc;
    int video_index;
    char *filename;
} StreamingContext;

StreamingContext *decoder;
StreamingContext *encoder;

int fill_stream_info(AVStream *avs, AVCodec **avc, AVCodecContext **avcc) {
    *avc = avcodec_find_decoder(avs->codecpar->codec_id);
    if (!*avc) {logging("failed to find the codec"); return -1;}

    *avcc = avcodec_alloc_context3(*avc);
    if (!*avcc) {logging("failed to alloc memory for codec context"); return -1;}

    if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) {logging("failed to fill codec context"); return -1;}

    if (avcodec_open2(*avcc, *avc, NULL) < 0) {logging("failed to open codec"); return -1;}
    return 0;
}

int open_media(const char *in_filename, AVFormatContext **avfc) {
    *avfc = avformat_alloc_context();
    if (!*avfc) {logging("failed to alloc memory for format"); return -1;}

    if (avformat_open_input(avfc, in_filename, NULL, NULL) != 0) {logging("failed to open input file %s", in_filename); return -1;}

    if (avformat_find_stream_info(*avfc, NULL) < 0) {logging("failed to get stream info"); return -1;}
    return 0;
}

int prepare_decoder(StreamingContext *sc) {
    for (int i = 0; i < sc->avfc->nb_streams; i++) {
        if (sc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            sc->video_avs = sc->avfc->streams[i];
            sc->video_index = i;

            if (fill_stream_info(sc->video_avs, &sc->video_avc, &sc->video_avcc)) {return -1;}
        } else if (sc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        } else {
            logging("skipping streams other than audio and video");
        }
    }

    return 0;
}

static int open_output_file(const char *filename)
{
    //AVStream *out_stream; = encoder->video_avs
    //AVStream *in_stream; = decoder->video_avs
    //AVCodecContext *dec_ctx, *enc_ctx;
    //const AVCodec *reg_encoder;//encoder->video_avc
    int ret;

    encoder->avfc = NULL;//encoder->avfc
    avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, filename);
    if (!encoder->avfc) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    encoder->video_avs = avformat_new_stream(encoder->avfc, NULL);
    if (!encoder->video_avs) {
        av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
        return AVERROR_UNKNOWN;
    }
    //ifmt_ctx = decoder->avfc
    //dec_ctx = decoder->video_avcc;//stream_ctx[0].dec_ctx;
    if (decoder->video_avcc->codec_type == AVMEDIA_TYPE_VIDEO
        || decoder->video_avcc->codec_type == AVMEDIA_TYPE_AUDIO) {
        /* in this example, we choose transcoding to same codec */
        encoder->video_avc = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder->video_avc) {
            av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
            return AVERROR_INVALIDDATA;
        }
        //enc_ctx = encoder->video_avcc
        encoder->video_avcc = avcodec_alloc_context3(encoder->video_avc);
        if (!encoder->video_avcc) {
            av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
            return AVERROR(ENOMEM);
        }

        /* In this example, we transcode to same properties (picture size,
         * sample rate etc.). These properties can be changed for output
         * streams easily using filters */
        if (decoder->video_avcc->codec_type == AVMEDIA_TYPE_VIDEO) {
            encoder->video_avcc->height = decoder->video_avcc->height;
            encoder->video_avcc->width = decoder->video_avcc->width;
            encoder->video_avcc->sample_aspect_ratio = decoder->video_avcc->sample_aspect_ratio;
            /* take first format from list of supported formats */
            if (encoder->video_avc->pix_fmts)
                encoder->video_avcc->pix_fmt = encoder->video_avc->pix_fmts[0];
            else
                encoder->video_avcc->pix_fmt = AV_PIX_FMT_YUV420P;//dec_ctx->pix_fmt;
            /* video time_base can be set to whatever is handy and supported by encoder */
            //encoder->video_avcc->time_base = av_inv_q(decoder->video_avcc->framerate);
            encoder->video_avcc->color_primaries= AVCOL_PRI_BT470BG;
            //enc_ctx->bit_rate= 2000000; //means
        }

        if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
            encoder->video_avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        /* Third parameter can be used to pass settings to encoder */
        AVDictionary* opt = NULL;
        av_dict_set(&opt, "c", "libopenh264", 0);//c:v
        av_dict_set(&opt, "profile", "high", 0);
        av_dict_set(&opt, "b", "4000000", 0);//b:v
        av_dict_set(&opt, "allow_skip_frames", "1", 0);
        av_dict_set(&opt, "maxrate", "7500000", 0);

        AVRational input_framerate = {6666, 1}; //for test
        encoder->video_avcc->time_base = av_inv_q(input_framerate); //tbc = the time base in AVCodecContext for the codec used for a particular stream
        encoder->video_avs->time_base = encoder->video_avcc->time_base; //tbn = the time base in AVStream that has come from the container

        //Works ok with input.yuvj422p
        /*encoder->video_avs->time_base = decoder->video_avs->time_base;
        printf("decoder time_base %u %u", decoder->video_avs->time_base.num, decoder->video_avs->time_base.den); //1 25 */

        ret = avcodec_open2(encoder->video_avcc, encoder->video_avc, &opt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", 0);
            return ret;
        }
        ret = avcodec_parameters_from_context(encoder->video_avs->codecpar, encoder->video_avcc);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", 0);
            return ret;
        }

        //stream_ctx[0].enc_ctx = encoder->video_avcc;
    } else if (decoder->video_avcc->codec_type == AVMEDIA_TYPE_UNKNOWN) {
        av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", 0);
        return AVERROR_INVALIDDATA;
    } else {
        /* if this stream must be remuxed */
        ret = avcodec_parameters_copy(encoder->video_avs->codecpar, decoder->video_avs->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", 0);
            return ret;
        }
    }

    av_dump_format(encoder->avfc, 0, filename, 1);

    if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&encoder->avfc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(encoder->avfc, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

int prepare_copy(AVFormatContext *avfc, AVStream **avs, AVCodecParameters *decoder_par) {
    *avs = avformat_new_stream(avfc, NULL);
    avcodec_parameters_copy((*avs)->codecpar, decoder_par);
    return 0;
}


int encode_video(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame) {
    if (input_frame) input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {logging("could not allocate memory for output packet"); return -1;}

    int response = avcodec_send_frame(encoder->video_avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(encoder->video_avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            logging("Error while receiving packet from encoder: %s", av_err2str(response));
            return -1;
        }

        output_packet->stream_index = decoder->video_index;

        output_packet->pts = av_rescale_q_rnd(output_packet->pts, decoder->video_avs->time_base, encoder->video_avs->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        output_packet->dts = av_rescale_q_rnd(output_packet->dts, decoder->video_avs->time_base, encoder->video_avs->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        output_packet->duration = av_rescale_q(output_packet->duration, decoder->video_avs->time_base, encoder->video_avs->time_base);
        //output_packet->duration = encoder->video_avs->time_base.den / encoder->video_avs->time_base.num / decoder->video_avs->avg_frame_rate.num * decoder->video_avs->avg_frame_rate.den;

        AVRational input_framerate = {25, 1};
        AVRational input_framerate2 = {25, 1};
        av_packet_rescale_ts(output_packet, input_framerate, input_framerate2);
        response = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (response != 0) { logging("Error %d while receiving packet from decoder: %s", response, av_err2str(response)); return -1;}
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int transcode_video(StreamingContext *decoder, StreamingContext *encoder, AVPacket *input_packet, AVFrame *input_frame) {
    int response = avcodec_send_packet(decoder->video_avcc, input_packet);
    if (response < 0) {logging("Error while sending packet to decoder: %s", av_err2str(response)); return response;}

    while (response >= 0) {
        response = avcodec_receive_frame(decoder->video_avcc, input_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            logging("Error while receiving frame from decoder: %s", av_err2str(response));
            return response;
        }

        if (response >= 0) {
            if (encode_video(decoder, encoder, input_frame)) return -1;
        }
        av_frame_unref(input_frame);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    /*
   * H264 -> H265
   * Audio -> remuxed (untouched)
   * MP4 - MP4
   */
//    StreamingParams sp = {0};
//    sp.copy_audio = 1;
//    sp.copy_video = 0;
//    sp.video_codec = "libx265";
//    sp.codec_priv_key = "x265-params";
//    sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0";

    /*
   * H264 -> H264 (fixed gop)
   * Audio -> remuxed (untouched)
   * MP4 - MP4
   */
    StreamingParams sp = {0};
    //sp.video_codec = "libx264";
    //sp.codec_priv_key = "x264-params";
    //sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";

    /*
   * H264 -> H264 (fixed gop)
   * Audio -> remuxed (untouched)
   * MP4 - fragmented MP4
   */
    //StreamingParams sp = {0};
    //sp.copy_audio = 1;
    //sp.copy_video = 0;
    //sp.video_codec = "libx264";
    //sp.codec_priv_key = "x264-params";
    //sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
    //sp.muxer_opt_key = "movflags";
    //sp.muxer_opt_value = "frag_keyframe+empty_moov+delay_moov+default_base_moof";

    /*
   * H264 -> H264 (fixed gop)
   * Audio -> AAC
   * MP4 - MPEG-TS
   */
    //StreamingParams sp = {0};
    //sp.copy_audio = 0;
    //sp.copy_video = 0;
    //sp.video_codec = "libx264";
    //sp.codec_priv_key = "x264-params";
    //sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
    //sp.audio_codec = "aac";
    //sp.output_extension = ".ts";

    /*
   * H264 -> VP9
   * ffmpeg -i small_bunny_1080p_60fps.mp4 -c:v libvpx-vp9 -minrate 110k -b:v 2000k -maxrate 2800k -bsf vp9_superframe -an small_bunny_1080p_60fps.webm
   * MP4 - WebM
   */
//      StreamingParams sp = {0};
//      sp.video_codec = "libvpx-vp9";
//      sp.audio_codec = "vorbis"; //https://trac.ffmpeg.org/ticket/10571
//      sp.output_extension = ".webm";

    decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    //decoder->filename = "f5_20.yuvj422p";
    //decoder->filename = "/home/a/mystorage/mnt_sda2_pseudo/2.3D printing video_2023.08.23.yuvj422p"; //"input.yuvj422p"; //"small_bunny_1080p_60fps.mp4";
    decoder->filename = "input.yuvj422p"; //"small_bunny_1080p_60fps.mp4";

    encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    encoder->filename = "argv.mkv";

//    if (sp.output_extension)
//        strcat(encoder->filename, sp.output_extension);

    if (open_media(decoder->filename, &decoder->avfc)) return -1;
    if (prepare_decoder(decoder)) return -1;

    open_output_file(encoder->filename);
//    avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, encoder->filename);
//    if (!encoder->avfc) {logging("could not allocate memory for output format");return -1;}

//    AVRational input_framerate = av_guess_frame_rate(decoder->avfc, decoder->video_avs, NULL);
//    prepare_video_encoder(encoder, decoder->video_avcc, input_framerate, sp);

//    if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
//        encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

//    if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
//        if (avio_open(&encoder->avfc->pb, encoder->filename, AVIO_FLAG_WRITE) < 0) {
//            logging("could not open the output file");
//            return -1;
//        }
//    }



    AVDictionary* muxer_opts = NULL;

    if (sp.muxer_opt_key && sp.muxer_opt_value) {
        av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);
    }

    if (avformat_write_header(encoder->avfc, &muxer_opts) < 0) {logging("an error occurred when opening output file"); return -1;}

    AVFrame *input_frame = av_frame_alloc();
    if (!input_frame) {logging("failed to allocated memory for AVFrame"); return -1;}

    AVPacket *input_packet = av_packet_alloc();
    if (!input_packet) {logging("failed to allocated memory for AVPacket"); return -1;}

    while (av_read_frame(decoder->avfc, input_packet) >= 0)
    {
        if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // TODO: refactor to be generic for audio and video (receiving a function pointer to the differences)
            if (transcode_video(decoder, encoder, input_packet, input_frame)) return -1;
            av_packet_unref(input_packet);
        } else if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)  {
        } else {
            logging("ignoring all non video or audio packets");
        }
    }
    // TODO: should I also flush the audio encoder?
    if (encode_video(decoder, encoder, NULL)) return -1;

    av_write_trailer(encoder->avfc);

    if (muxer_opts != NULL) {
        av_dict_free(&muxer_opts);
        muxer_opts = NULL;
    }

    if (input_frame != NULL) {
        av_frame_free(&input_frame);
        input_frame = NULL;
    }

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    avformat_close_input(&decoder->avfc);

    avformat_free_context(decoder->avfc); decoder->avfc = NULL;
    avformat_free_context(encoder->avfc); encoder->avfc = NULL;

    avcodec_free_context(&decoder->video_avcc); decoder->video_avcc = NULL;

    free(decoder); decoder = NULL;
    free(encoder); encoder = NULL;
    return 0;
}

