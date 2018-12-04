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
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <iostream>
#include <string>
#include <stdexcept>

#include <getopt.h>

#include "pcmengin.h"
#include "wav.h"
#include "aea.h"
#include "config.h"
#include "atrac1denc.h"
#include "atrac3denc.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::stoi;

using namespace NAtracDEnc;

typedef std::unique_ptr<TPCMEngine<TFloat>> TPcmEnginePtr;
typedef std::unique_ptr<IProcessor<TFloat>> TAtracProcessorPtr;

static void printUsage(const char* myName)
{
    cout << "\tusage: " << myName << " <-e <atrac1|atrac3>|-d> <-i input> <-o output>\n" << endl;
    cout << "-e atrac1|atrac3 encode mode (PCM -> ATRAC), -i wav file, -o aea/oma file" << endl;
    cout << "-d decode mode (ATRAC -> PCM), -i aea file, -o wav file" << endl;
    cout << "-h get help" << endl;

}

static void printProgress(int percent)
{
    static uint32_t counter;
    counter++;
    const char symbols[4] = {'-', '\\', '|', '/'};
    cout << symbols[counter % 4]<< "  "<< percent <<"% done\r";
    fflush(stdout);
}

static string GetHelp()
{
    return "\n--encode [atrac1|atrac3] -e <atrac1|atrac3> \t - encode mode (default codec is ATRAC1)"
        "\n--decode -d \t - decode mode (only ATRAC1 supported)"
        "\n -i input file"
        "\n -o output file"
        "\n --bitrate (only if supported by codec)"
        "\nAdvanced options:\n --bfuidxconst\t Set constant amount of used BFU (ATRAC1, ATRAC3). "
             "WARNING: It is not a lowpass filter! Do not use it to cut off hi frequency."
        "\n --bfuidxfast\t enable fast search of BFU amount (ATRAC1)"
        "\n --notransient[=mask] disable transient detection and use optional mask to set bands with short MDCT window "
                                                                                                              "(ATRAC1)";
        /*"\n --nogaincontrol disable gain control (ATRAC3)"*/
}

static int checkedStoi(const char* data, int min, int max, int def)
{
    int tmp = 0;
    try {
        tmp = stoi(data);
        if (tmp < min || tmp > max)
            throw std::invalid_argument(data);
        return tmp;
    } catch (std::invalid_argument&) {
        cerr << "Wrong arg: " << data << " " << def << " will be used" << endl;
        return def;
    }
}

enum EOptions
{
    O_ENCODE = 'e',
    O_DECODE = 'd',
    O_HELP = 'h',
    O_BITRATE = 'b',
    O_BFUIDXCONST = 1,
    O_BFUIDXFAST = 2,
    O_NOTRANSIENT = 3,
    O_MONO = 'm',
    O_NOSTDOUT = '4',
    O_NOTONAL = 5,
    O_NOGAINCONTROL = 6,
};

static void CheckInputFormat(const TWav* p)
{
    if (p->IsFormatSupported() == false)
        throw std::runtime_error("unsupported format of input file");

    if (p->GetSampleRate() != 44100)
        throw std::runtime_error("unsupported sample rate");
}

static TWavPtr OpenWavFile(const string& inFile)
{
    TWav* wavPtr = new TWav(inFile);
    CheckInputFormat(wavPtr);
    return TWavPtr(wavPtr);
}

static void PrepareAtrac1Encoder(const string& inFile,
                                 const string& outFile, 
                                 const bool noStdOut, 
                                 NAtrac1::TAtrac1EncodeSettings&& encoderSettings,
                                 uint64_t* totalSamples,
                                 TWavPtr* wavIO,
                                 TPcmEnginePtr* pcmEngine,
                                 TAtracProcessorPtr* atracProcessor)
{
    using NAtrac1::TAtrac1Data;

    {
        TWav* wavPtr = new TWav(inFile);
        CheckInputFormat(wavPtr);
        wavIO->reset(wavPtr);
    }
    const int numChannels = (*wavIO)->GetChannelNum();
    *totalSamples = (*wavIO)->GetTotalSamples();
    //TODO: recheck it
    const uint32_t numFrames = numChannels * (*totalSamples) / TAtrac1Data::NumSamples;
    TCompressedIOPtr aeaIO = TCompressedIOPtr(new TAea(outFile, "test", numChannels, numFrames));
    pcmEngine->reset(new TPCMEngine<TFloat>(4096,
                                            numChannels,
                                            TPCMEngine<TFloat>::TReaderPtr((*wavIO)->GetPCMReader<TFloat>())));
    if (!noStdOut)
        cout << "Input file: " << inFile
             << "\n Channels: " << numChannels
             << "\n SampleRate: " << (*wavIO)->GetSampleRate()
             << "\n TotalSamples: " << totalSamples
             << endl;
    atracProcessor->reset(new TAtrac1Processor(std::move(aeaIO), std::move(encoderSettings)));
}

