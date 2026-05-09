#include "help.h"
#include <string>

const std::string& GetHelp() {
    const static std::string txt = R"(
atracdenc is a tool to encode in to ATRAC1 or ATRAC3, ATRAC3PLUS, decode from ATRAC1 formats

Usage:
atracdenc {-e <codec> | --encode=<codec> | -d | --decode} -i <in> -o <out>

-e or --encode		encode file using one of codecs
	{atrac1 | atrac3 | atrac3_lp4 | atrac3plus}
-d or --decode		decode file (only ATRAC1 supported for decoding)
-i			path to input file (any sample rate, auto-resampled to 44100 Hz)
-o			path to output file
-h			print help and exit

--bitrate		allow to specify bitrate (for ATRAC3 + RealMedia container only)

Advanced options:
--bfuidxconst		Set constant amount of used BFU (ATRAC1, ATRAC3).
--notransient[=mask]	Disable transient detection and use optional mask
			to set bands with forced short MDCT window (ATRAC1)
--advanced		ATRAC3Plus advanced options (see source)

Output containers:
  .aea            ATRAC1 raw stream
  .oma            Sony OpenMG (ATRAC3 / ATRAC3Plus)
  .at3 / .wav     RIFF WAV container (ATRAC3 / ATRAC3Plus)
  .rm             RealMedia (ATRAC3 only)

Examples:
Encode in to ATRAC1 (SP)
	atracdenc -e atrac1 -i my_file.wav -o my_file.aea
Encode in to ATRAC3 (LP2, OMA container)
	atracdenc -e atrac3 -i my_file.wav -o my_file.oma
Encode in to ATRAC3 (RIFF container)
	atracdenc -e atrac3 -i my_file.wav -o my_file.at3
Encode in to ATRAC3Plus (OMA container)
	atracdenc -e atrac3plus -i my_file.wav -o my_file.oma
Encode in to ATRAC3Plus (RIFF container)
	atracdenc -e atrac3plus -i my_file.wav -o my_file.at3
Decode ATRAC1
	atracdenc -d -i my_file.aea -o my_file.wav
Non-44100 Hz input (auto-resampled)
	atracdenc -e atrac3 -i input_48k.wav -o output.at3

)";

    return txt;
}
