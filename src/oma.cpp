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

#include "oma.h"

#include <stdlib.h>

using std::string;
using std::vector;
using std::unique_ptr;

TOma::TOma(const string& filename, const string&, size_t numChannel,
    uint32_t /*numFrames*/, int cid, uint32_t framesize, bool jointStereo) {
    oma_info_t info;
    info.codec = cid;
    info.samplerate = 44100;
    info.channel_format = (cid == OMAC_ID_ATRAC3)
        ? (jointStereo ? OMA_STEREO_JS : OMA_STEREO)
        : (numChannel == 1 ? OMA_MONO : OMA_STEREO);
    info.framesize = framesize;
    File = oma_open(filename.c_str(), OMAM_W, &info);
    if (!File)
        abort();
}

TOma::~TOma() {
    oma_close(File);
}

void TOma::WriteFrame(vector<char> data) {
    if (oma_write(File, &data[0], 1) == -1) {
        fprintf(stderr, "write error\n");
        abort();
    }
}

string TOma::GetName() const {
    abort();
    return {};
}

size_t TOma::GetChannelNum() const {
    return 2; //for ATRAC3
}
