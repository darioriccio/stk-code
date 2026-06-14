// Minimal macOS Syphon receiver probe for Immersive Relay validation.
// It is a development/test tool, not the production Swift dome receiver.

#import <Foundation/Foundation.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <Syphon/Syphon.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

namespace
{

struct Options
{
    std::string server_name = "SuperTuxKart Immersive Relay Domemaster";
    int expected_width = 0;
    int expected_height = 0;
    double timeout_seconds = 15.0;
    double duration_seconds = 5.0;
    int min_frames = 10;
    int sample_every = 10;
    bool check_orientation_grid = true;
    std::string save_ppm;
};

struct Pixel
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct TextureSample
{
    int width = 0;
    int height = 0;
    double non_black_ratio = 0.0;
    uint64_t checksum = 0;
    Pixel center = {0, 0, 0, 0};
    Pixel left = {0, 0, 0, 0};
    Pixel right = {0, 0, 0, 0};
    Pixel up = {0, 0, 0, 0};
    Pixel back = {0, 0, 0, 0};
    bool gl_ok = false;
    std::vector<unsigned char> pixels;
};

void printUsage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "  --server-name <name>\n"
        << "  --expected-width <pixels>\n"
        << "  --expected-height <pixels>\n"
        << "  --duration <seconds>\n"
        << "  --timeout <seconds>\n"
        << "  --min-frames <count>\n"
        << "  --sample-every <count>\n"
        << "  --save-ppm <path>\n"
        << "  --orientation-grid on|off\n";
}

