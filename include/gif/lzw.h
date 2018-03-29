#ifndef GIF_LZW_H
#define GIF_LZW_H

#include <vector>
#include <stdint.h>
#include <string>

namespace gif {

namespace lzw {

using ByteArray = std::vector<uint8_t>;

class DecodeTable
{
public:
	DecodeTable(size_t N);
	const ByteArray& Get(unsigned);
	void Clear();
	bool InTable(unsigned);
	void Add(const ByteArray&, unsigned);

protected:
	std::vector<ByteArray> data_;
	size_t size_;
};

} // namespace lzw

} // namespace gif

#endif // GIF_LZW_H
