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
