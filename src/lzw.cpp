#include <gif/lzw.h>
#include <assert.h>
#include <iostream>

namespace gif {

namespace lzw {

DecodeTable::DecodeTable(size_t N) :
	size_(N)
{
	Clear();
}

const ByteArray& DecodeTable::Get(unsigned index)
{
	std::cout << "get index: " << index << " size=" << data_.size() << std::endl;
	return data_.at(index);
}

void DecodeTable::Clear()
{
	data_.clear();
	data_.reserve(4096);

	for(size_t i = 0; i < (1 << size_); ++i) {
		ByteArray entry = { static_cast<uint8_t>(i) };
		std::cout << "Add table entry: " << i << std::endl;
		data_.push_back(entry);
	}
	// add the two special cases
	ByteArray dummy;
	data_.push_back(dummy);
	data_.push_back(dummy);
	std::cout << "Table size is: " << data_.size() << std::endl;
}

bool DecodeTable::InTable(unsigned code)
{
	return code < data_.size();
}

void DecodeTable::Add(const ByteArray& input, unsigned code)
{
	std::cout << "Current size=" << data_.size() << " next code=" << code << std::endl;
	assert(data_.size() == code);
	data_.push_back(input);
}

} // namespace lzw

} // namespace gif
