/*
 * Xiph PCM parser for Ogg
 * Copyright (c) 2012 Jasper St. Pierre
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>

#include "libavutil/intreadwrite.h"
#include "avformat.h"
#include "internal.h"
#include "oggdec.h"

struct oggpcm_private {
    int extra_headers_left;
};

static int pcm_codec_map[] = {
    AV_CODEC_ID_PCM_S8,    /* OGGPCM_FMT_S8       = 0x00000000 */
    AV_CODEC_ID_PCM_U8,    /* OGGPCM_FMT_U8       = 0x00000001 */
    AV_CODEC_ID_PCM_S16LE, /* OGGPCM_FMT_S16_LE   = 0x00000002 */
    AV_CODEC_ID_PCM_S16BE, /* OGGPCM_FMT_S16_BE   = 0x00000003 */
    AV_CODEC_ID_PCM_S24LE, /* OGGPCM_FMT_S24_LE   = 0x00000004 */
    AV_CODEC_ID_PCM_S24BE, /* OGGPCM_FMT_S24_BE   = 0x00000005 */
    AV_CODEC_ID_PCM_S32LE, /* OGGPCM_FMT_S32_LE   = 0x00000006 */
    AV_CODEC_ID_PCM_S32BE, /* OGGPCM_FMT_S32_BE   = 0x00000007 */

    -1,                    /* none                = 0x00000008 */
    -1,                    /* none                = 0x00000009 */
    -1,                    /* none                = 0x0000000A */
    -1,                    /* none                = 0x0000000B */
    -1,                    /* none                = 0x0000000C */
    -1,                    /* none                = 0x0000000D */
    -1,                    /* none                = 0x0000000E */
    -1,                    /* none                = 0x0000000F */

    /* Compressed PCM */
    AV_CODEC_ID_PCM_MULAW, /* OGGPCM_FMT_ULAW     = 0x00000010 */
    AV_CODEC_ID_PCM_ALAW,  /* OGGPCM_FMT_ALAW     = 0x00000011 */

    -1,                    /* none                = 0x00000012 */
    -1,                    /* none                = 0x00000013 */
    -1,                    /* none                = 0x00000014 */
    -1,                    /* none                = 0x00000015 */
    -1,                    /* none                = 0x00000016 */
    -1,                    /* none                = 0x00000017 */
    -1,                    /* none                = 0x00000018 */
    -1,                    /* none                = 0x00000019 */
    -1,                    /* none                = 0x0000001A */
    -1,                    /* none                = 0x0000001B */
    -1,                    /* none                = 0x0000001C */
    -1,                    /* none                = 0x0000001D */
    -1,                    /* none                = 0x0000001E */
    -1,                    /* none                = 0x0000001F */

    /* IEEE Floating point Coding */
    AV_CODEC_ID_PCM_F32LE, /* OGGPCM_FMT_FLT32_LE = 0x00000020 */
    AV_CODEC_ID_PCM_F32BE, /* OGGPCM_FMT_FLT32_BE = 0x00000021 */
    AV_CODEC_ID_PCM_F64LE, /* OGGPCM_FMT_FLT64_LE = 0x00000022 */
    AV_CODEC_ID_PCM_F64BE, /* OGGPCM_FMT_FLT64_BE = 0x00000023 */
};

static int pcm_header(AVFormatContext *s, int idx)
{
    struct ogg *ogg = s->priv_data;
    struct ogg_stream *os = ogg->streams + idx;
    AVStream *st = s->streams[idx];
    uint8_t *p = os->buf + os->pstart;
    struct oggpcm_private *priv = os->private;

    if (!priv) {
        /* Main header */

        int codec_id;
        uint16_t major_version;
        uint32_t sample_rate, format, extra_headers;
        uint8_t nb_channels;

        major_version    = AV_RL16(p + 8);
        /* minor version skipped */
        if (major_version != 0)
            return AVERROR_INVALIDDATA;

        format            = AV_RL32(p + 12);
        sample_rate       = AV_RL32(p + 16);
        /* significant bits skipped */
        nb_channels       = AV_RL8(p + 21);
        /* frames per packet skipped (this is calculated manually by libav) */
        extra_headers     = AV_RL32(p + 24);

        st->codec->codec_type = AVMEDIA_TYPE_AUDIO;
        codec_id = pcm_codec_map[format];
        if (codec_id < 0) {
            return AVERROR_INVALIDDATA;
        }
        st->codec->codec_id = codec_id;

        st->codec->sample_rate = sample_rate;
        st->codec->channels = nb_channels;

        avpriv_set_pts_info(st, 64, 1, st->codec->sample_rate);

        priv = av_malloc(sizeof(struct oggpcm_private));
        if (!priv)
            return AVERROR(ENOMEM);

        os->private = priv;
        /* Add one for the comment header that's
         * always there. */
        priv->extra_headers_left = extra_headers + 1;
        return 1;
    } else if (priv->extra_headers_left > 0) {
        /* Extra headers (vorbiscomment) */

        ff_vorbis_comment(s, &st->metadata, p, os->psize);
        priv->extra_headers_left --;
        return 1;
    } else {
        return 0;
    }
}

const struct ogg_codec ff_pcm_codec = {
    .magic     = "PCM      ",
    .magicsize = 8,
    .header    = pcm_header,
};
