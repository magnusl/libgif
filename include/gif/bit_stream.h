#ifndef GIF_BIT_STREAM_H
#define GIF_BIT_STREAM_H

#include <stdint.h>
#include <string_view>

namespace gif {

class BitStream
{
public:
    BitStream(
        std::basic_string_view<uint8_t>::const_iterator it,
        std::basic_string_view<uint8_t>::const_iterator end);

    unsigned GetBits(size_t count);
    std::basic_string_view<uint8_t>::const_iterator readDataTerminator();

private:
    std::basic_string_view<uint8_t>::const_iterator iterator_;
    std::basic_string_view<uint8_t>::const_iterator end_;
    uint8_t byte_;
    uint8_t bitsLeftInByte_;
    uint8_t bytesInBlock_;
};

} // namespace gif

#endif // GIF_BIT_STREAM_H
