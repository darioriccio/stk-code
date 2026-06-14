//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/output/debug_window/debug_window_output_adapter.hpp"

#include "graphics/gl_headers.hpp"
#include "graphics/irr_driver.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"

#include <algorithm>

namespace ImmersiveRelay
{

DebugWindowOutputAdapter::DebugWindowOutputAdapter()
{
    m_read_frame_buffer = 0;
    m_active = false;
}

DebugWindowOutputAdapter::~DebugWindowOutputAdapter()
{
    shutdown();
}

bool DebugWindowOutputAdapter::publishFrame(const ImmersiveOutputFrame& frame,
                                            float* publish_time_ms)
{
    const uint64_t start = StkTime::getMonoTimeMs();
#ifdef SERVER_ONLY
    const bool ok = false;
#else
    bool ok = false;
    if (frame.m_texture != 0 && frame.m_width > 0 && frame.m_height > 0)
    {
        if (m_read_frame_buffer == 0)
            glGenFramebuffers(1, (GLuint*)&m_read_frame_buffer);

        GLint previous_read_fbo = 0;
        GLint previous_draw_fbo = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);

        GLenum texture_target = frame.m_texture_target == 0
            ? GL_TEXTURE_2D : (GLenum)frame.m_texture_target;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_read_frame_buffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               texture_target, (GLuint)frame.m_texture, 0);
        if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE)
        {
            const irr::core::dimension2du screen =
                irr_driver->getActualScreenSize();
            const float frame_aspect =
                (float)frame.m_width / std::max(1.0f, (float)frame.m_height);
            const float screen_aspect =
                (float)screen.Width / std::max(1.0f, (float)screen.Height);
            unsigned int out_w = screen.Width;
            unsigned int out_h = screen.Height;
            if (screen_aspect > frame_aspect)
                out_w = (unsigned int)((float)out_h * frame_aspect);
            else
                out_h = (unsigned int)((float)out_w / frame_aspect);
            const unsigned int x0 = (screen.Width - out_w) / 2;
            const unsigned int y0 = (screen.Height - out_h) / 2;

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                              irr_driver->getDefaultFramebuffer());
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0,
                              (GLint)frame.m_width,
                              (GLint)frame.m_height,
                              (GLint)x0,
                              (GLint)y0,
                              (GLint)(x0 + out_w),
                              (GLint)(y0 + out_h),
                              GL_COLOR_BUFFER_BIT,
                              GL_LINEAR);
            ok = true;
        }
        else
        {
            Log::warn("DebugWindowOutputAdapter",
                      "Cannot preview immersive projection: read FBO incomplete.");
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
    }
    m_active = ok;
#endif
    if (publish_time_ms)
        *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
    return ok;
}

bool DebugWindowOutputAdapter::publish(unsigned int texture,
                                       unsigned int width,
                                       unsigned int height,
                                       float* publish_time_ms)
{
    ImmersiveOutputFrame frame;
    frame.m_texture = texture;
    frame.m_texture_target = GL_TEXTURE_2D;
    frame.m_width = width;
    frame.m_height = height;
    return publishFrame(frame, publish_time_ms);
}

void DebugWindowOutputAdapter::shutdown()
{
#ifndef SERVER_ONLY
    if (m_read_frame_buffer != 0)
    {
        GLuint fbo = (GLuint)m_read_frame_buffer;
        glDeleteFramebuffers(1, &fbo);
    }
#endif
    m_read_frame_buffer = 0;
    m_active = false;
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
