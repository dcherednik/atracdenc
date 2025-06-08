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

#include <iostream>
#include <string>
#include <stdexcept>

#include <getopt.h>

#include "help.h"

#include "pcmengin.h"
#include "wav.h"
#include "aea.h"
#include "rm.h"
#include "at3.h"
#include "config.h"
#include "atrac1denc.h"
#include "atrac3denc.h"
#include "atrac3p.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::stoi;

using namespace NAtracDEnc;

typedef std::unique_ptr<TPCMEngine> TPcmEnginePtr;
typedef std::unique_ptr<IProcessor> TAtracProcessorPtr;

static void printUsage(const char* myName, const string& err = string())
{
    if (!err.empty()) {
        cerr << err << endl;
    }
    cerr << "\tuse: " << myName << " -h to get help" << endl;
}

static void printProgress(int percent)
{
    static uint32_t counter;
    counter++;
    const char symbols[4] = {'-', '\\', '|', '/'};
    cout << symbols[counter % 4]<< "  "<< percent <<"% done\r";
    fflush(stdout);
}

static string GetFileExt(const string& path) {
    size_t dotPos = path.rfind('.');
    std::string ext;
    if (dotPos != std::string::npos && dotPos < path.size()) {
        ext = path.substr(dotPos + 1);
    }
    return ext;
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

enum EMode {
    E_ENCODE = 1,
    E_DECODE = 2,
    E_ATRAC3 = 4,
    E_ATRAC3PLUS = 8
};

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
    O_ADVANCED_OPT = 7,
};

static void CheckInputFormat(const TWav* p)
{
//    if (p->IsFormatSupported() == false)
//        throw std::runtime_error("unsupported format of input file");

    if (p->GetSampleRate() != 44100)
        throw std::runtime_error("unsupported sample rate");
}

static TWavPtr OpenWavFile(const string& inFile)
{
    TWavPtr wavPtr = std::make_unique<TWav>(inFile);
    CheckInputFormat(wavPtr.get());
    return wavPtr;
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
    const size_t numChannels = (*wavIO)->GetChannelNum();
    *totalSamples = (*wavIO)->GetTotalSamples();
    //TODO: recheck it
    const uint64_t numFrames = numChannels * (*totalSamples) / TAtrac1Data::NumSamples;
    if (numFrames >= UINT32_MAX) {
        std::cerr << "Number of input samples exceeds output format limitation,"
            "the result will be incorrect" << std::endl;
    }
    TCompressedOutputPtr aeaIO = CreateAeaOutput(outFile, "test", numChannels, (uint32_t)numFrames);
    pcmEngine->reset(new TPCMEngine(4096,
                                            numChannels,
                                            TPCMEngine::TReaderPtr((*wavIO)->GetPCMReader())));
    if (!noStdOut)
        cout << "Input\n Filename: " << inFile
             << "\n Channels: " << (int)numChannels
             << "\n SampleRate: " << (*wavIO)->GetSampleRate()
             << "\n Duration (sec): " << *totalSamples / (*wavIO)->GetSampleRate()
	     << "\nOutput:\n Filename: " << outFile
	     << "\n Codec: ATRAC1"
             << endl;
    atracProcessor->reset(new TAtrac1Encoder(std::move(aeaIO), std::move(encoderSettings)));
}