static void PrepareAtrac1Decoder(const string& inFile,
                                 const string& outFile,
                                 const bool noStdOut,
                                 uint64_t* totalSamples,
                                 TWavPtr* wavIO,
                                 TPcmEnginePtr* pcmEngine,
                                 TAtracProcessorPtr* atracProcessor)
{
    TCompressedIOPtr aeaIO = TCompressedIOPtr(new TAea(inFile));
    *totalSamples = aeaIO->GetLengthInSamples();
    uint32_t length = aeaIO->GetLengthInSamples();
    if (!noStdOut)
        cout << "Name: " << aeaIO->GetName()
             << "\n Channels: " << aeaIO->GetChannelNum()
             << "\n Length: " << length
             << endl;
    wavIO->reset(new TWav(outFile, aeaIO->GetChannelNum(), 44100));
    pcmEngine->reset(new TPCMEngine<TFloat>(4096,
                                            aeaIO->GetChannelNum(),
                                            TPCMEngine<TFloat>::TWriterPtr((*wavIO)->GetPCMWriter<TFloat>())));
    atracProcessor->reset(new TAtrac1Processor(std::move(aeaIO), NAtrac1::TAtrac1EncodeSettings()));
}

static void PrepareAtrac3Encoder(const string& inFile,
                                 const string& outFile,
                                 const bool noStdOut,
                                 NAtrac3::TAtrac3EncoderSettings&& encoderSettings,
                                 uint64_t* totalSamples,
                                 const TWavPtr& wavIO,
                                 TPcmEnginePtr* pcmEngine,
                                 TAtracProcessorPtr* atracProcessor)
{
    std::cout << "WARNING: ATRAC3 is uncompleted, result will be not good )))" << std::endl;
    if (!noStdOut)
        std::cout << "bitrate " << encoderSettings.ConteinerParams->Bitrate << std::endl;
    const int numChannels = encoderSettings.SourceChannels;
    *totalSamples = wavIO->GetTotalSamples();
    TCompressedIOPtr omaIO = TCompressedIOPtr(new TOma(outFile,
                                                       "test",
                                                       numChannels,
                                                       numChannels * (*totalSamples) / 512, OMAC_ID_ATRAC3,
                                                       encoderSettings.ConteinerParams->FrameSz));
    pcmEngine->reset(new TPCMEngine<TFloat>(4096,
                                            numChannels,
                                            TPCMEngine<TFloat>::TReaderPtr(wavIO->GetPCMReader<TFloat>())));
    atracProcessor->reset(new TAtrac3Processor(std::move(omaIO), std::move(encoderSettings)));
}

