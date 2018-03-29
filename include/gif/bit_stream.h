#ifndef GIF_BIT_STREAM_H
#define GIF_BIT_STREAM_H

#include "array_view.h"
#include <stdint.h>

namespace gif {

class BitStream
{
public:
	BitStream(
		array_view<uint8_t>::const_iterator it,
		array_view<uint8_t>::const_iterator end);

	unsigned GetBits(size_t count);
	unsigned GetBit();

private:
	array_view<uint8_t>::const_iterator iterator_;
	array_view<uint8_t>::const_iterator end_;
	uint8_t byte_;
	uint8_t count_;
	uint8_t bytesInBlock_;
};

} // namespace gif

#endif // GIF_BIT_STREAM_H
