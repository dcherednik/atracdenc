#include "help.h"
#include <string>

const std::string& GetHelp() {
    const static std::string txt = R"(
atracdenc is a tool to encode in to ATRAC1 or ATRAC3, decode from ATRAC1 formats

Usage:
atracdenc {-e <codec> | --encode=<codec> | -d | --decode} -i <in> -o <out>

-e or --encode		encode file using one of codecs
	{atrac1 | atrac3 | atrac3_lp}
-d or --decode		decode file (only ATRAC1 supported for decoding)
-i			path to input file
-o			path to output file
-h			print help and exit

--bitrate		allow to specify bitrate (for ATRAC3 + RealMedia container only)

Advanced options:
--bfuidxconst		Set constant amount of used BFU (ATRAC1, ATRAC3).
--bfuidxfast		Enable fast search of BFU amount (ATRAC1)
--notransient[=mask]	Disable transient detection and use optional mask
			to set bands with forced short MDCT window

Examples:
Encode in to ATRAC1 (SP)
	atracdenc -e atrac1 -i my_file.wav -o my_file.aea
Encode in to ATRAC3 (LP2)
	atracdenc -e atrac3 -i my_file.wav -o my_file.oma
)";

    return txt;
}
