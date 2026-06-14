//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_IMMERSIVE_OUTPUT_ADAPTER_HPP
#define HEADER_IMMERSIVE_OUTPUT_ADAPTER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include <string>

#include "modes/immersive_relay/render/immersive_render_config.hpp"

namespace ImmersiveRelay
{

struct ImmersiveOutputFrame
{
    ImmersiveOutputFrame()
    {
        m_texture = 0;
        m_texture_target = 0;
        m_width = 0;
        m_height = 0;
        m_projection_mode = IMMERSIVE_PROJECTION_NONE;
    }

    unsigned int m_texture;
    unsigned int m_texture_target;
    unsigned int m_width;
    unsigned int m_height;
    ImmersiveProjectionMode m_projection_mode;
    std::string m_projection_id;
    std::string m_server_name;
};

class ImmersiveOutputAdapter
{
public:
    virtual ~ImmersiveOutputAdapter() {}
    virtual bool publishFrame(const ImmersiveOutputFrame& frame,
                              float* publish_time_ms) = 0;
    virtual bool publish(unsigned int texture,
                         unsigned int width,
                         unsigned int height,
                         float* publish_time_ms)
    {
        ImmersiveOutputFrame frame;
        frame.m_texture = texture;
        frame.m_width = width;
        frame.m_height = height;
        return publishFrame(frame, publish_time_ms);
    }
    virtual void shutdown() {}
    virtual bool isActive() const = 0;
    virtual const char* name() const = 0;
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_OUTPUT_ADAPTER_HPP
