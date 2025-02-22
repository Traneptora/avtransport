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

#include <string.h>
#include <stdckdint.h>
#include <avtransport/avtransport.h>

#include "utils_internal.h"

uint64_t avt_get_time_ns(void)
{
    struct timespec ts;

    if (!timespec_get(&ts, TIME_UTC)) {
        avt_log(NULL, AVT_LOG_WARN, "Unable to get current time, assuming zero!\n");
        return 0;
    }

    return (ts.tv_sec * 1000000000) + ts.tv_nsec;
}

void avt_pkt_fifo_clear(AVTPacketFifo *fifo)
{
    for (int i = 0; i < fifo->nb; i++) {
        AVTOutputPacket *data = &fifo->data[i];
        avt_buffer_quick_unref(&data->pl);
    }
    fifo->nb = 0;
}

void avt_pkt_fifo_free(AVTPacketFifo *fifo)
{
    avt_pkt_fifo_clear(fifo);
    free(fifo->data);
    memset(fifo, 0, sizeof(*fifo));
}

int avt_pkt_fifo_push(AVTPacketFifo *fifo,
                      union AVTPacketData pkt, AVTBuffer *pl)
{
    if ((fifo->nb + 1) >= fifo->alloc) {
        if (!fifo->alloc)
            fifo->alloc = 1;

        /* Ptwo allocations */
        AVTOutputPacket *alloc = reallocarray(fifo->data,
                                              fifo->alloc << 1,
                                              sizeof(*fifo->data));
        if (!alloc)
            return AVT_ERROR(ENOMEM);

        fifo->data = alloc;
        fifo->alloc <<= 1;
    }

    AVTOutputPacket *data = &fifo->data[fifo->nb];
    int err = avt_buffer_quick_ref(&data->pl, pl, 0, 0);
    data->pkt = pkt;
    if (err < 0)
        fifo->nb++;

    return err;
}

int avt_pkt_fifo_copy(AVTPacketFifo *dst, AVTPacketFifo *src)
{
    if ((dst->nb + src->nb) >= dst->alloc) {
        if (!dst->alloc)
            dst->alloc = src->nb;

        AVTOutputPacket *alloc = reallocarray(dst->data,
                                              dst->alloc + src->nb,
                                              sizeof(*dst->data));
        if (!alloc)
            return AVT_ERROR(ENOMEM);

        dst->data = alloc;
        dst->alloc += src->nb;
    }

    for (int i = 0; i < src->nb; i++) {
        AVTOutputPacket *pdst = &dst->data[dst->nb + i];
        AVTOutputPacket *psrc = &src->data[i];
        *psrc = *pdst;
        int err = avt_buffer_quick_ref(&pdst->pl, &psrc->pl, 0, 0);
        if (err < 0) {
            dst->nb += 1;
            return err;
        }
    }

    dst->nb += src->nb;

    return 0;
}

int avt_pkt_fifo_move(AVTPacketFifo *dst, AVTPacketFifo *src)
{
    if ((dst->nb + src->nb) >= dst->alloc) {
        if (!dst->alloc)
            dst->alloc = src->nb;

        AVTOutputPacket *alloc = reallocarray(dst->data,
                                              dst->alloc + src->nb,
                                              sizeof(*dst->data));
        if (!alloc)
            return AVT_ERROR(ENOMEM);

        dst->data = alloc;
        dst->alloc += src->nb;
    }

    memcpy(&dst->data[dst->nb], src->data, src->nb*sizeof(*dst->data));
    dst->nb += src->nb;
    avt_pkt_fifo_clear(src);

    return 0;
}

int avt_pkt_fifo_peek(AVTPacketFifo *fifo,
                      union AVTPacketData *pkt, AVTBuffer *pl)
{
    if (!fifo->nb)
        return AVT_ERROR(ENOENT);

    AVTOutputPacket *data = &fifo->data[0];

    *pkt = data->pkt;
    return avt_buffer_quick_ref(pl, &data->pl, 0, 0);
}

int avt_pkt_fifo_pop(AVTPacketFifo *fifo,
                     union AVTPacketData *pkt, AVTBuffer *pl)
{
    if (!fifo->nb)
        return AVT_ERROR(ENOENT);

    AVTOutputPacket *data = &fifo->data[0];

    int err = avt_buffer_quick_ref(pl, &data->pl, 0, 0);
    avt_buffer_quick_unref(&data->pl);
    *pkt = data->pkt;

    fifo->nb--;
    memmove(fifo->data, fifo->data + 1, fifo->nb*sizeof(*fifo->data));

    return err;
}

static inline size_t avt_pkt_fifo_get_entry_size(AVTOutputPacket *e)
{
    return sizeof(e) + avt_buffer_get_data_len(&e->pl);
}

int avt_pkt_fifo_drop(AVTPacketFifo *fifo, unsigned int nb_pkts, size_t ceiling)
{
    unsigned int idx = 0;

    if (!nb_pkts) {
        size_t acc = 0;
        for (int i = 0; i < fifo->nb; i++) {
            acc += avt_pkt_fifo_get_entry_size(&fifo->data[i]);
            if (acc > ceiling) {
                idx = i;
                break;
            }
        }
    } else {
        if (ckd_sub(&idx, fifo->nb, nb_pkts))
            return AVT_ERROR(EINVAL);
    }

    for (; idx < fifo->nb; idx++) {
        AVTOutputPacket *data = &fifo->data[idx];
        avt_buffer_quick_unref(&data->pl);
    }

    fifo->nb = idx;

    return 0;
}

size_t avt_pkt_fifo_size(AVTPacketFifo *fifo)
{
    // TODO: not sure if I want to use fifo->alloc instead of fifo->nb here
    size_t acc = fifo->alloc * sizeof(*fifo->data);

    for (int i = 0; i < fifo->nb; i++)
        acc += avt_buffer_get_data_len(&fifo->data[i].pl);

    return acc;
}
