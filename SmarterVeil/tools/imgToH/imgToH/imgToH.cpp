#include "win32_SDK.h"
#include "win32_Helpers.h"
#include "stb_image.h"

#include <shellapi.h>
#include <filesystem>

int _log(int logtype, const wchar_t* _Format,...)
{
    switch (logtype) {
    case 0:  wprintf(L"[INFO] "); break;
    case 1:  wprintf(L"[WARNING] "); break;
    case 2:  wprintf(L"[ERROR] "); break;
    default:  wprintf(L"[Invalid log type] "); break;
    }
    int _Result;
    va_list _ArgList;
    __crt_va_start(_ArgList, _Format);
    _Result = _vfwprintf_l(stdout, _Format, NULL, _ArgList);
    __crt_va_end(_ArgList);
    wprintf(L"\n");
    return _Result;
}
#define LOG(logtype,fmt,...) _log(logtype,fmt,__VA_ARGS__)
#define LOG_INFO(fmt,...) LOG(0,fmt,__VA_ARGS__)
#define LOG_WARNING(fmt,...) LOG(1,fmt,__VA_ARGS__)
#define LOG_ERROR(fmt,...) LOG(2,fmt,__VA_ARGS__)

namespace codegen {
    std::string/*utf8*/ create_img_h(const u8* img, int width, int height, int n/*# of components per pixel*/, const utf8* img_name, bool premultiply) {
        using namespace std::string_literals;
        std::string res;
        if (n == 1) {
            std::string type;
            std::string byteswap;

            if (width != height) LOG_WARNING(L"Image width(%d) and height(%d) dont match: %s", width, height, img_name);
            switch (width) {
            case 16: type = "u16"; byteswap = "bswap16"; break;
            case 32: type = "u32"; byteswap = "bswap32"; break;
            default: LOG_ERROR(L"No support for %d pixel wide images (TODO): %s", width, img_name); return "";
            }

            const std::string common =
                "#include <cstdint>\n"
                "typedef uint8_t u8;\n"
                "typedef uint16_t u16;\n"
                "typedef uint32_t u32;\n"
                "typedef uint64_t u64;\n"
                "#define lo16(x) ((u16)((x) & 0xffff))\n"
                "#define hi16(x) ((u16)(((x) >> 16) & 0xffff))\n"
                "#define bswap16(x) ((u16)((x>>8) | (x<<8)))\n"
                "#define bswap32(x) ((u32)((((u32)bswap16(lo16(x)))<<16) | ((u32)bswap16(hi16(x)))))\n"
                "struct def_img {\n"
                "\tint bpp;\n"
                "\tint w;\n"
                "\tint h;\n"
                "\tvoid* mem;\n"
                "};)\n";

            res =
                common + "\n"
                "static constexpr " + type + " " + "_" + img_name + "[" + std::to_string(width) + "]" + " = " "//1bpp " + std::to_string(width) + "x" + std::to_string(height) + "\n"
                "{" "\n";

            const u8* mem = img;
            for (int y = 0; y < height; y++) {
                res += "\t" + byteswap + "(0b";
                for (int x = 0; x < width; x++) {
                    if (x && x % 4 == 0) res += "'";
                    //IMPORTANT: we assume perfect alignment in between rows, this may not be correct but right now idk how to ask stb_image for it's stride
                    //NOTE: each byte represents one bit of image information (since we're storing 1bpp imgs)
                    u8 bit = *mem++;
                    if (bit) res += "1";
                    else res += "0";
                }
                res += "),\n";
            }
            res += "};\n";

            res += "constexpr def_img "s + img_name + "{ 1" ", " + std::to_string(width) + ", " + std::to_string(height) + ", " "(void*)" + "_" + img_name + " };\n";

        }
        else if (n==4) {
            //NOTE(fran): all images will have alpha (4 channels:rgba (not necessarily in that order)), images with only rgb are automatically converted by stb_image to rgba
            std::string type="u32";
            std::string bpp= std::to_string(n*8);
            res =
                "static constexpr " + type + " " + "_" + img_name + "[" + std::to_string(width*height) + "]" + " = " "//" + bpp + "bpp " + std::to_string(width) + "x" + std::to_string(height) + "\n"
                "{" "\n";

            const u32* mem = (decltype(mem))img;
            for (int y = 0; y < height; y++) {
                res += "\t";
                for (int x = 0; x < width; x++) {
                    //IMPORTANT: we assume perfect alignment in between rows, this may not be correct but right now idk how to ask stb_image for it's stride
                    u32 pixel = *mem++;

                    u8 r = (pixel>>0)  & 0xff;
                    u8 g = (pixel>>8)  & 0xff;
                    u8 b = (pixel>>16) & 0xff;
                    u8 a = (pixel>>24) & 0xff;

                    f32 scale = (f32)a / 255.f;

                    r = (u8)((f32)r * scale);
                    g = (u8)((f32)g * scale);
                    b = (u8)((f32)b * scale);

                    pixel = (((u32)r) << 0) | (((u32)g) << 8) | (((u32)b) << 16) | (((u32)a) << 24);

                    res += std::format("{:#X}", pixel) + ","; //TODO(fran): better packing, try base64
                }
                res += "\n";
            }

            res += "};\n";

            res += "constexpr embedded_img "s + img_name + "{ .bpp = " + bpp + "u" ", " ".w = " + std::to_string(width) + "u" ", " + ".h = " + std::to_string(height) + "u" ", " ".mem = " "(void*)" + "_" + img_name + " };\n";
        }
        return res;
    }
}

