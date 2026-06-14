//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#if defined(ENABLE_IMMERSIVE_RELAY) && defined(ENABLE_SYPHON_OUTPUT)

#include "modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.hpp"

#include "graphics/gl_headers.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <Syphon/Syphon.h>

namespace ImmersiveRelay
{

SyphonOutputAdapterMac::SyphonOutputAdapterMac()
{
    m_server = NULL;
    m_initialized = false;
    m_unavailable_logged = false;
    m_width = 0;
    m_height = 0;
}

SyphonOutputAdapterMac::~SyphonOutputAdapterMac()
{
    shutdown();
}

bool SyphonOutputAdapterMac::initialize(const std::string& server_name,
                                       unsigned int width,
                                       unsigned int height)
{
    if (m_initialized && m_server_name == server_name &&
        m_width == width && m_height == height)
        return true;

    shutdown();
    m_server_name = server_name;
    m_width = width;
    m_height = height;

    CGLContextObj context = CGLGetCurrentContext();
    if (!context)
    {
        Log::error("SyphonOutputAdapterMac",
                   "Cannot initialize Syphon: no current OpenGL context.");
        return false;
    }

    NSString* name = [NSString stringWithUTF8String:server_name.c_str()];
    SyphonOpenGLServer* server =
        [[SyphonOpenGLServer alloc] initWithName:name
                                         context:context
                                         options:nil];
    if (!server)
    {
        Log::error("SyphonOutputAdapterMac",
                   "SyphonOpenGLServer init failed for '%s'.",
                   server_name.c_str());
        return false;
    }

    m_server = server;
    m_initialized = true;
    Log::info("SyphonOutputAdapterMac",
              "Publishing immersive projection via Syphon server '%s' at %ux%u.",
              server_name.c_str(), width, height);
    return true;
}

bool SyphonOutputAdapterMac::publishFrame(const ImmersiveOutputFrame& frame,
                                          float* publish_time_ms)
{
    return publishWithTarget(frame.m_texture,
                             frame.m_texture_target == 0
                                ? GL_TEXTURE_2D
                                : frame.m_texture_target,
                             frame.m_width,
                             frame.m_height,
                             publish_time_ms);
}

bool SyphonOutputAdapterMac::publish(unsigned int texture,
                                     unsigned int width,
                                     unsigned int height,
                                     float* publish_time_ms)
{
    return publishWithTarget(texture, GL_TEXTURE_2D, width, height,
                             publish_time_ms);
}

bool SyphonOutputAdapterMac::publishWithTarget(unsigned int texture,
                                               unsigned int texture_target,
                                               unsigned int width,
                                               unsigned int height,
                                               float* publish_time_ms)
{
    const uint64_t start = StkTime::getMonoTimeMs();
    if (!m_initialized || !m_server || texture == 0)
    {
        if (publish_time_ms)
            *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
        return false;
    }

    SyphonOpenGLServer* server = (SyphonOpenGLServer*)m_server;
    [server publishFrameTexture:(GLuint)texture
                  textureTarget:(GLenum)texture_target
                    imageRegion:NSMakeRect(0, 0, width, height)
              textureDimensions:NSMakeSize(width, height)
                        flipped:NO];

    if (publish_time_ms)
        *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
    return true;
}

void SyphonOutputAdapterMac::shutdown()
{
    if (m_server)
    {
        SyphonOpenGLServer* server = (SyphonOpenGLServer*)m_server;
        [server stop];
        [server release];
        m_server = NULL;
    }
    m_initialized = false;
}

} // namespace ImmersiveRelay

#endif
