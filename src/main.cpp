#include <iostream>
#include <string>

#include <getopt.h>

#include "pcmengin.h"
#include "wav.h"
#include "aea.h"
#include "atracdenc.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::unique_ptr;
using std::move;

using namespace NAtracDEnc;


static void printUsage(const char* myName) {
    cout << "\tusage: " << myName << " <-e|-d> <-i input> <-o output>\n" << endl;
    cout << "-e encode mode (PCM -> ATRAC), -i wav file, -o aea file" << endl;
    cout << "-d decode mode (ATRAC -> PCM), -i aea file, -o wav file" << endl;
}

static void printProgress(int percent) {
    static uint32_t counter;
    counter++;
    const char symbols[4] = {'-', '\\', '|', '/'};
    cout << symbols[counter % 4]<< "  "<< percent <<"% done\r";
    fflush(stdout);
}

int main(int argc, char* const* argv) {
    const char* myName = argv[0];
    static struct option longopts[] = {
        { "encode", no_argument, NULL, 'e' },
        { "decode", no_argument, NULL, 'd' },
        { "numbfus", required_argument, NULL, 1},
        { "mono", no_argument, NULL, 'm'},
        { NULL, 0, NULL, 0}
    };

    int ch = 0;
    string inFile;
    string outFile;
    uint32_t mode = 0;
    bool mono = false;
    while ((ch = getopt_long(argc, argv, "edi:o:m", longopts, NULL)) != -1) {
        switch (ch) {
            case 'e':
                cout << "encode " << endl;
                mode |= E_ENCODE;
                break;
            case 'd':
                cout << "decode" << endl;

                mode |= E_DECODE;
                break;
            case 'i':
                inFile = optarg;
                break;
            case 'o':
                outFile = optarg;
                cout << "out: " << outFile<< endl;
                break;
            case 'm':
                mono = true;
                break;
            case 1:
                cout << "numbfus" << optarg << endl;
                break;

			default:
                printUsage(myName);
        }

    }
    argc -= optind;
    argv += optind;

    if (inFile.empty()) {
        cout << "No input file" << endl;
        return 1;
    }
    if (outFile.empty()) {
        cout << "No out file" << endl;
        return 1;
    }

    TWavPtr wavIO;
    TAeaPtr aeaIO;
    TPCMEngine<double>* pcmEngine = nullptr;
    uint64_t totalSamples = 0;
    if (mode == E_ENCODE) {
        wavIO = TWavPtr(new TWav(inFile));
        const TWavHeader& wavHeader = wavIO->GetWavHeader();
        const int numChannels = wavHeader.NumOfChan;
        totalSamples = wavHeader.ChunkSize;
        aeaIO = TAeaPtr(new TAea(outFile, "test", numChannels, totalSamples / 2 / 512));
        pcmEngine = new TPCMEngine<double>(4096, numChannels, TPCMEngine<double>::TReaderPtr(wavIO->GetPCMReader<double>()));
        cout << "Input file: " << inFile << "Channels: " << numChannels << "SampleRate: " << wavHeader.SamplesPerSec << "TotalSamples: " << totalSamples << endl;
    } else if (mode == E_DECODE) {
        aeaIO = TAeaPtr(new TAea(inFile));
        totalSamples = 4 *  aeaIO->GetLengthInSamples();
        uint32_t length = aeaIO->GetLengthInSamples();
        cout << "Name: " << aeaIO->GetName() << "\n Channels: " << aeaIO->GetChannelNum() << "\n Length: " << length << endl;
        wavIO = TWavPtr(new TWav(outFile, TWav::CreateHeader(aeaIO->GetChannelNum(), length)));
        pcmEngine = new TPCMEngine<double>(4096, aeaIO->GetChannelNum(), TPCMEngine<double>::TWriterPtr(wavIO->GetPCMWriter<double>()));
    } else {
        cout << "Processing mode was not specified" << endl;
        return 1;
    }

    TAtrac1Processor atrac1Processor(move(aeaIO), mono);
    auto atracLambda = (mode == E_DECODE) ? atrac1Processor.GetDecodeLambda() : atrac1Processor.GetEncodeLambda();
    uint64_t processed = 0;
    try {
        while (totalSamples/4 > (processed = pcmEngine->ApplyProcess(512, atracLambda)))
        {
            printProgress(processed*100/(totalSamples/4));
        }
        cout << "\nDone" << endl;
    }
    catch (TAeaIOError err) {
        cerr << "Aea IO fatal error: " << err.what() << endl;
        exit(1);
    }
    catch (TWavIOError err) {
        cerr << "Wav IO fatal error: " << err.what() << endl;
        exit(1);
    }
}
