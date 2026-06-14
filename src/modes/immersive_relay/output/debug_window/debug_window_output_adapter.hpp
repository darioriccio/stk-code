//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_DEBUG_WINDOW_OUTPUT_ADAPTER_HPP
#define HEADER_DEBUG_WINDOW_OUTPUT_ADAPTER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/output/immersive_output_adapter.hpp"

namespace ImmersiveRelay
{

class DebugWindowOutputAdapter : public ImmersiveOutputAdapter
{
private:
    unsigned int m_read_frame_buffer;
    bool m_active;

public:
    DebugWindowOutputAdapter();
    ~DebugWindowOutputAdapter();
    bool publishFrame(const ImmersiveOutputFrame& frame,
                      float* publish_time_ms);
    bool publish(unsigned int texture, unsigned int width,
                 unsigned int height, float* publish_time_ms);
    void shutdown();
    bool isActive() const { return m_active; }
    const char* name() const { return "window"; }
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_DEBUG_WINDOW_OUTPUT_ADAPTER_HPP