static void PrepareAtrac1Decoder(const string& inFile,
                                 const string& outFile,
                                 const bool noStdOut,
                                 uint64_t* totalSamples,
                                 TWavPtr* wavIO,
                                 TPcmEnginePtr* pcmEngine,
                                 TAtracProcessorPtr* atracProcessor)
{
    TCompressedInputPtr aeaIO = CreateAeaInput(inFile);
    *totalSamples = aeaIO->GetLengthInSamples();
    if (!noStdOut)
        cout << "Input\n Filename: " << inFile
             << "\n Name: " << aeaIO->GetName()
             << "\n Channels: " << (int)aeaIO->GetChannelNum()
	     << "\nOutput:\n Filename: " << outFile
	     << "\n Codec: PCM"
             << endl;
    wavIO->reset(new TWav(outFile, aeaIO->GetChannelNum(), 44100));
    pcmEngine->reset(new TPCMEngine(4096,
                                            aeaIO->GetChannelNum(),
                                            TPCMEngine::TWriterPtr((*wavIO)->GetPCMWriter())));
    atracProcessor->reset(new TAtrac1Decoder(std::move(aeaIO)));
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
    const int numChannels = encoderSettings.SourceChannels;
    *totalSamples = wavIO->GetTotalSamples();
    const uint64_t numFrames = (*totalSamples) / 1024;
    if (numFrames >= UINT32_MAX) {
        std::cerr << "Number of input samples exceeds output format limitation,"
            "the result will be incorrect" << std::endl;
    }

    const string ext = GetFileExt(outFile);

    TCompressedOutputPtr omaIO;

    string contName;
    if (ext == "wav" || ext == "at3") {
        contName = "AT3 (RIFF)";
        omaIO = CreateAt3Output(outFile, numChannels, numFrames,
                encoderSettings.ConteinerParams->FrameSz,
                encoderSettings.ConteinerParams->Js);
    } else if (ext == "rm") {
        contName = "RealMedia";
        omaIO = CreateRmOutput(outFile, "test", numChannels,
            numFrames, encoderSettings.ConteinerParams->FrameSz,
            encoderSettings.ConteinerParams->Js);
    } else {
        contName = "OMA";
        omaIO.reset(new TOma(outFile,
            "test",
            numChannels,
            (int32_t)numFrames, OMAC_ID_ATRAC3,
            encoderSettings.ConteinerParams->FrameSz,
            encoderSettings.ConteinerParams->Js));
    }

    if (!noStdOut)
        cout << "Input:\n Filename: " << inFile
             << "\n Channels: " << (int)numChannels
             << "\n SampleRate: " << wavIO->GetSampleRate()
             << "\n Duration (sec): " << *totalSamples / wavIO->GetSampleRate()
	     << "\nOutput:\n Filename: " << outFile
	     << "\n Codec: ATRAC3"
	     << "\n Container: " << contName
             << "\n Bitrate: " << encoderSettings.ConteinerParams->Bitrate
             << endl;

    pcmEngine->reset(new TPCMEngine(4096,
                                            numChannels,
                                            TPCMEngine::TReaderPtr(wavIO->GetPCMReader())));
    atracProcessor->reset(new TAtrac3Encoder(std::move(omaIO), std::move(encoderSettings)));
}

static void PrepareAtrac3PEncoder(const string& inFile,
                                  const string& outFile,
                                  const bool noStdOut,
                                  int numChannels,
                                  uint64_t* totalSamples,
                                  const TWavPtr& wavIO,
                                  TPcmEnginePtr* pcmEngine,
                                  TAtracProcessorPtr* atracProcessor,
                                  const char* advancedOpt)
{
    *totalSamples = wavIO->GetTotalSamples();
    const uint64_t numFrames = (*totalSamples) / 2048;
    if (numFrames >= UINT32_MAX) {
        std::cerr << "Number of input samples exceeds output format limitation,"
            "the result will be incorrect" << std::endl;
    }

    const string ext = GetFileExt(outFile);

    TCompressedOutputPtr omaIO;

    string contName;
    if (ext == "wav" || ext == "at3") {
        throw std::runtime_error("Not implemented");
    } else if (ext == "rm") {
        throw std::runtime_error("RealMedia container is not supported for ATRAC3PLUS");
    } else {
        contName = "OMA";
        omaIO.reset(new TOma(outFile,
            "test",
            numChannels,
            (int32_t)numFrames, OMAC_ID_ATRAC3PLUS,
            2048,
            false));
    }

    if (!noStdOut)
        cout << "Input:\n Filename: " << inFile
             << "\n Channels: " << (int)numChannels
             << "\n SampleRate: " << wavIO->GetSampleRate()
             << "\n Duration (sec): " << *totalSamples / wavIO->GetSampleRate()
	     << "\nOutput:\n Filename: " << outFile
	     << "\n Codec: ATRAC3Plus"
	     << "\n Container: " << contName
             //<< "\n Bitrate: " << encoderSettings.ConteinerParams->Bitrate
             << endl;

    pcmEngine->reset(new TPCMEngine(4096,
                                            numChannels,
                                            TPCMEngine::TReaderPtr(wavIO->GetPCMReader())));
    TAt3PEnc::TSettings settings;
    if (advancedOpt) {
        TAt3PEnc::ParseAdvancedOpt(advancedOpt, settings);
    }
    atracProcessor->reset(new TAt3PEnc(std::move(omaIO), numChannels, settings));
}


