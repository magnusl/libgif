#include <gif/bit_stream.h>
#include <gif/gif.h>

namespace gif {

BitStream::BitStream(
	array_view<uint8_t>::const_iterator it,
	array_view<uint8_t>::const_iterator end) :
	iterator_(it),
	end_(end),
	byte_(0),
	count_(0)
{
	// empty
	bytesInBlock_ = ParseByte(iterator_, end_);
}

unsigned BitStream::GetBits(size_t count)
{
	unsigned result = 0;
	for(size_t i = 0; i < count; ++i) {
		result |= (GetBit() << i);
	}

	return result;
}

unsigned BitStream::GetBit()
{
	if (count_ == 0) {
		if (iterator_ == end_) {
			throw std::runtime_error(
				"Reached EOF, can't read any more bytes from input.");
		}

		if (bytesInBlock_ > 0) {
			std::cout << "Bytes left: " << std::dec << static_cast<unsigned>(bytesInBlock_) << std::endl;
			// there are still data in the block
			--bytesInBlock_;
		}
		else {
			bytesInBlock_ = ParseByte(iterator_, end_);
			if (!bytesInBlock_) {
				throw std::runtime_error("Reached null terminator block.");
			}
		}

		byte_ = *iterator_;
		count_ = 8;
		++iterator_;
	}

	--count_;
	unsigned result = byte_ & 0x01;
	byte_ >>= 1;
	return result;
}

} // namespace gif
