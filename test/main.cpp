#include <gif/gif.h>
#include <cstdio>
#include <iostream>

bool ReadFile(const char* path, std::vector<uint8_t>& result)
{
	if (!path) {
		return false;
	}

	FILE* fp = fopen(path, "rb");
	if (!fp) {
		return false;
	}

	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return false;
	}

	long size = ftell(fp);
	if (size <= 0) {
		fclose(fp);
		return false;
	}

	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return false;
	}

	result.resize(size);
	if (fread(result.data(), result.size(), 1, fp) != 1) {
		fclose(fp);
		return false;
	}

	fclose(fp);
	return true;
}

std::ostream& operator<<(std::ostream& os, const gif::LogicalScreenDescriptor& lsd)
{
	os  << "{ width: " << lsd.width
		<< " height: " << lsd.height
		<< " globalColorTable: " << static_cast<unsigned>(lsd.globalColorTable)
		<< " globalColorTableSize: " << static_cast<unsigned>(lsd.globalColorTableSize)
		<< "}";
	return os;
}

int main(int argc, char**argv)
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " [filename]" << std::endl;
		return -1;
	}

	std::vector<uint8_t> file;
	if (!ReadFile(argv[1], file)) {
		std::cerr << "Failed to read file." << std::endl;
		return -1;
	}

	array_view<uint8_t> view(file);

	array_view<uint8_t>::const_iterator it = view.begin();
	array_view<uint8_t>::const_iterator end = view.end();

	try {
		// read the GIF header
		auto header = gif::ParseHeader(it, end);
		auto lsd = gif::ParseLogicalScreenDescriptor(it, end);

		gif::ColorTable globalColorTable;
		if (lsd.globalColorTable) {
			gif::ParseColorTable(globalColorTable, 1 << (lsd.globalColorTableSize + 1), it, end);
		}

		while(it != end) {
			auto byte = gif::PeekByte(it, end);
			std::cout << "process byte: " << std::hex << static_cast<unsigned>(byte) << std::endl;
			switch(byte)
			{
			case 0x21:
				{
					// parse an extension
					++it; 	// consume the extension introducer
					switch(gif::PeekByte(it, end)) {
					case 0xF9:
						{
							std::cout << "Parse GraphicControlExtension" << std::endl;
							gif::GraphicControlExtension gce = gif::ParseGraphicControlExtension(it, end);
							break;
						}
					case 0xFF:
						{
							std::cout << "Parse ApplicationExtension" << std::endl;
							gif::ApplicationExtension ae = gif::ParseApplicationExtension(it, end);
							break;
						}
					default:
						std::cout << "Unknown extension" << std::endl;
						break;
					}
					break;
				}
			case 0x2c:
				{
					// image descriptor
					std::cout << "Parse image descriptor" << std::endl;
					const gif::ImageDescriptor descriptor = gif::ParseImageDescriptor(it, end);

					std::cout << "{" << descriptor.top << ", " << descriptor.left << "," << descriptor.width << ", height: " << descriptor.height << std::endl;

					gif::ColorTable localColorTable;
					if (descriptor.localColorTable) {
						// parse color table
						gif::ParseColorTable(globalColorTable, 1 << (descriptor.localColorTableSize + 1), it, end);
					}
					// reject interlaced images for now
					if (descriptor.interlaced) {
						throw std::runtime_error("Interlaced images are currently not supported.");
					}
					// now parse the image data
					std::vector<uint8_t> crap;
					gif::ParseImageData(it, end, crap);
					break;
				}
			default:
				std::cout << "Unknown byte: " << std::hex << static_cast<unsigned>(byte) << std::endl;
				return -1;
			}
		}
	}
	catch(std::exception& err) {
		std::cerr << "Caught exception: " << err.what() << std::endl;
		return -1;
	}

	return 0;
}
