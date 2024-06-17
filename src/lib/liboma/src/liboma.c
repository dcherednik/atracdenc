/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "../include/oma.h"

#include <endian_tools.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>


#define OMA_HEADER_SIZE 96

struct omafile_ctx {
    FILE* file;
    oma_info_t info;
};

static const int liboma_samplerates[8] = { 32000, 44100, 48000, 88200, 96000, 0 };
static const char* codec_name[6] = { "ATRAC3", "ATRAC3PLUS", "MPEG1LAYER3", "LPCM", "", "OMAC_ID_WMA" };
static char ea3_str[] = {'E', 'A', '3'};
static int channel_id_to_format_tab[7] = { OMA_MONO, OMA_STEREO, OMA_3, OMA_4, OMA_6, OMA_7, OMA_8 };
enum {
    OMAERR_OK = 0,
    OMAERR_IO = -1,
    OMAERR_PERM = -2,
    OMAERR_FMT = -3,
    OMAERR_ENCRYPT = -4,
    OMAERR_VAL = -5,
    OMAERR_EOF = -6
};

#ifdef _MSC_VER
__declspec(thread) int err;
#else
static __thread int err;
#endif

int oma_get_last_err() {
    return err;
}

static void save_err(int e) {
    err = e;
}

static int oma_check_header(const char* buf) {
    if (memcmp(buf, &ea3_str[0], 3) || buf[4] != 0 || buf[5] != OMA_HEADER_SIZE) {
        return OMAERR_FMT;
    }
    return OMAERR_OK;
}

static int oma_check_encryption(const char* buf) {
    if (buf[6] == -1 && buf[7] == -1)
        return OMAERR_OK;
    return OMAERR_ENCRYPT;
}

static int oma_get_samplerate_idx(int samplerate) {
    if (samplerate <= 0) {
        fprintf(stderr, "wrong samplerate\n");
        return -1;
    }
    for (int i = 0; ; i++) {
       if (liboma_samplerates[i] == samplerate)
           return i;
       if (liboma_samplerates[i] == 0)
           return -1;
    }
    return -1;
}

static int oma_get_channel_idx(int channel_format) {
    for (int i = 0; i < 7; i++) {
        if (channel_id_to_format_tab[i] == channel_format)
            return i;
    }
    return -1;
}

static int oma_read_atrac3_header(uint32_t params, oma_info_t* info) {
    const int js = (params >> 17) & 0x1;
    const int samplerate = liboma_samplerates[(params >> 13) & 0x7];
    if (samplerate == 0) {
        fprintf(stderr, "liboma: wrong samplerate params, can't read header\n");
        return -1;
    }
    info->codec = OMAC_ID_ATRAC3;
    info->framesize = (params & 0x3FF) * 8;
    info->samplerate = samplerate;
    info->channel_format = js ? OMA_STEREO_JS : OMA_STEREO;
    return 0;
}

static int oma_write_atrac3_header(uint32_t *params, oma_info_t *info) {
    const int channel_format = info->channel_format;
    if (channel_format != OMA_STEREO_JS && channel_format != OMA_STEREO) {
        fprintf(stderr, "wrong channel format\n");
        return -1;
    }
    const uint32_t js = channel_format == OMA_STEREO_JS;
    const int samplerate_idx = oma_get_samplerate_idx(info->samplerate);
    if (samplerate_idx == -1)
        return -1;
    const uint32_t framesz = info->framesize / 8;
    if (framesz > 0x3FF)
        return -1;
    *params = swapbyte32_on_le((OMAC_ID_ATRAC3 << 24) | (js << 17) | ((uint32_t)samplerate_idx << 13) | framesz);
    return 0;
}

static int oma_read_atrac3p_header(uint32_t params, oma_info_t* info) {
    const int channel_id = (params >> 10) & 7;
    if (channel_id == 0) {
        return -1;
    }
    const int samplerate = liboma_samplerates[(params >> 13) & 0x7];
     if (samplerate == 0) {
        fprintf(stderr, "liboma: wrong samplerate params, can't read header\n");
        return -1;
    }
    info->codec = OMAC_ID_ATRAC3PLUS;
    info->framesize = ((params & 0x3FF) * 8) + 8;
    info->samplerate = samplerate;
    uint32_t ch_id = (params >> 10) & 7;
    info->channel_format = channel_id_to_format_tab[ch_id - 1];
    return 0;
}

static int oma_write_atrac3p_header(uint32_t *params, oma_info_t *info) {

    const int samplerate_idx = oma_get_samplerate_idx(info->samplerate);
    if (samplerate_idx == -1)
        return -1;

    const uint32_t framesz = (info->framesize - 8) / 8;
    if (framesz > 0x3FF)
        return -1;

    const int32_t ch_id = oma_get_channel_idx(info->channel_format);
    if (ch_id < 0)
        return -1;

    *params = swapbyte32_on_le((OMAC_ID_ATRAC3PLUS << 24) | ((int32_t)samplerate_idx << 13) | ((ch_id + 1) << 10) | framesz);
    return 0;
}

