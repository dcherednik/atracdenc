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

#include "oma.h"

int main(int argc, char* const* argv) {
    fprintf(stderr, "%d\n", argc);
    if (2 > argc) {
        fprintf(stdout, "usage: \n\t omainfo [filename]\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        OMAFILE* file = oma_open(argv[i], OMAM_R, NULL);
        if (NULL == file)
            fprintf(stderr, "Can't open %s\n", argv[i]);

        oma_info_t *info = oma_get_info(file);
        const char *codecname = oma_get_codecname(info);
        const int bitrate = oma_get_bitrate(info);

        fprintf(stdout, "%s codec: %s, bitrate: %d, channelformat: %d framesz: %d\n", argv[i], codecname, bitrate, info->channel_format, info->framesize);
        oma_close(file);
    }
    return 0;
}
