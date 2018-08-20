#include <gif/bit_stream.h>
#include <gif/gif.h>
#include <assert.h>

namespace gif {

BitStream::BitStream(
    array_view<uint8_t>::const_iterator it,
    array_view<uint8_t>::const_iterator end) :
    iterator_(it),
    end_(end),
    byte_(0),
    bitsLeftInByte_(0)
{
    bytesInBlock_ = ParseByte(iterator_, end_);
}

unsigned BitStream::GetBits(size_t count)
{
    unsigned result = 0;
    for(size_t i = 0; i < count; ++i) {
        if (bitsLeftInByte_ == 0) {
            if (iterator_ == end_) {
                throw std::runtime_error(
                    "Reached EOF, can't read any more bytes from input.");
            }
            if (bytesInBlock_ > 0) {
                // there are still data in the block
                --bytesInBlock_;
            }
            else {
                bytesInBlock_ = ParseByte(iterator_, end_);
                if (!bytesInBlock_) {
                    throw std::runtime_error("Reached null terminator block.");
                }
                --bytesInBlock_;
            }

            byte_ = *iterator_;
            bitsLeftInByte_ = 8;
            ++iterator_;
        }
        --bitsLeftInByte_;

        result |= (byte_ & 0x01) << i;
        byte_ >>= 1;
    }

    return result;
}

array_view<uint8_t>::const_iterator BitStream::readDataTerminator()
{
    // skip any remaining bytes in the current block
    while(bytesInBlock_ > 0) {
        ++iterator_;
        --bytesInBlock_;
    }
    // now read the null terminator block
    if (ParseByte(iterator_, end_) != 0x00) {
        throw std::runtime_error("Expected null terminator block");
    }
    // return a iterator to the data that folloes the terminator block
    return iterator_;
}

} // namespace gif