static int oma_write_header(OMAFILE* ctx, oma_info_t *omainfo) {
    if (ctx == NULL || omainfo == NULL)
        return -1;
    char *headerbuf = (char*)calloc(OMA_HEADER_SIZE, 1);
    memcpy(headerbuf, &ea3_str[0], 3);
    headerbuf[3] = 1; //???
    headerbuf[5] = OMA_HEADER_SIZE;
    headerbuf[6] = 0xFF;
    headerbuf[7] = 0xFF;
    uint32_t *params = (uint32_t*)(headerbuf+32);
    switch (omainfo->codec) {
        case OMAC_ID_ATRAC3:
            oma_write_atrac3_header(params, omainfo);
            break;
        case OMAC_ID_ATRAC3PLUS:
            oma_write_atrac3p_header(params, omainfo);
            break;
        default:
            assert(0);
            break;
    }
    int rv = fwrite(headerbuf, sizeof(char), OMA_HEADER_SIZE, ctx->file);
    if (rv != OMA_HEADER_SIZE) {
        fprintf(stderr, "can't write header\n");
        rv = -1;
    }
    free(headerbuf);
    return rv;
}

static int oma_parse_header(OMAFILE* file) {
    char buf[OMA_HEADER_SIZE];
    int read = fread(&buf[0], sizeof(char), OMA_HEADER_SIZE, file->file);
    int err = 0;
    uint32_t params = 0;
    if (OMA_HEADER_SIZE != read)
        return feof(file->file) ? OMAERR_FMT : OMAERR_IO;

    err = oma_check_header(&buf[0]);
    if (OMAERR_OK != err)
        return err;

    err = oma_check_encryption(&buf[0]);
    if (OMAERR_OK != err)
        return err;

    //detect codecs
    params = ((uint8_t)buf[33]) << 16 | ((uint8_t)buf[34]) << 8 | ((uint8_t)buf[35]);
    switch (buf[32]) {
        case OMAC_ID_ATRAC3:
            oma_read_atrac3_header(params, &file->info);
            break;
        case OMAC_ID_ATRAC3PLUS:
            oma_read_atrac3p_header(params, &file->info);
            break;

        default:
            fprintf(stderr, "got unsupported format: %d\n", buf[32]);
            return OMAERR_FMT;
    }

    return OMAERR_OK;
}

OMAFILE* oma_open(const char *path, int mode, oma_info_t *info) {
    const static char* modes[3] = {"", "rb", "wb"};
    FILE* file = fopen(path, modes[mode]);
    int err = 0;
    if (NULL == file) {
        return NULL;
    }

    struct omafile_ctx *ctx = (struct omafile_ctx*)malloc(sizeof(struct omafile_ctx));
    if (NULL == ctx) {
        goto close_ret;
    }

    ctx->file = file;
    if (mode == OMAM_R) { 
        err = oma_parse_header(ctx);
        if (OMAERR_OK != err) {
            goto free_close_ret;
        }
    } else {
        if (!info) {
            err = OMAERR_VAL;
            goto free_close_ret;
        }
        memcpy(&ctx->info, info, sizeof(oma_info_t));
        err = oma_write_header(ctx, info);
    }

    return ctx;

free_close_ret:
    free(ctx);

close_ret:
    save_err(err);
    fclose(file);
    return NULL;
}

int oma_close(OMAFILE *ctx) {
    FILE* file = ctx->file;
    free(ctx);
    fclose(file);
    return 0;
}

block_count_t oma_read(OMAFILE *oma_file, void *ptr, block_count_t blocks) {
    size_t read = fread(ptr, oma_file->info.framesize, blocks, oma_file->file);
    if (read == blocks)
        return read;
    if (feof(oma_file->file)) {
        save_err(OMAERR_EOF);
        return 0;
    }
    return -1;
}

block_count_t oma_write(OMAFILE *oma_file, const void *ptr, block_count_t blocks) {
    size_t writen = fwrite(ptr, oma_file->info.framesize, blocks, oma_file->file);
    if (writen == blocks)
        return writen;
    return -1;
}

oma_info_t* oma_get_info(OMAFILE *oma_file) {
    if (oma_file == NULL)
        return NULL;
    return &oma_file->info;
}
int oma_get_bitrate(oma_info_t *info) {
    switch (info->codec) {
        case OMAC_ID_ATRAC3:
            return info->samplerate * info->framesize * 8 / 1024;
            break;
        case OMAC_ID_ATRAC3PLUS:
            return info->samplerate * info->framesize * 8 / 2048;
            break;
        default:
            return -1;
    }
    return -1;
}

const char *oma_get_codecname(oma_info_t *info) {
    if (info == NULL)
        return "";
    int id = info->codec;
    if (id < 0 || id > 5)
        return "";
    return codec_name[id];
}
