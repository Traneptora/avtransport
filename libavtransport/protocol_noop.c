/*
 * Copyright © 2024, Lynne
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "protocol_common.h"
#include "io_common.h"

struct AVTProtocolCtx {
    const AVTIO *io;
    AVTIOCtx *io_ctx;
};

static int noop_init(AVTContext *ctx, AVTProtocolCtx **p, AVTAddress *addr)
{
    AVTProtocolCtx *priv = malloc(sizeof(*priv));
    if (!priv)
        return AVT_ERROR(ENOMEM);

    int err = avt_io_init(ctx, &priv->io, &priv->io_ctx, addr);
    if (err < 0)
        free(priv);
    else
        *p = priv;

    return err;
}

static int noop_add_dst(AVTContext *ctx, AVTProtocolCtx *p, AVTAddress *addr)
{
    if (!p->io->add_dst)
        return AVT_ERROR(ENOTSUP);
    return p->io->add_dst(ctx, p->io_ctx, addr);
}

static int noop_rm_dst(AVTContext *ctx, AVTProtocolCtx *p, AVTAddress *addr)
{
    if (!p->io->del_dst)
        return AVT_ERROR(ENOTSUP);
    return p->io->del_dst(ctx, p->io_ctx, addr);
}

static int64_t noop_send_packet(AVTContext *ctx, AVTProtocolCtx *p,
                                union AVTPacketData pkt, AVTBuffer *pl)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    size_t hdr_len = 0;

    // TODO

    return p->io->write_output(ctx, p->io_ctx, hdr, hdr_len, pl);
}

static int64_t noop_send_packets(AVTContext *ctx, AVTProtocolCtx *p,
                                 AVTPacketFifo *seq)
{
    uint8_t hdr[AVT_MAX_HEADER_LEN];
    size_t hdr_len = 0;

    // TODO

    return 0;
}

static int noop_receive_packet(AVTContext *ctx, AVTProtocolCtx *p,
                               union AVTPacketData *pkt, AVTBuffer **pl)
{
    AVTBuffer *buf;
    int err = p->io->read_input(ctx, p->io_ctx, &buf, 0);
    if (err < 0)
        return err;

    // TODO

    return 0;
}

static uint32_t noop_max_pkt_len(AVTContext *ctx, AVTProtocolCtx *p)
{
    return p->io->get_max_pkt_len(ctx, p->io_ctx);
}

static int64_t noop_seek(AVTContext *ctx, AVTProtocolCtx *p,
                         int64_t off, uint32_t seq,
                         int64_t ts, bool ts_is_dts)
{
    return p->io->seek(ctx, p->io_ctx, off);
}

static int noop_flush(AVTContext *ctx, AVTProtocolCtx *p)
{
    return p->io->flush(ctx, p->io_ctx);
}

static int noop_close(AVTContext *ctx, AVTProtocolCtx **p)
{
    AVTProtocolCtx *priv = *p;
    int err = priv->io->close(ctx, &priv->io_ctx);
    free(priv);
    *p = NULL;
    return err;
}

const AVTProtocol avt_protocol_noop = {
    .name = "noop",
    .type = AVT_PROTOCOL_NOOP,
    .init = noop_init,
    .add_dst = noop_add_dst,
    .rm_dst = noop_rm_dst,
    .get_max_pkt_len = noop_max_pkt_len,
    .receive_packet = noop_receive_packet,
    .send_packet = noop_send_packet,
    .seek = noop_seek,
    .flush = noop_flush,
    .close = noop_close,
};