bool parseArgs(int argc, char** argv, Options* options)
{
    for (int i = 1; i < argc; i++)
    {
        const std::string arg = argv[i];
        auto needValue = [&](const char* name) -> const char*
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        else if (arg == "--server-name")
        {
            const char* value = needValue("--server-name");
            if (!value) return false;
            options->server_name = value;
        }
        else if (arg == "--expected-width")
        {
            const char* value = needValue("--expected-width");
            if (!value) return false;
            options->expected_width = std::atoi(value);
        }
        else if (arg == "--expected-height")
        {
            const char* value = needValue("--expected-height");
            if (!value) return false;
            options->expected_height = std::atoi(value);
        }
        else if (arg == "--duration")
        {
            const char* value = needValue("--duration");
            if (!value) return false;
            options->duration_seconds = std::atof(value);
        }
        else if (arg == "--timeout")
        {
            const char* value = needValue("--timeout");
            if (!value) return false;
            options->timeout_seconds = std::atof(value);
        }
        else if (arg == "--min-frames")
        {
            const char* value = needValue("--min-frames");
            if (!value) return false;
            options->min_frames = std::atoi(value);
        }
        else if (arg == "--sample-every")
        {
            const char* value = needValue("--sample-every");
            if (!value) return false;
            options->sample_every = std::max(1, std::atoi(value));
        }
        else if (arg == "--save-ppm")
        {
            const char* value = needValue("--save-ppm");
            if (!value) return false;
            options->save_ppm = value;
        }
        else if (arg == "--orientation-grid")
        {
            const char* value = needValue("--orientation-grid");
            if (!value) return false;
            options->check_orientation_grid =
                std::strcmp(value, "off") != 0 &&
                std::strcmp(value, "false") != 0 &&
                std::strcmp(value, "0") != 0;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

double nowSeconds()
{
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto elapsed = clock::now() - start;
    return std::chrono::duration<double>(elapsed).count();
}

uint64_t fnv1a(const std::vector<unsigned char>& bytes)
{
    uint64_t hash = 1469598103934665603ULL;
    const size_t stride = std::max<size_t>(1, bytes.size() / 65536);
    for (size_t i = 0; i < bytes.size(); i += stride)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

Pixel pixelAt(const TextureSample& sample, double nx, double ny)
{
    const int x = std::max(0, std::min(sample.width - 1,
        (int)std::floor(nx * (double)(sample.width - 1) + 0.5)));
    const int y = std::max(0, std::min(sample.height - 1,
        (int)std::floor(ny * (double)(sample.height - 1) + 0.5)));
    const size_t index = ((size_t)y * (size_t)sample.width + (size_t)x) * 4;
    if (index + 3 >= sample.pixels.size())
        return {0, 0, 0, 0};
    return {sample.pixels[index + 0], sample.pixels[index + 1],
            sample.pixels[index + 2], sample.pixels[index + 3]};
}

bool readTexture(GLuint texture, int width, int height, TextureSample* sample)
{
    sample->width = width;
    sample->height = height;
    sample->pixels.assign((size_t)width * (size_t)height * 4, 0);

    GLint previous_fbo = 0;
    GLint previous_viewport[4] = {0, 0, 0, 0};
    GLint previous_texture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_fbo);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_EXT, &previous_texture);

    GLuint fbo = 0;
    GLuint target_texture = 0;
    glGenTextures(1, &target_texture);
    glBindTexture(GL_TEXTURE_2D, target_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, target_texture, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnable(GL_TEXTURE_RECTANGLE_EXT);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, texture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f((GLfloat)width, 0.0f);
    glVertex2f((GLfloat)width, 0.0f);
    glTexCoord2f((GLfloat)width, (GLfloat)height);
    glVertex2f((GLfloat)width, (GLfloat)height);
    glTexCoord2f(0.0f, (GLfloat)height);
    glVertex2f(0.0f, (GLfloat)height);
    glEnd();

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                 sample->pixels.data());
    const GLenum err = glGetError();

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, (GLuint)previous_texture);
    glDisable(GL_TEXTURE_RECTANGLE_EXT);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_fbo);
    glViewport(previous_viewport[0], previous_viewport[1],
               previous_viewport[2], previous_viewport[3]);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &target_texture);
    if (err != GL_NO_ERROR)
    {
        std::cerr << "Syphon texture readback failed with error 0x"
                  << std::hex << err << std::dec << "\n";
        return false;
    }

    size_t non_black = 0;
    const size_t pixel_count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < pixel_count; i++)
    {
        const unsigned char r = sample->pixels[i * 4 + 0];
        const unsigned char g = sample->pixels[i * 4 + 1];
        const unsigned char b = sample->pixels[i * 4 + 2];
        if (r > 8 || g > 8 || b > 8)
            non_black++;
    }

    sample->non_black_ratio = pixel_count > 0
        ? (double)non_black / (double)pixel_count : 0.0;
    sample->checksum = fnv1a(sample->pixels);
    sample->center = pixelAt(*sample, 0.50, 0.50);
    sample->left = pixelAt(*sample, 0.11, 0.50);
    sample->right = pixelAt(*sample, 0.89, 0.50);
    sample->up = pixelAt(*sample, 0.50, 0.89);
    sample->back = pixelAt(*sample, 0.50, 0.11);
    sample->gl_ok = true;
    return true;
}

bool savePpm(const TextureSample& sample, const std::string& path)
{
    if (sample.pixels.empty())
        return false;
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out << "P6\n" << sample.width << " " << sample.height << "\n255\n";
    for (int y = sample.height - 1; y >= 0; y--)
    {
        for (int x = 0; x < sample.width; x++)
        {
            const size_t index =
                ((size_t)y * (size_t)sample.width + (size_t)x) * 4;
            const char rgb[3] = {
                (char)sample.pixels[index + 0],
                (char)sample.pixels[index + 1],
                (char)sample.pixels[index + 2]
            };
            out.write(rgb, 3);
        }
    }
    return true;
}

std::string pixelString(const Pixel& p)
{
    return std::to_string((int)p.r) + "," +
           std::to_string((int)p.g) + "," +
           std::to_string((int)p.b);
}

bool isRed(const Pixel& p)
{
    return p.r > 120 && p.r > p.g * 1.5 && p.r > p.b * 1.5;
}

