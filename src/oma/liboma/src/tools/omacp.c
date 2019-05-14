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

#include <stdio.h>
#include <stdlib.h>

#include "oma.h"

int main(int argc, char* const* argv) {
    if (3 != argc)
        fprintf(stdout, "usage: \n\t omainfo [in] [out]\n");

    OMAFILE* infile = oma_open(argv[1], OMAM_R, NULL);
    if (NULL == infile)
        fprintf(stderr, "Can't open %s to read, err: %d\n", argv[1], oma_get_last_err());

    oma_info_t *info = oma_get_info(infile);
    const char *codecname = oma_get_codecname(info);
    const int bitrate = oma_get_bitrate(info);

    fprintf(stdout, "codec: %s, bitrate: %d, channel format: %d\n", codecname, bitrate, info->channel_format);

    OMAFILE* outfile = oma_open(argv[2], OMAM_W, info);
    if (NULL == outfile)
        fprintf(stderr, "Can't open %s to write, err: %d\n", argv[2], oma_get_last_err());

    char* buf = (char*)malloc(info->framesize);
    for (;;) {
        block_count_t rcount = oma_read(infile, buf, 1);
        if (rcount == 0)
            break;
        if (rcount == -1) {
            fprintf(stderr, "read error\n");
            break;
        }
        if (oma_write(outfile, buf, 1) == -1) {
            fprintf(stderr, "write error\n");
            break;
        }
    }
}