int main_(int argc, char* const* argv)
{
    const char* myName = argv[0];
    const char* advancedOpt = nullptr;
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
        { "advanced", required_argument, NULL, O_ADVANCED_OPT},
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
                    } else if (strcmp(optarg, "atrac3_lp4") == 0) {
                        mode |= E_ATRAC3;
                        bitrate = 64;
                    } else if (strcmp(optarg, "atrac3plus") == 0) {
                        mode |= E_ATRAC3PLUS;
                    } else if (strcmp(optarg, "atrac1") == 0) {
                        // this is the default
                    } else {
                       // bad value
                       string err = "unrecognized encoding codec: ";
                       err.append(optarg);
                       printUsage(myName, err);
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
            case O_ADVANCED_OPT:
                advancedOpt = optarg;
                break;
            default:
                printUsage(myName);
                return 1;
        }

    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        cerr << "Unhandled arg: " << argv[0] << endl;
        return 1;
    }

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
            case (E_ENCODE | E_ATRAC3PLUS):
            {
                wavIO = OpenWavFile(inFile);
                PrepareAtrac3PEncoder(inFile, outFile, noStdOut, wavIO->GetChannelNum(),
                    &totalSamples, wavIO, &pcmEngine, &atracProcessor, advancedOpt);
                pcmFrameSz = 2048;
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

    auto atracLambda = atracProcessor->GetLambda();

    uint64_t processed = 0;
    try {
        while (totalSamples > (processed = pcmEngine->ApplyProcess(pcmFrameSz, atracLambda)))
        {
            if (!noStdOut)
                printProgress(static_cast<int>(processed*100/totalSamples));
        }
        if (!noStdOut)
            cout << "\nDone" << endl;
    }
    catch (const TAeaIOError& err) {
        cerr << "Aea IO fatal error: " << err.what() << endl;
        return 1;
    }
    catch (const TNoDataToRead&) {
        cerr << "No more data to read from input" << endl;
        return 0;
    }
    catch (const std::exception& ex) {
        cerr << "Encode/Decode error: " << ex.what() << endl;
        return 1;
    }
    return 0;
}

int main(int argc, char* const* argv) {
#ifndef PLATFORM_WINDOWS
	return main_(argc, argv);
# else
	LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);

	std::vector<char*> newArgv(argc);

	for (int i = 0; i < argc; i++) {
		const size_t sz = wcslen(argvW[i]);
		const size_t maxUtf8Len = sz * 4;
		// 0 termination in any case
		std::vector<char> buf(maxUtf8Len + 1);

		if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argvW[i], -1, buf.data(), maxUtf8Len, NULL, NULL)) {
			DWORD lastError = GetLastError();
			std::cerr << "Unable to convert argument to uft8, " << std::hex << lastError << std::endl;
		}
		const size_t utf8Len = strlen(buf.data());
		newArgv[i] = new char[utf8Len + 1];
		strcpy(newArgv[i], buf.data());
	}


	int rv = main_(argc, newArgv.data());

	for (auto& a : newArgv) {
		delete[] a;
	}

	LocalFree(argvW);
	return rv;
#endif
}
