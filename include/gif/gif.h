#ifndef LIBGIF_GIF_H
#define LIBGIF_GIF_H

#include "array_view.h"
#include <stdint.h>
#include <vector>
#include <string>
#include <array>

namespace gif {

enum class Version {
    kGif87a,
    kGif89a
};

/**
 * \struct  LogicalScreenDescriptor
 */
struct LogicalScreenDescriptor
{
    uint16_t width;
    uint16_t height;
    uint8_t backgroundColorIndex;
    uint8_t pixelAspectRatio;
    uint8_t globalColorTable        : 1;
    uint8_t colorResolution         : 3;
    uint8_t sortFlag                : 1;
    uint8_t globalColorTableSize    : 3;
};

/**
 * \struct  Color
 */
struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/**
 * \struct  Frame
 */
struct Frame
{
    Frame(size_t w, size_t h) :
        width(w),
        height(h),
        pitch(w * 4),
        pixels(pitch * h)
    {
        // empty
    }

    inline void setPixel(
        size_t x, size_t y, uint8_t r, uint8_t g, uint8_t b)
    {
        const size_t index = (y * pitch) + (x * 4);
        pixels[index] = r;
        pixels[index + 1] = g;
        pixels[index + 2] = b;
        pixels[index + 3] = 0xff;
    }

    uint8_t* rowPointer(size_t y)
    {
        return &pixels[y * pitch];
    }

    size_t width;
    size_t height;
    size_t pitch;
    std::vector<uint8_t> pixels;    // RGBA pixels
};

using ColorTable = std::vector<Color>;

/**
 * \struct  ImageDescriptor
 */
struct ImageDescriptor
{
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
    uint8_t localColorTable     : 1;
    uint8_t interlaced          : 1;
    uint8_t sortFlag            : 1;
    uint8_t localColorTableSize : 3;
};

/**
 * \struct  GraphicControlExtension
 */
struct GraphicControlExtension
{
    uint16_t delayTime;
    uint8_t transparentColorIndex;
    uint8_t disposalMethod          : 3;
    uint8_t userInputFlag           : 1;
    uint8_t transparentColorFlag    : 1;
};

/**
 * \struct  ApplicationExtension
 */
struct ApplicationExtension
{
    std::string identifier;
    std::array<uint8_t, 3> code;
};

uint8_t PeekByte(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a byte from the stream.
 */
uint8_t ParseByte(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a 16-bit unsigned integer from the stream.
 */
uint16_t ParseShort(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a string
 */
std::string ParseString(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end,
    size_t length);

/**
 * \struct  Parses the GIF header
 */
Version ParseHeader(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses the logical screen descriptor
 */
LogicalScreenDescriptor ParseLogicalScreenDescriptor(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a color table
 */
void ParseColorTable(
    ColorTable& table,
    unsigned tableSize,
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a GraphicControlExtension
 */
GraphicControlExtension ParseGraphicControlExtension(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

ApplicationExtension ParseApplicationExtension(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses an ImageDescriptor
 */
ImageDescriptor ParseImageDescriptor(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end);

/**
 * \brief   Parses a image frame
 */
array_view<uint8_t>::const_iterator ParseImageData(
    array_view<uint8_t>::const_iterator& it,
    array_view<uint8_t>::const_iterator end,
    const gif::ImageDescriptor& descriptor,
    Frame& frame,
    const gif::ColorTable& table,
    const GraphicControlExtension* gce);

} // namespace gif

#endif // LIBGIF_GIF_H
