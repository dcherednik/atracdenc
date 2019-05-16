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


#ifndef OMA_H
#define OMA_H

typedef struct omafile_ctx OMAFILE;

struct oma_info {
    int codec;
    int framesize;
    int samplerate;
    int channel_format;
};

enum {
    OMAM_R = 0x1,
    OMAM_W = 0x2,
};

enum {
    OMAC_ID_ATRAC3 = 0,
    OMAC_ID_ATRAC3PLUS = 1,
    OMAC_ID_MP3 = 2,
    OMAC_ID_LPCM = 3,
    OMAC_ID_WMA = 5
};

enum {
    OMA_MONO = 0,
    OMA_STEREO = 1,
    OMA_STEREO_JS = 2,
    OMA_3 = 3,
    OMA_4 = 4,
    OMA_6 = 5,
    OMA_7 = 6,
    OMA_8 = 7

};

typedef struct oma_info oma_info_t;
typedef int block_count_t;

#ifdef __cplusplus
extern "C" {
#endif
int oma_get_last_err();

OMAFILE* oma_open(const char *path, int mode, oma_info_t *info);
int oma_close(OMAFILE* oma_file);

block_count_t oma_read(OMAFILE *oma_file, void *ptr, block_count_t blocks);
block_count_t oma_write(OMAFILE *oma_file, const void *ptr, block_count_t blocks);

oma_info_t* oma_get_info(OMAFILE *oma_file);
int oma_get_bitrate(oma_info_t *info);
const char *oma_get_codecname(oma_info_t *info);
#ifdef __cplusplus
}
#endif

#endif /* OMA_H */