void make_h_file(s16 img_file, std::string& common_include_code)//TODO(fran): may be better to simply receive utf8
{
    LOG_INFO(L"Parsing image: %ws", img_file.str);

    s8 img_file8 = s16_to_s8(img_file); defer{ free_any_str(img_file8); };
    int x,y,n; // x = width, y = height, n = # of 8-bit components per pixel
    int desired_nro_components = 4;
    u8 *data = stbi_load(img_file8, &x, &y, &n, desired_nro_components); defer{ if (data)stbi_image_free(data); };
    if (data) {
        typedef std::filesystem::path filepath;
        const filepath img_filepath(img_file.str);//TODO(fran): should be stringview
        #define img_filename img_filepath.filename().c_str()

        if (n != 3 && n != 4) {
            LOG_ERROR(L"Invalid component count per pixel, expected %ws but got %d: %ws",L"[3,4]", n, img_filename);
            return; 
        }
        if (x <= 0 || y <= 0) { 
            LOG_ERROR(L"Width or height are zero or negative: %ws", n, img_filename); 
            return;
        }

        std::string code = codegen::create_img_h(data,x,y,desired_nro_components,(utf8*)img_filepath.stem().u8string().c_str(), true); //TODO(fran): pngs for example are always straight alpha, we should ask stb_image which image type it was and base our premultiply decision on that
        if (!code.empty()) {
            filepath h_filepath = img_filepath; h_filepath.replace_extension(L".h");
            bool write_res = write_entire_file(h_filepath.c_str(), (void*)code.c_str(), (u32)(code.length() * sizeof(code[0])));
            if (write_res) {
                LOG_INFO(L"Image saved %ws as %ws", img_file.str, h_filepath.c_str());

                auto filename = h_filepath.filename().u8string();
                common_include_code += "#include \"";
                common_include_code += (const char*)h_filepath.filename().u8string().c_str();
                common_include_code += "\"\n";;

            }
            else LOG_ERROR(L"Failed to save image: %ws", img_file.str);
        }

    }
    else LOG_ERROR(L"Couldn't read file, it does not exist or has an invalid image type or format: %ws",img_file.str);
}

int main() {
    //TODO(fran): make OS independent, right now it only works on Windows
    //TODO(fran): allow to define order of components in the pixel, eg Windows uses BGRA instead of RGBA
    int cnt;
    LPWSTR* args= CommandLineToArgvW(GetCommandLineW(), &cnt); defer{ if(args)LocalFree(args); };
    LOG_INFO(L"Supported Image file formats: JPEG - PNG - BMP - GIF - PSD - PIC - PNM - TGA");
    if (!args) LOG_ERROR(L"Failed to retrieve command line arguments");
    //TODO(fran): if args<=1 then check the working directory for image files and convert them all
    else if(cnt<=1) LOG_ERROR(L"No arguments specified, usage: imgToH img1.png img2.bmp ...");
    else {
        std::string common_include_code = 
            "struct embedded_img {\n"
            "\tu32 bpp; // bits per pixel\n"
            "\tu32 w;\n"
            "\tu32 h;\n"
            "\tvoid* mem;\n"
            "};\n\n";
        
        for (int i = 1; i < cnt; i++) make_h_file({ args[i], (wcslen(args[i]) + 1) * sizeof(*args[i]) }, common_include_code);
        
        std::wstring common_include_file = L"embedded_images.h";
        bool write_res = write_entire_file(common_include_file.c_str(), (void*)common_include_code.c_str(), (u32)(common_include_code.length() * sizeof(common_include_code[0])));
        if (write_res) LOG_INFO(L"Common include file saved as %ws", common_include_file.c_str());
        else LOG_ERROR(L"Failed to save Common include file: %ws", common_include_file.c_str());
    }

    //TODO(fran): create a common include file (embedded_images.h) that includes all the other .h images
}
