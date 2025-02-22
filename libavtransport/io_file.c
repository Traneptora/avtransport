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

#include "os_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_common.h"

struct AVTIOCtx {
    FILE *f;
    off_t rpos;
    off_t wpos;
    int is_write;
};

static int handle_error(AVTIOCtx *io, const char *msg)
{
    char8_t err_info[256];
    strerror_s(err_info, sizeof(err_info), errno);
    avt_log(io, AVT_LOG_ERROR, msg, err_info);
    return AVT_ERROR(errno);
}

static int file_init(AVTContext *ctx, AVTIOCtx **_io, AVTAddress *addr)
{
    int ret;
    AVTIOCtx *io = malloc(sizeof(*io));
    if (!io)
        return AVT_ERROR(ENOMEM);

    io->f = fopen(addr->path, "w+");
    if (!io->f) {
        ret = handle_error(io, "Error opening: %s\n");
        free(io);
        return ret;
    }

    *_io = io;

    return 0;
}

static uint32_t file_max_pkt_len(AVTContext *ctx, AVTIOCtx *io)
{
    return UINT32_MAX;
}

static int64_t file_read_input(AVTContext *ctx, AVTIOCtx *io,
                               AVTBuffer **_buf, size_t len)
{
    int ret;
    uint8_t *data;
    size_t buf_len, off = 0;
    AVTBuffer *buf = *_buf;

    if (io->is_write) {
        ret = fseeko(io->f, io->rpos, SEEK_SET);
        if (ret < 0) {
            ret = handle_error(io, "Error seeking: %s\n");
            return ret;
        }
        io->is_write = 0;
    }

    if (!buf) {
        buf = avt_buffer_alloc(len);
        if (!buf)
            return AVT_ERROR(ENOMEM);
    } else {
        off = avt_buffer_get_data_len(buf);
        ret = avt_buffer_realloc(buf, len);
        if (ret < 0)
            return ret;
    }

    data = avt_buffer_get_data(buf, &buf_len);
    len = AVT_MIN(len, buf_len - off);
    fread(data + off, 1, len, io->f);

    return (int64_t)(io->rpos = ftello(io->f));
}

static int64_t file_write_output(AVTContext *ctx, AVTIOCtx *io,
                                 uint8_t hdr[AVT_MAX_HEADER_LEN], size_t hdr_len,
                                 AVTBuffer *payload)
{
    int ret;
    size_t len;
    uint8_t *data = avt_buffer_get_data(payload, &len);

    if (!io->is_write) {
        ret = fseeko(io->f, io->wpos, SEEK_SET);
        if (ret < 0) {
            ret = handle_error(io, "Error seeking: %s\n");
            return ret;
        }
        io->is_write = 1;
    }

    size_t out = fwrite(hdr, 1, hdr_len, io->f);
    if (out != hdr_len) {
        ret = handle_error(io, "Error writing: %s\n");
        return ret;
    }

    if (payload) {
        out = fwrite(data, 1, len, io->f);
        if (out != len) {
            ret = handle_error(io, "Error writing: %s\n");
            return ret;
        }
    }
    fflush(io->f);

    return (int64_t)(io->wpos = ftello(io->f));
}

static int64_t file_seek(AVTContext *ctx, AVTIOCtx *io, int64_t off)
{
    int ret = fseeko(io->f, (off_t)off, SEEK_SET);
    if (ret < 0) {
        ret = handle_error(io, "Error seeking: %s\n");
        return ret;
    }
    io->is_write = 0;
    return (int64_t)(io->rpos = ftello(io->f));
}

static int file_flush(AVTContext *ctx, AVTIOCtx *io)
{
    int ret = fflush(io->f);
    if (ret)
        ret = handle_error(io, "Error flushing: %s\n");
    return ret;
}

static int file_close(AVTContext *ctx, AVTIOCtx **_io)
{
    AVTIOCtx *io = *_io;
    int ret = fclose(io->f);
    if (ret)
        ret = handle_error(io, "Error closing: %s\n");

    free(io);
    *_io = NULL;
    return ret;
}

const AVTIO avt_io_file = {
    .name = "file",
    .type = AVT_IO_FILE,
    .init = file_init,
    .get_max_pkt_len = file_max_pkt_len,
    .read_input = file_read_input,
    .write_output = file_write_output,
    .seek = file_seek,
    .flush = file_flush,
    .close = file_close,
};
