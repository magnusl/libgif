#include <gif/gif.h>
#include <cstdio>
#include <iostream>
#include <SDL.h>
#include <boost/optional.hpp>

#define WINDOW_WIDTH (1280)
#define WINDOW_HEIGHT (720)

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

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "GIF",                             // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        WINDOW_WIDTH,                      // width, in pixels
        WINDOW_HEIGHT,                     // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );

    if (!window) {
        std::cerr << "Failed to create SDL window" << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "Failed to create SDL renderer" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    Uint32 rmask, gmask, bmask, amask;

    /* SDL interprets each pixel as a 32-bit number, so our masks must depend
       on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    array_view<uint8_t> view(file);

    array_view<uint8_t>::const_iterator it = view.begin();
    array_view<uint8_t>::const_iterator end = view.end();

    try {
        // read the GIF header
        auto header = gif::ParseHeader(it, end);
        auto lsd = gif::ParseLogicalScreenDescriptor(it, end);

        gif::Frame framebuffer(lsd.width, lsd.height);

        SDL_Surface* surface = SDL_CreateRGBSurface(0, lsd.width, lsd.height, 32, rmask, gmask, bmask, amask);
        assert(surface);

        gif::ColorTable globalColorTable;
        if (lsd.globalColorTable) {
            gif::ParseColorTable(globalColorTable, 1 << (lsd.globalColorTableSize + 1), it, end);
        }

        SDL_Event e;
        bool quit = false;
        SDL_Texture* fbTexture = nullptr;

        boost::optional<gif::GraphicControlExtension> gce;
        while (!quit) {
            while (SDL_PollEvent(&e)){
                if (e.type == SDL_QUIT){
                    quit = true;
                }
            }

            if (it == end) {
                continue;
            }

            auto byte = gif::PeekByte(it, end);
            switch(byte)
            {
            case 0x21:
                {
                    // parse an extension
                    ++it;   // consume the extension introducer
                    switch(gif::PeekByte(it, end)) {
                    case 0xF9:
                        {
                            gce = gif::ParseGraphicControlExtension(it, end);
                            break;
                        }
                    case 0xFF:
                        {
                            gif::ApplicationExtension ae = gif::ParseApplicationExtension(it, end);
                            break;
                        }
                    case 0xFE:
                        {
                            // comment section
                            ++it;
                            auto length = gif::ParseByte(it, end);
                            while(length > 0) {
                                it = it + length;
                                length = gif::ParseByte(it, end);
                            }
                            break;
                        }
                    default:
                        break;
                    }
                    break;
                }
            case 0x2c:
                {
                    // image descriptor
                    const gif::ImageDescriptor descriptor = gif::ParseImageDescriptor(it, end);
                    gif::ColorTable localColorTable;
                    if (descriptor.localColorTable) {
                        // parse color table
                        gif::ParseColorTable(localColorTable, 1 << (descriptor.localColorTableSize + 1), it, end);
                    }
                    // reject interlaced images for now
                    if (descriptor.interlaced) {
                        throw std::runtime_error("Interlaced images are currently not supported.");
                    }

                    it = ParseImageData(
                        it,
                        end,
                        descriptor,
                        framebuffer,
                        descriptor.localColorTable ? localColorTable : globalColorTable,
                        (gce ? gce.get_ptr() : nullptr));

                    // now show the updated frame
                    SDL_LockSurface(surface);
                    for(size_t y = 0; y < framebuffer.height; ++y ) {
                        auto rowp = reinterpret_cast<uint8_t*>(surface->pixels) + (y * (surface->pitch));
                        memcpy(rowp, framebuffer.rowPointer(y), framebuffer.pitch);
                    }
                    SDL_UnlockSurface(surface);

                    if (fbTexture) {
                        SDL_DestroyTexture(fbTexture);
                    }
                    fbTexture = SDL_CreateTextureFromSurface(renderer, surface);
                    assert(fbTexture);

                    SDL_Rect dstRect;
                    dstRect.x = 0;
                    dstRect.y = 0;
                    dstRect.w = framebuffer.width;
                    dstRect.h = framebuffer.height;

                    SDL_RenderClear(renderer);
                    if (fbTexture) {
                        SDL_RenderCopy(renderer, fbTexture, nullptr, &dstRect);
                    }

                    SDL_RenderPresent(renderer);

                    if (gce) {
                        if (gce->delayTime) {
                            // delay
                            SDL_Delay(10 * gce->delayTime);
                        }
                        else {
                            SDL_Delay(100);
                        }

                        // check the disposal method
                        switch(gce->disposalMethod) {
                        case 0:
                        case 1:
                            break;
                        case 2:
                            break;
                        case 3:
                            break;
                        default:
                            break;
                        }

                        gce = boost::none;
                    }

                    break;
                }
            case 0xff:
                {
                    auto ext = gif::ParseApplicationExtension(it, end);
                    break;
                }
            case 0x3b:
                {
                    ++it;
                    break;
                }
            default:
                std::cout << "Unhandled: " << std::hex << ((unsigned) byte) << std::endl;
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
