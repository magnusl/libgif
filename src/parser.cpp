/**
 * \file    parser.cpp
 *
 * \author  Magnus Leksell
 */

#include <gif/gif.h>
#include <gif/bit_stream.h>
#include <gif/lzw.h>
#include <iostream>

namespace gif {

uint8_t PeekByte(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    if (it == end) {
        throw std::runtime_error("Can't read byte from stream, EOF reached.");
    }
    return *it;
}

uint8_t ParseByte(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    if (it == end) {
        throw std::runtime_error("Can't read byte from stream, EOF reached.");
    }
    uint8_t result = *it;
    ++it;
    return result;
}

uint16_t ParseShort(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    const uint8_t lsb = ParseByte(it, end);
    const uint8_t msb = ParseByte(it, end);
    return (static_cast<uint16_t>(msb) << 8) | lsb;
}

std::string ParseString(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end,
    size_t length)
{
    std::string result;
    for(size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<char>(ParseByte(it, end)));
    }
    return result;
}

Version ParseHeader(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    // must start with "GIF"
    auto id = ParseString(it, end, 3);
    if (id != "GIF") {
        throw std::runtime_error(
            "Not a valid GIF file, invalid signature.");
    }

    auto version = ParseString(it, end, 3);
    if (version == "87a") {
        return Version::kGif87a;
    }
    else if (version == "89a") {
        return Version::kGif89a;
    }
    else {
        throw std::runtime_error("Unknown GIF version.");
    }
}

LogicalScreenDescriptor ParseLogicalScreenDescriptor(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    LogicalScreenDescriptor result;
    // width/height
    result.width = ParseShort(it, end);
    result.height = ParseShort(it, end);
    const uint8_t packedFields = ParseByte(it, end);
    result.globalColorTable = (packedFields >> 7) & 0x01;
    result.colorResolution = (packedFields >> 4) & 0x07;
    result.sortFlag = (packedFields >> 3) & 0x01;
    result.globalColorTableSize = (packedFields & 0x07);
    result.backgroundColorIndex = ParseByte(it, end);
    result.pixelAspectRatio = ParseByte(it, end);

    return result;
}

void ParseColorTable(
    ColorTable& table,
    unsigned tableSize,
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    table.resize(tableSize);
    for(unsigned i = 0; i < tableSize; ++i) {
        table[i].r = ParseByte(it, end);
        table[i].g = ParseByte(it, end);
        table[i].b = ParseByte(it, end);
    }
}

ImageDescriptor ParseImageDescriptor(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    // must start with an "Image Separator" byte with value 0x2c
    if (ParseByte(it, end) != 0x2c) {
        throw std::runtime_error("Invalid image separator.");
    }

    ImageDescriptor result;
    result.left = ParseShort(it, end);
    result.top = ParseShort(it, end);
    result.width = ParseShort(it, end);
    result.height = ParseShort(it, end);
    const uint8_t flags = ParseByte(it, end);

    result.localColorTable = (flags >> 7) & 0x01;
    result.interlaced = (flags >> 6) & 0x01;
    result.sortFlag = (flags >> 5) & 0x01;
    result.localColorTableSize = (flags & 0x07);

    return result;
}

GraphicControlExtension ParseGraphicControlExtension(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    GraphicControlExtension result;
    if (ParseByte(it, end) != 0xF9) {
        throw std::runtime_error("Unexpected block label.");
    }
    // the block size has to be four bytes
    if (ParseByte(it, end) != 0x04) {
        throw std::runtime_error(
            "Invalid block size for a Graphics control extension.");
    }

    const uint8_t fields = ParseByte(it, end);
    result.disposalMethod = (fields >> 2) & 0x07;
    result.userInputFlag = (fields >> 1) & 0x01;
    result.transparentColorFlag = (fields & 0x01);
    result.delayTime = ParseShort(it, end);
    result.transparentColorIndex = ParseByte(it, end);

    if (ParseByte(it, end) != 0x00) {
        throw std::runtime_error("Missing block terminator.");
    }

    return result;
}