int main(int argc, char* const* argv)
{
    const char* myName = argv[0];
    static struct option longopts[] = {
        { "encode", optional_argument, NULL, O_ENCODE },
        { "decode", no_argument, NULL, O_DECODE },
        { "help", no_argument, NULL, O_HELP },
        { "bitrate", required_argument, NULL, O_BITRATE},
        { "bfuidxconst", required_argument, NULL, O_BFUIDXCONST},
        { "bfuidxfast", no_argument, NULL, O_BFUIDXFAST},
        { "notransient", optional_argument, NULL, O_NOTRANSIENT},
        { "nostdout", no_argument, NULL, O_NOSTDOUT},
        { "nogaincontrol", no_argument, NULL, O_NOGAINCONTROL},
        { NULL, 0, NULL, 0}
    };

    int ch = 0;
    string inFile;
    string outFile;
    uint32_t mode = 0;
    uint32_t bfuIdxConst = 0; //0 - auto, no const
    bool fastBfuNumSearch = false;
    bool noStdOut = false;
    bool noGainControl = false;
    bool noTonalComponents = false;
    NAtrac1::TAtrac1EncodeSettings::EWindowMode windowMode = NAtrac1::TAtrac1EncodeSettings::EWindowMode::EWM_AUTO;
    uint32_t winMask = 0; //0 - all is long
    uint32_t bitrate = 0; //0 - use default for codec
    while ((ch = getopt_long(argc, argv, "e:dhi:o:m", longopts, NULL)) != -1) {
        switch (ch) {
            case O_ENCODE:
                mode |= E_ENCODE;
                // if arg is given, it must specify the codec; otherwise use atrac1
                if (optarg) {
                    if (strcmp(optarg, "atrac3") == 0) {
                        mode |= E_ATRAC3;
                    } else if (strcmp(optarg, "atrac1") == 0) {
                        // this is the default
                    } else {
                       // bad value
                       printUsage(myName);
                       return 1;
                    }
                }
                break;
            case O_DECODE:
                mode |= E_DECODE;
                break;
            case 'i':
                inFile = optarg;
                break;
            case 'o':
                outFile = optarg;
                if (outFile == "-")
                    noStdOut = true;
                break;
            case 'h':
                cout << GetHelp() << endl;
                return 0;
                break;
            case O_BITRATE:
                bitrate = checkedStoi(optarg, 32, 384, 0);
                std::cout << "BITRATE" << bitrate << std::endl;
                break;
            case O_BFUIDXCONST:
                bfuIdxConst = checkedStoi(optarg, 1, 32, 0);
                break;
            case O_BFUIDXFAST:
                fastBfuNumSearch = true;
                break;
            case O_NOTRANSIENT:
                windowMode = NAtrac1::TAtrac1EncodeSettings::EWindowMode::EWM_NOTRANSIENT;
                if (optarg) {
                    winMask = stoi(optarg);
                }
                cout << "Transient detection disabled, bands: low - " <<
                    ((winMask & 1) ? "short": "long") << ", mid - " <<
                    ((winMask & 2) ? "short": "long") << ", hi - " <<
                    ((winMask & 4) ? "short": "long") << endl;
                break;
            case O_NOSTDOUT:
                noStdOut = true;
                break;
            case O_NOTONAL:
                noTonalComponents = true;
                break;
            case O_NOGAINCONTROL:
                noGainControl = true;
                break;
            default:
                printUsage(myName);
                return 1;
        }

    }
    argc -= optind;
    argv += optind;

    if (inFile.empty()) {
        cerr << "No input file" << endl;
        return 1;
    }
    if (outFile.empty()) {
        cerr << "No out file" << endl;
        return 1;
    }

    TPcmEnginePtr pcmEngine;
    TAtracProcessorPtr atracProcessor;
    uint64_t totalSamples = 0;
    TWavPtr wavIO;
    uint32_t pcmFrameSz = 0; //size of one pcm frame to process

    try {
        switch (mode) {
            case E_ENCODE:
	        {
                if (bfuIdxConst > 8) {
                    throw std::invalid_argument("ATRAC1 mode, --bfuidxconst is a index of max used BFU. "
                        "Values [1;8] is allowed");
                }
                using NAtrac1::TAtrac1Data;
                NAtrac1::TAtrac1EncodeSettings encoderSettings(bfuIdxConst, fastBfuNumSearch, windowMode, winMask);
                PrepareAtrac1Encoder(inFile, outFile, noStdOut, std::move(encoderSettings),
                &totalSamples, &wavIO, &pcmEngine, &atracProcessor);
                pcmFrameSz = TAtrac1Data::NumSamples;
            }
            break;
            case E_DECODE:
            {
                using NAtrac1::TAtrac1Data;
                PrepareAtrac1Decoder(inFile, outFile, noStdOut,
                &totalSamples, &wavIO, &pcmEngine, &atracProcessor);
                pcmFrameSz = TAtrac1Data::NumSamples;
            }
            break;
            case (E_ENCODE | E_ATRAC3):
            {
                using NAtrac3::TAtrac3Data;
                wavIO = OpenWavFile(inFile);
                NAtrac3::TAtrac3EncoderSettings encoderSettings(bitrate * 1024, noGainControl,
                                                                noTonalComponents, wavIO->GetChannelNum(), bfuIdxConst);
                PrepareAtrac3Encoder(inFile, outFile, noStdOut, std::move(encoderSettings),
                &totalSamples, wavIO, &pcmEngine, &atracProcessor);
                pcmFrameSz = TAtrac3Data::NumSamples;;
            }
            break;
            default:
            {
                throw std::runtime_error("Processing mode was not specified");
            }
        }
    } catch (const std::exception& ex) {
        cerr << "Fatal error: " << ex.what() << endl;
        return 1;
    }

    auto atracLambda = (mode == E_DECODE) ? atracProcessor->GetDecodeLambda() :
        atracProcessor->GetEncodeLambda();

    uint64_t processed = 0;
    try {
        while (totalSamples > (processed = pcmEngine->ApplyProcess(pcmFrameSz, atracLambda)))
        {
            if (!noStdOut)
                printProgress(processed*100/totalSamples);
        }
        if (!noStdOut)
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
