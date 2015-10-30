#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <memory>



class TAeaIOError : public std::exception {
    const int ErrNum = 0;
    const char* Text;
public:
    TAeaIOError(const char* str, int err)
        : ErrNum(err)
        , Text(str)
    {}
    virtual const char* what() const throw() {
        return Text;
    }
};

class TAeaFormatError {
};

class TAea {
    static constexpr uint32_t AeaMetaSize = 2048;
    struct TMeta {
        FILE* AeaFile;
        std::array<char, AeaMetaSize> AeaHeader;
    } Meta;
    static TAea::TMeta ReadMeta(const std::string& filename);
    static TAea::TMeta CreateMeta(const std::string& filename, const std::string& title, int numChannel);
public:
        typedef std::array<char, 212> TFrame;
		TAea(const std::string& filename);
        TAea(const std::string& filename, const std::string& title, int numChannel);
		~TAea();
        std::unique_ptr<TFrame> ReadFrame(); 
//        void WriteFrame(std::unique_ptr<TAea::TFrame>&& frame);
        void WriteFrame(std::vector<char> data);
        std::string GetName() const;
        int GetChannelNum() const;
        uint32_t GetLengthInSamples() const;
};

typedef std::unique_ptr<TAea> TAeaPtr;