bool isGreen(const Pixel& p)
{
    return p.g > 120 && p.g > p.r * 1.4 && p.g > p.b * 1.2;
}

bool isMagenta(const Pixel& p)
{
    return p.r > 120 && p.b > 120 && p.g < 150;
}

bool isCyan(const Pixel& p)
{
    return p.g > 120 && p.b > 120 && p.r < 150;
}

bool isOrange(const Pixel& p)
{
    return p.r > 120 && p.g > 45 && p.r > p.b * 2.0;
}

template<typename Predicate>
bool hasColorNear(const TextureSample& sample, double nx, double ny,
                  int radius, Predicate predicate)
{
    const int cx = std::max(0, std::min(sample.width - 1,
        (int)std::floor(nx * (double)(sample.width - 1) + 0.5)));
    const int cy = std::max(0, std::min(sample.height - 1,
        (int)std::floor(ny * (double)(sample.height - 1) + 0.5)));
    const int r2 = radius * radius;
    for (int y = std::max(0, cy - radius);
         y <= std::min(sample.height - 1, cy + radius); y++)
    {
        for (int x = std::max(0, cx - radius);
             x <= std::min(sample.width - 1, cx + radius); x++)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy > r2)
                continue;
            const size_t index =
                ((size_t)y * (size_t)sample.width + (size_t)x) * 4;
            if (index + 3 >= sample.pixels.size())
                continue;
            Pixel p = {sample.pixels[index + 0],
                       sample.pixels[index + 1],
                       sample.pixels[index + 2],
                       sample.pixels[index + 3]};
            if (predicate(p))
                return true;
        }
    }
    return false;
}

CGLContextObj createContext()
{
    CGLPixelFormatObj pixel_format = nullptr;
    GLint pixel_format_count = 0;
    CGLPixelFormatAttribute attrs[] = {
        kCGLPFAAccelerated,
        kCGLPFAAllowOfflineRenderers,
        (CGLPixelFormatAttribute)0
    };
    CGLError err = CGLChoosePixelFormat(attrs, &pixel_format,
                                        &pixel_format_count);
    if (err != kCGLNoError || !pixel_format)
    {
        CGLPixelFormatAttribute fallback_attrs[] = {
            kCGLPFAAllowOfflineRenderers,
            (CGLPixelFormatAttribute)0
        };
        err = CGLChoosePixelFormat(fallback_attrs, &pixel_format,
                                   &pixel_format_count);
    }
    if (err != kCGLNoError || !pixel_format)
        return nullptr;

    CGLContextObj context = nullptr;
    err = CGLCreateContext(pixel_format, nullptr, &context);
    CGLDestroyPixelFormat(pixel_format);
    if (err != kCGLNoError || !context)
        return nullptr;
    CGLSetCurrentContext(context);
    return context;
}