ApplicationExtension ParseApplicationExtension(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end)
{
    if (ParseByte(it, end) != 0xFF) {
        throw std::runtime_error("Unexpected block label.");
    }

    if (ParseByte(it, end) != 11) {
        throw std::runtime_error(
            "Invalid block size for a Application extension.");
    }

    ApplicationExtension result;
    result.identifier = ParseString(it, end, 8);
    // read the application authentication code
    for(size_t i = 0; i < result.code.size(); ++i) {
        result.code[i] = ParseByte(it, end);
    }
    // for each application data block
    uint8_t blockSize = ParseByte(it, end);
    while(blockSize > 0) {
        // skip the application data
        it = it + blockSize;
        blockSize = ParseByte(it, end);
    }
    return result;
}

void Output(std::vector<uint8_t>& output, const lzw::ByteArray& data)
{
    static int i = 0;
    i += data.size();
    std::cout << "has emitted: " << i << " bytes" << std::endl;
}

void ParseImageData(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end,
    std::vector<uint8_t>& output)
{
    // read the "root size"
    const uint8_t rootSize = ParseByte(it, end);
    uint8_t N = rootSize;

    std::cout << "rootSize=" << static_cast<unsigned>(rootSize) << std::endl;

    // initialize the decode table
    lzw::DecodeTable table(N);
    
    // read the length of the first sub-block
    //uint8_t length = ParseByte(it, end);

    // the input is a bit-stream
    BitStream input(it, end);

    unsigned code, oldCode;
    
    // two reserved codes, the first for "clear code", and the second
    // for "end of information"
    unsigned cc = (1 << N);
    unsigned eoi = cc + 1;
    // the first code for new entries
    unsigned nextCode = eoi + 1;

    while(1) {

        code = input.GetBits(N + 1);
        std::cout << std::dec << "code: " << code << std::endl;
        if (code == eoi) {
            // reached end of information
            break;
        }
        else if (code == cc) {
            // clear-code
            table.Clear();
            //std::cout << "Now clear" << std::endl;
            // reset the code size
            std::cout << "Clear" << std::endl;
            N = rootSize;
            code = input.GetBits(N + 1);
            if (code == eoi) {
                break;
            }
            std::cout << "Code after clear: " << std::dec << code << std::endl;
            nextCode = eoi + 1;
            // output the string corresponding to code.
            Output(output, table.Get(code));
            oldCode = code;
        }
        else {
            if (table.InTable(code)) {
                //std::cout << "Is in table" << std::endl;
                // the code is in the table. First emit the string that
                // corresponds to the current code
                auto& currentEntry = table.Get(code);
                Output(output, currentEntry);
                // concatenate the string for the previous code and with
                // first character of the new string
                lzw::ByteArray oldEntry = table.Get(oldCode);
                oldEntry.push_back(currentEntry.front());
                // now add the new entry to the dictionary
                //std::cout << "insert next entry at: " << std::dec << nextCode << std::endl;
                table.Add(oldEntry, nextCode);
                if (nextCode == ((1 << N) - 1)) {
                    ++N;

                }
                ++nextCode;
                oldCode = code;
            }
            else {
                //std::cout << "Code: " << static_cast<unsigned>(code) << " is not in the table" << std::endl;

                // the code is not in the dictionary, hande the speciaml K omega K case
                lzw::ByteArray outString = table.Get(oldCode);
                outString.push_back(outString.front());
                // output the string
                Output(output, outString);
                // add the string to the dictionary
                //std::cout << "insert next entry at: " << std::dec << nextCode << std::endl;
                table.Add(outString, nextCode);
                if (nextCode == ((1 << N) - 1)) {
                    ++N;
                }
                ++nextCode;
                oldCode = code;
            }
        }
    }
}

} // namespace gif
