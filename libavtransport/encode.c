/*
 * Copyright © 2023, Lynne
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>

#include "encode.h"

#include "../packet_encode.h"

int avt_output_session_start(AVTContext *ctx, AVTOutput *out)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));

    AVTSessionStart pkt = {
        .global_seq = atomic_fetch_add(&out->seq, 1ULL) & UINT32_MAX,
        .session_flags = 0x0,
        .producer_major = PROJECT_VERSION_MAJOR,
        .producer_minor = PROJECT_VERSION_MINOR,
        .producer_micro = PROJECT_VERSION_MICRO,
    };
    memcpy(pkt.producer_name, PROJECT_NAME, strlen(PROJECT_NAME));

    avt_encode_session_start(&bs, &pkt);

    return avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), NULL);
}

int avt_output_time_sync(AVTContext *ctx, AVTOutput *out)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));

    avt_encode_time_sync(&bs, &(AVTTimeSync){
        .ts_clock_id = 0,
        .ts_clock_hz2 = 0,
        .global_seq = atomic_fetch_add(&out->seq, 1ULL) & UINT32_MAX,
        .epoch = atomic_load(&out->epoch),
        .ts_clock_seq = 0,
        .ts_clock_hz = 0,
    });

    return avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), NULL);
}

int avt_output_stream_register(AVTContext *ctx, AVTOutput *out,
                               AVTStream *st)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));

    avt_encode_stream_registration(&bs, &(AVTStreamRegistration){
        .stream_id = st->id,
        .global_seq = atomic_fetch_add(&out->seq, 1ULL) & UINT32_MAX,
        .related_stream_id = st->related_to ? st->related_to->id : UINT16_MAX,
        .derived_stream_id = st->derived_from ? st->derived_from->id : UINT16_MAX,
        .bandwidth = st->bitrate,
        .stream_flags = st->flags,

        .codec_id = st->codec_id,
        .timebase = st->timebase,
        .ts_clock_id = 0,
        .skip_preroll = 0,
        .init_packets = 0,
    });

    return avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), NULL);
}

#include <stdio.h>

int avt_output_stream_data(AVTContext *ctx, AVTOutput *out,
                           AVTStream *st, AVTPacket *pkt)
{
    int err;
    size_t seg_len;
    size_t maxp = avt_packet_get_max_size(ctx, out);
    size_t payload_size = avt_buffer_get_data_len(pkt->data);
    size_t pbytes = payload_size;
    AVTBuffer tmp;

    seg_len = AVT_MIN(pbytes, maxp);

    avt_buffer_quick_ref(&tmp, pkt->data, 0, seg_len);

    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));
    uint32_t init_seq = atomic_fetch_add(&out->seq, 1ULL) & UINT32_MAX;
    avt_encode_stream_data(&bs, &(AVTStreamData){
        .frame_type = pkt->type,
        .pkt_segmented = seg_len < pbytes,
        .pkt_in_fec_group = 0,
        .field_id = 0,
        .pkt_compression = AVT_DATA_COMPRESSION_NONE,
        .stream_id = st->id,
        .global_seq = init_seq,
        .pts = pkt->pts,
        .duration = pkt->duration,
        .data_length = seg_len,
    });

    err = avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), &tmp);
    avt_buffer_quick_unref(&tmp);
    if (err < 0)
        return err;

    pbytes -= seg_len;

    uint8_t seg[AVT_MAX_HEADER_LEN];
    AVTBytestream bss = avt_bs_init(hdr, sizeof(hdr));
    while (pbytes) {
        const uint32_t seq = atomic_fetch_add(&out->seq, 1ULL) & UINT32_MAX;
        const int h7o = 4*(seq % 7);
        seg_len = AVT_MIN(pbytes, maxp);
        avt_buffer_quick_ref(&tmp, pkt->data, payload_size - pbytes, seg_len);
        avt_encode_generic_segment(&bss, &(AVTGenericSegment){
            .generic_segment_descriptor = AVT_PKT_STREAM_DATA_SEGMENT,
            .stream_id = st->id,
            .global_seq = seq,
            .target_seq = init_seq,
            .pkt_total_data = payload_size,
            .seg_offset = payload_size - pbytes,
            .seg_length = seg_len,
            .header_7 = { hdr[h7o + 0], hdr[h7o + 1], hdr[h7o + 2], hdr[h7o + 3] },
        });

        err = avt_packet_send(ctx, out, seg, avt_bs_offs(&bss), &tmp);
        avt_buffer_quick_unref(&tmp);
        if (err < 0)
            break;

        pbytes -= seg_len;
        avt_bs_reset(&bss);
    } while (pbytes);

    return err;
}

int avt_output_video_info(AVTContext *ctx, AVTOutput *out,
                          AVTStream *st)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));

    avt_encode_video_info(&bs, &st->video_info);

    return avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), NULL);
}

int avt_output_video_orientation(AVTContext *ctx, AVTOutput *out,
                                 AVTStream *st)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    AVTBytestream bs = avt_bs_init(hdr, sizeof(hdr));

    avt_encode_video_orientation(&bs, &st->video_orientation);

    return avt_packet_send(ctx, out, hdr, avt_bs_offs(&bs), NULL);
}

#if 0
static int pq_output_generic_segment(AVTContext *ctx, uint8_t *header,
                                     uint32_t header_pkt_seq, AVTStream *st,
                                     AVTBuffer *data, size_t offset, size_t max,
                                     uint16_t seg_desc, uint16_t term_desc)
{
    size_t len_tot = avt_buffer_get_data_len(data), len = len_tot, len_tgt;
    size_t off = offset;
    uint8_t h7 = 0;
    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))
    AVTBuffer tmp;
    int ret;

    do {
        PQ_RESET(bs);
        len_tgt = AVTMIN(len, max);

        pq_buffer_quick_ref(&tmp, data, off, len_tgt);

        PQ_WBL(bs, 16, len < max ? term_desc : seg_desc);
        PQ_WBL(bs, 16, st->id);
        PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
        PQ_WBL(bs, 32, header_pkt_seq);
        PQ_WBL(bs, 32, len_tot);
        PQ_WBL(bs, 32, off);
        PQ_WBL(bs, 32, AVTMIN(len, max));
        PQ_WBL(bs, 32, PQ_RB32(header + h7));
        PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

        ret = ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), &tmp);
        pq_buffer_quick_unref(&tmp);
        if (ret < 0) {
            return ret;
        }

        h7 = (h7 + 1) % 28;
        len -= len_tgt;
        off += len_tgt;
    } while (len > 0);

    return ret;
}

static int pq_output_generic_data(AVTContext *ctx, AVTStream *st,
                                  AVTBuffer *buf,
                                  uint16_t desc_full, uint16_t desc_part,
                                  uint16_t desc_seg, uint16_t desc_end)
{
    if (!buf)
        return 0;

    int ret;
    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))
    uint32_t seq;
    size_t data_len = avt_buffer_get_data_len(buf);
    if (data_len > UINT32_MAX)
        return AVT_ERROR(EINVAL);

    uint32_t mtu = ctx->dst.cb->max_pkt_len(ctx, ctx->dst.ctx);

    seq = atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed);

    PQ_WBL(bs, 16, data_len > mtu ? desc_part : desc_full);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, seq);
    PQ_WBL(bs, 32, AVTMIN(data_len, mtu));
    PQ_WPD(bs, 128);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    ret = ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), buf);
    if (ret < 0)
        return ret;

    if (data_len > mtu)
        ret = pq_output_generic_segment(ctx, hdr, seq, st, buf,
                                        AVTMIN(data_len, mtu), mtu,
                                        desc_seg, desc_end);

    return ret;
}

static int pq_output_video_info(AVTContext *ctx, AVTStream *st)
{
    AVTStreamVideoInfo *vi = &st->video_info;
    uint8_t hdr[356];
    uint8_t raptor2[80];
    PQ_INIT(bs, hdr, sizeof(hdr))

    PQ_WBL(bs, 16, AVT_PKT_STREAM_DURATION);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WBL(bs, 32, vi->width);
    PQ_WBL(bs, 32, vi->height);
    PQ_WBL(bs, 32, vi->signal_aspect.num);
    PQ_WBL(bs, 32, vi->signal_aspect.den);
    PQ_WBL(bs, 8,  vi->subsampling);
    PQ_WBL(bs, 8,  vi->colorspace);
    PQ_WBL(bs, 8,  vi->bit_depth);
    PQ_WBL(bs, 8,  vi->interlaced);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));
    PQ_WBL(bs, 32, vi->gamma.num);
    PQ_WBL(bs, 32, vi->gamma.den);
    PQ_WBL(bs, 32, vi->framerate.num);
    PQ_WBL(bs, 32, vi->framerate.den);
    PQ_WBL(bs, 16, vi->limited_range);
    PQ_WBL(bs, 8,  vi->chroma_pos);
    PQ_WBL(bs, 8,  vi->primaries);
    PQ_WBL(bs, 8,  vi->transfer);
    PQ_WBL(bs, 8,  vi->matrix);
    PQ_WBL(bs, 8,  vi->has_mastering_primaries);
    PQ_WBL(bs, 8,  vi->has_luminance);
    for (int i = 0; i < 16; i++) {
        PQ_WBL(bs, 32, vi->custom_matrix[i].num);
        PQ_WBL(bs, 32, vi->custom_matrix[i].den);
    }
    for (int i = 0; i < 6; i++) {
        PQ_WBL(bs, 32, vi->custom_primaries[i].num);
        PQ_WBL(bs, 32, vi->custom_primaries[i].den);
    }
    PQ_WBL(bs, 32, vi->white_point[0].num);
    PQ_WBL(bs, 32, vi->white_point[0].den);
    PQ_WBL(bs, 32, vi->white_point[1].num);
    PQ_WBL(bs, 32, vi->white_point[1].den);
    PQ_WBL(bs, 32, vi->min_luminance.num);
    PQ_WBL(bs, 32, vi->min_luminance.den);
    PQ_WBL(bs, 32, vi->max_luminance.num);
    PQ_WBL(bs, 32, vi->max_luminance.den);
    PQ_WDT(bs, pq_calc_raptor_short(PQ_GETLAST(bs, 240), raptor2, 240, 80), 80);

    return ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), NULL);
}

static int pq_output_stream_duration(AVTContext *ctx, AVTStream *st)
{
    if (!st->duration)
        return 0;

    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))

    PQ_WBL(bs, 16, AVT_PKT_STREAM_DURATION);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WBL(bs, 64, st->duration);
    PQ_WPD(bs, 96);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    return ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), NULL);
}

int avt_output_update_stream(AVTContext *ctx, AVTStream *st)
{
    if (!ctx->dst.ctx)
        return AVT_ERROR(EINVAL);

    int i;
    for (i = 0; i < ctx->nb_stream; i++)
        if (ctx->stream[i]->id == st->id)
            break;
    if (i == ctx->nb_stream)
        return AVT_ERROR(EINVAL);

    uint8_t hdr[56];
    PQ_INIT(bs, hdr, sizeof(hdr))

    PQ_WBL(bs, 16, AVT_PKT_STREAM_REG);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WBL(bs, 16, st->related_to ? st->related_to->id : st->id);
    PQ_WBL(bs, 16, st->derived_from ? st->derived_from->id : st->id);
    PQ_WBL(bs, 64, st->bitrate);
    PQ_WBL(bs, 64, st->flags);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));
    PQ_WBL(bs, 32, st->codec_id);
    PQ_WBL(bs, 32, st->timebase.num);
    PQ_WBL(bs, 32, st->timebase.den);
    PQ_WBL(bs, 64, pq_calc_raptor_160(PQ_GETLAST(bs, 12)));

    st->private->codec_id = st->codec_id;

    int ret = ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), NULL);
    if (ret < 0)
        return ret;

    ret = pq_output_stream_duration(ctx, st);
    if (ret < 0)
        return ret;

    ret = pq_output_video_info(ctx, st);
    if (ret < 0)
        return ret;

    ret = pq_output_generic_data(ctx, st, st->init_data,
                                 AVT_PKT_STREAM_INIT, AVT_PKT_STREAM_INIT_PRT,
                                 AVT_PKT_STREAM_INIT_SEG, AVT_PKT_STREAM_INIT_END);
    if (ret < 0)
        return ret;

    ret = pq_output_generic_data(ctx, st, st->icc_profile,
                                 AVT_PKT_ICC, AVT_PKT_ICC_PRT,
                                 AVT_PKT_ICC_SEG, AVT_PKT_ICC_END);
    if (ret < 0)
        return ret;

    return ret;
}

int avt_output_write_stream_data(AVTContext *ctx, AVTStream *st,
                                AVTPacket *pkt)
{
    if (!ctx->dst.ctx)
        return AVT_ERROR(EINVAL);

    int ret;
    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))
    uint32_t seq;
    size_t data_len = avt_buffer_get_data_len(pkt->data);
    if (data_len > UINT32_MAX)
        return AVT_ERROR(EINVAL);

    uint32_t mtu = ctx->dst.cb->max_pkt_len(ctx, ctx->dst.ctx);

    uint16_t desc;
    desc = AVT_PKT_STREAM_DATA & 0xFF00;
    desc |= data_len > mtu ? 0x20 : 0x0;
    desc |= (pkt->type & 0xC0);
    seq = atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed);

    PQ_WBL(bs, 16, desc);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, seq);
    PQ_WBL(bs, 64, pkt->pts);
    PQ_WBL(bs, 64, pkt->duration);
    PQ_WBL(bs, 32, AVTMIN(data_len, mtu));
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    ret = ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), pkt->data);
    if (ret < 0)
        return ret;

    if (data_len > mtu)
        ret = pq_output_generic_segment(ctx, hdr, seq, st, pkt->data,
                                        AVTMIN(data_len, mtu), mtu,
                                        AVT_PKT_STREAM_SEG_DATA,
                                        AVT_PKT_STREAM_SEG_END);

    return 0;
}

int avt_output_write_user_data(AVTContext *ctx, AVTBuffer *data,
                              uint8_t descriptor_flags, uint16_t user,
                              int prioritize)
{
    if (!ctx->dst.ctx)
        return AVT_ERROR(EINVAL);

    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))
    uint16_t desc = AVT_PKT_USER_DATA & 0xFF00;

    desc |= descriptor_flags;

    PQ_WBL(bs, 16, desc);
    PQ_WBL(bs, 16, user);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WBL(bs, 32, avt_buffer_get_data_len(data));
    PQ_WPD(bs, 128);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    return ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), data);
}

int avt_output_close_stream(AVTContext *ctx, AVTStream *st)
{
    if (!ctx->dst.ctx)
        return AVT_ERROR(EINVAL);

    int i;
    for (i = 0; i < ctx->nb_stream; i++)
        if (ctx->stream[i]->id == st->id)
            break;
    if (i == ctx->nb_stream)
        return AVT_ERROR(EINVAL);

    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))

    PQ_WBL(bs, 16, AVT_PKT_EOS);
    PQ_WBL(bs, 16, st->id);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WPD(bs, 160);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    free(st->private);
    free(st);

    memmove(&ctx->stream[i], &ctx->stream[i+1], sizeof(*ctx->stream)*(ctx->nb_stream - i));
    ctx->nb_stream--;

    return ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), NULL);
}

int avt_output_close(AVTContext *ctx)
{
    if (!ctx->dst.ctx)
        return AVT_ERROR(EINVAL);

    uint8_t hdr[36];
    PQ_INIT(bs, hdr, sizeof(hdr))

    PQ_WBL(bs, 16, AVT_PKT_EOS);
    PQ_WBL(bs, 16, 0xFFFF);
    PQ_WBL(bs, 32, atomic_fetch_add_explicit(&ctx->dst.seq, 1, memory_order_relaxed));
    PQ_WPD(bs, 160);
    PQ_WBL(bs, 64, pq_calc_raptor_224(PQ_GETLAST(bs, 28)));

    int ret  = ctx->dst.cb->output(ctx, ctx->dst.ctx, hdr, PQ_WLEN(bs), NULL);
    int ret2 = ctx->dst.cb->close(ctx, &ctx->dst.ctx);

    return ret < 0 ? ret : ret2;
}

uint32_t pq_unlim_pkt_len(AVTContext *ctx, PQOutputContext *pc)
{
    return UINT32_MAX;
}
#endif