NSDictionary<NSString*, id>* waitForServer(const Options& options)
{
    NSString* name = [NSString stringWithUTF8String:options.server_name.c_str()];
    const double deadline = nowSeconds() + options.timeout_seconds;
    while (nowSeconds() < deadline)
    {
        @autoreleasepool
        {
            NSArray* matches =
                [[SyphonServerDirectory sharedDirectory]
                    serversMatchingName:name appName:nil];
            if ([matches count] > 0)
                return [[matches objectAtIndex:0] retain];
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    }
    return nil;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArgs(argc, argv, &options))
    {
        printUsage(argv[0]);
        return 2;
    }

    @autoreleasepool
    {
        CGLContextObj context = createContext();
        if (!context)
        {
            std::cerr << "SYPHON_PROBE FAIL reason=no_opengl_context\n";
            return 3;
        }

        NSDictionary<NSString*, id>* server_description =
            waitForServer(options);
        if (!server_description)
        {
            std::cerr << "SYPHON_PROBE FAIL reason=server_not_found"
                      << " server=\"" << options.server_name << "\"\n";
            CGLDestroyContext(context);
            return 4;
        }

        SyphonOpenGLClient* client =
            [[SyphonOpenGLClient alloc]
                initWithServerDescription:server_description
                                   context:context
                                   options:nil
                           newFrameHandler:nil];
        [server_description release];

        if (!client || ![client isValid])
        {
            std::cerr << "SYPHON_PROBE FAIL reason=client_invalid\n";
            if (client) [client release];
            CGLDestroyContext(context);
            return 5;
        }

        const double start = nowSeconds();
        const double end = start + options.duration_seconds;
        int frames = 0;
        int readbacks = 0;
        int width = 0;
        int height = 0;
        std::set<uint64_t> unique_hashes;
        TextureSample last_sample;

        while (nowSeconds() < end)
        {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, false);
            if (![client isValid])
                break;

            if ([client hasNewFrame] || frames == 0)
            {
                SyphonOpenGLImage* image = [client newFrameImage];
                if (image)
                {
                    frames++;
                    width = (int)std::lround([image textureSize].width);
                    height = (int)std::lround([image textureSize].height);
                    if (frames == 1 || frames % options.sample_every == 0)
                    {
                        TextureSample sample;
                        if (readTexture([image textureName], width, height,
                                        &sample))
                        {
                            readbacks++;
                            unique_hashes.insert(sample.checksum);
                            last_sample = sample;
                        }
                    }
                    [image release];
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        const double elapsed = std::max(0.001, nowSeconds() - start);
        const bool size_ok =
            (options.expected_width <= 0 || width == options.expected_width) &&
            (options.expected_height <= 0 || height == options.expected_height);
        const bool frames_ok = frames >= options.min_frames;
        const bool non_black_ok = last_sample.non_black_ratio > 0.01;
        const bool changing_ok = unique_hashes.size() > 1;
        bool orientation_ok = true;
        if (options.check_orientation_grid && last_sample.gl_ok)
        {
            const int marker_radius =
                std::max(24, std::min(width, height) / 10);
            orientation_ok =
                hasColorNear(last_sample, 0.50, 0.50, marker_radius, isCyan) &&
                hasColorNear(last_sample, 0.50, 0.89, marker_radius, isRed) &&
                hasColorNear(last_sample, 0.89, 0.50, marker_radius, isGreen) &&
                hasColorNear(last_sample, 0.11, 0.50, marker_radius, isMagenta) &&
                hasColorNear(last_sample, 0.50, 0.11, marker_radius, isOrange);
        }

        if (!options.save_ppm.empty())
        {
            if (savePpm(last_sample, options.save_ppm))
                std::cout << "SYPHON_PROBE saved_ppm=" << options.save_ppm << "\n";
            else
                std::cout << "SYPHON_PROBE save_ppm_failed=" << options.save_ppm << "\n";
        }

        const bool pass = size_ok && frames_ok && non_black_ok &&
            changing_ok && orientation_ok;
        std::cout
            << "SYPHON_PROBE " << (pass ? "PASS" : "FAIL")
            << " server=\"" << options.server_name << "\""
            << " received=" << width << "x" << height
            << " expected=" << options.expected_width << "x"
            << options.expected_height
            << " frames=" << frames
            << " duration=" << elapsed
            << " fps=" << ((double)frames / elapsed)
            << " readbacks=" << readbacks
            << " unique_samples=" << unique_hashes.size()
            << " non_black_ratio=" << last_sample.non_black_ratio
            << " orientation_grid="
            << (orientation_ok ? "pass" : "fail")
            << " center_rgb=" << pixelString(last_sample.center)
            << " left_rgb=" << pixelString(last_sample.left)
            << " right_rgb=" << pixelString(last_sample.right)
            << " up_rgb=" << pixelString(last_sample.up)
            << " back_rgb=" << pixelString(last_sample.back)
            << "\n";

        [client stop];
        [client release];
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(context);
        return pass ? 0 : 1;
    }
}
