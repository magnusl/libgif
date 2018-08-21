/**
 * \file    parser.cpp
 *
 * \author  Magnus Leksell
 */

#include <gif/gif.h>
#include <gif/bit_stream.h>
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

/*****************************************************************************/
/*                                Image decoding                             */
/*****************************************************************************/
namespace {

struct Dictionary
{
    std::array<int16_t, 4096> prefix;
    std::array<int16_t, 4096> length;
    std::array<uint8_t, 4096> byteValue;
    uint16_t minCodeSize;
    uint16_t codeLength;
    uint16_t clearCode;
    uint16_t eoiCode;
    uint16_t currentIndex;
    uint16_t maxCode;
};

// resets a dictionary
void reset(Dictionary& dictionary)
{
    const size_t dictionarySize = (1 << dictionary.minCodeSize);
    dictionary.codeLength = dictionary.minCodeSize + 1;
    dictionary.clearCode = dictionarySize;
    dictionary.eoiCode = dictionary.clearCode + 1;
    dictionary.currentIndex = dictionary.clearCode + 2;
    dictionary.maxCode = (1 << dictionary.codeLength) - 1;
}

// initializes a dictionary with the initial values
void init(Dictionary& dictionary, size_t minCodeSize)
{
    dictionary.minCodeSize = minCodeSize;
    size_t dictionarySize = (1 << minCodeSize);
    for(size_t i = 0; i < dictionarySize; i++) {
        dictionary.prefix[i] = -1;
        dictionary.byteValue[i] = static_cast<uint8_t>(i);
        dictionary.length[i] = 1;
    }
    reset(dictionary);
}

inline size_t add(Dictionary& dictionary, int prefix, uint8_t byteValue)
{
    auto& ci = dictionary.currentIndex;
    if (ci < 4096) {
        if ((ci == dictionary.maxCode) && (dictionary.codeLength < 12)) {
            ++dictionary.codeLength;
            dictionary.maxCode = (1 << dictionary.codeLength) - 1;
        }
        dictionary.prefix[ci] = prefix;
        dictionary.byteValue[ci] = byteValue;
        dictionary.length[ci] = ((prefix < 0) ? 0 : dictionary.length[prefix]) + 1;
        // return the index where the entry was inserted
        return ci++;
    }
    return ci;
}

// get the first byte associated with a dictionary entry
inline uint8_t getFirstByte(
    Dictionary& dictionary,
    size_t index)
{
    size_t i = index;
    while(dictionary.prefix[i] != -1) {
        i = dictionary.prefix[i];
    }
    return dictionary.byteValue[i];
}

/**
 * \struct  DecodeState
 */
struct DecodeState
{
    DecodeState(
        uint8_t minCodeSize,
        const gif::ImageDescriptor& imageDescriptor,
        const gif::ColorTable& table,
        const GraphicControlExtension* graphicControl,
        Frame& framebuffer) :
        descriptor(imageDescriptor),
        colorTable(table),
        gce(graphicControl),
        frame(framebuffer),
        index(0),
        old(0),
        px(descriptor.left),
        py(descriptor.top)
    {
        init(dictionary, minCodeSize);
        // extract the image rectangle
        left = descriptor.left;
        right = left + descriptor.width;
        top = descriptor.top;
        bottom = top + descriptor.height;
    }

    const ImageDescriptor& descriptor;
    const gif::ColorTable& colorTable;
    const GraphicControlExtension* gce;
    Frame& frame;
    Dictionary dictionary;

    // LZW decoding parameters
    int16_t index;
    int16_t old;
    // painting parameters
    size_t px;  // X position of pen
    size_t py;  // Y position of pen
    // image coordinates
    size_t left;
    size_t right;
    size_t top;
    size_t bottom;
};

inline void paint(
    DecodeState& state,
    size_t index)
{
    auto& dictionary = state.dictionary;
    auto length = dictionary.length[index];
    std::array<uint8_t, 4096> buffer;
    // extract the indicies
    size_t bufferIndex = length - 1;
    size_t i = index;
    while(dictionary.prefix[i] != -1) {
        buffer[bufferIndex] = dictionary.byteValue[i];
        i = dictionary.prefix[i];
        --bufferIndex;
    }
    buffer[bufferIndex] = dictionary.byteValue[i];

    // check if a background color has been specified for this frame
    auto& framebuffer = state.frame;
    if (state.gce && state.gce->transparentColorFlag) {
        // ignore transparent color index when painting
        auto tc = state.gce->transparentColorIndex;
        for(i = 0; i < length; ++i) {
            auto colorIndex = buffer[i];
            if (colorIndex != tc) {
                auto& color = state.colorTable[colorIndex];
                framebuffer.setPixel(state.px, state.py, color.r, color.g, color.b);
            }
            // move the pen to the next position
            ++state.px;
            if (state.px >= state.right) {
                ++state.py;
                state.px = state.left;
            }
        }
    }
    else {
        // no transparent color
        for(i = 0; i < length; ++i) {
            auto colorIndex = buffer[i];
            auto& color = state.colorTable[colorIndex]; // check length?
            framebuffer.setPixel(state.px, state.py, color.r, color.g, color.b);
            // move the pen to the next position
            ++state.px;
            if (state.px >= state.right) {
                ++state.py;
                state.px = state.left;
            }
        }

    }
}

inline void readStartIndex(DecodeState& state, BitStream& input)
{
    state.index = input.GetBits(state.dictionary.codeLength);
    paint(state, state.index);
    state.old = state.index;
}

} // namespace

array_view<uint8_t>::const_iterator ParseImageData(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end,
    const gif::ImageDescriptor& descriptor,
    Frame& frame,
    const gif::ColorTable& table,
    const GraphicControlExtension* gce)
{
    // initialize the state
    DecodeState state(ParseByte(it, end), descriptor, table, gce, frame);
    BitStream input(it, end);

    auto& dictionary = state.dictionary;
    if (input.GetBits(dictionary.codeLength) != dictionary.clearCode) {
        throw std::runtime_error("Expected initial clear code");
    }

    readStartIndex(state, input);
    while(1) {
        state.index = input.GetBits(dictionary.codeLength);
        if (state.index < dictionary.currentIndex) { // Does <index> exist in the dictionary?
            if (state.index == dictionary.eoiCode) {
                return input.readDataTerminator();
            }
            else if (state.index == dictionary.clearCode) {
                reset(dictionary);
                readStartIndex(state, input);
            }
            else { // code that already exists
                paint(state, state.index);
                add(dictionary, state.old, getFirstByte(dictionary, state.index));
            }
        }
        else if (state.index == dictionary.currentIndex) {      // <index> does not exist in the dictionary
            auto b = getFirstByte(dictionary, state.old);       // B <- first byte of string at <old>
            paint(state, add(dictionary, state.old, b));
        }
        // <old> <- <index>
        state.old = state.index;
    }
}

} // namespace gif
