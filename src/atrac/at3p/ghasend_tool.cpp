#include "at3p_bitstream.h"
#include "oma.h"

#include <string>
#include <iostream>

using namespace std;

void process(const string& in, NAtracDEnc::TAt3PBitStream* bs) {
    size_t pos = 0;
    int cur_num_pos = 0;
    enum {
        TAB,
        NUM,
        ERR,
    } state = NUM;

    std::vector<int> nums;

    while (pos < in.size()) {
        switch (state) {
            case TAB:
                if (in[pos] == '\t') {
                    break;
                } else if (('0' <= in[pos] && in[pos] <= '9') || in[pos] == '-') {
                    state = NUM;
                    cur_num_pos = pos;
                    break;
                } else {
                    fprintf(stderr, "TAB state: %s\n", &in[pos]);
                    abort();
                }
            case NUM:
                if (in[pos] == '\t') {
                    nums.push_back(stoi(in.substr(cur_num_pos, pos - cur_num_pos)));
                    state = TAB;
                    break;
                } else if (pos == in.size() - 1) {
                    nums.push_back(stoi(in.substr(cur_num_pos)));
                    break;
                } else if (('0' <= in[pos] && in[pos] <= '9') || in[pos] == '-') {
                    break;
                } else {
                    fprintf(stderr, "NUM state: %s\n", &in[pos]);
                    abort();
                }
            case ERR:
                abort();
        }
        pos++;
    }

    if (nums.size() != 3)
        return;

    std::cerr << "gen: " << nums[0] << '\t' << nums[1] << '\t' << nums[2] << std::endl;

    NAtracDEnc::TAt3PGhaData frame;
    frame.NumToneBands = 1;
    frame.Waves[0].WaveParams.push_back(NAtracDEnc::TAt3PGhaData::TWaveParam{(uint32_t)nums[1], 53, 0, 0});
    frame.Waves[0].WaveSbInfos.resize(1);
    frame.Waves[0].WaveSbInfos[0].WaveIndex = 0;
    frame.Waves[0].WaveSbInfos[0].WaveNums = 1;
    frame.Waves[0].WaveSbInfos[0].Envelope =  {(uint32_t)nums[0], (uint32_t)nums[2]};

    //bs->WriteFrame(1, &frame);
}

int main(int argc, char** argv) {
    if (argc != 2)
        return -1;

    string path(argv[1]);

    std::unique_ptr<TOma> out(new TOma(path,
            "test",
            1,
            1, OMAC_ID_ATRAC3PLUS,
            2048,
            false));

    NAtracDEnc::TAt3PBitStream bs(out.get(), 2048);

    cout << "output: " << path << endl;
    string textline;
    while (getline(cin, textline)) {
        process(textline, &bs);
    }
    return 0;
}
