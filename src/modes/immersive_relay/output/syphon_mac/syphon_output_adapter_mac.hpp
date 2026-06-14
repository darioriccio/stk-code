//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_SYPHON_OUTPUT_ADAPTER_MAC_HPP
#define HEADER_SYPHON_OUTPUT_ADAPTER_MAC_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/output/immersive_output_adapter.hpp"

#include <string>

namespace ImmersiveRelay
{

class SyphonOutputAdapterMac : public ImmersiveOutputAdapter
{
private:
    void* m_server;
    bool m_initialized;
    bool m_unavailable_logged;
    std::string m_server_name;
    unsigned int m_width;
    unsigned int m_height;

public:
    SyphonOutputAdapterMac();
    ~SyphonOutputAdapterMac();

    bool initialize(const std::string& server_name,
                    unsigned int width,
                    unsigned int height);
    bool publishFrame(const ImmersiveOutputFrame& frame,
                      float* publish_time_ms);
    bool publish(unsigned int texture, unsigned int width,
                 unsigned int height, float* publish_time_ms);
    bool publishWithTarget(unsigned int texture, unsigned int texture_target,
                           unsigned int width, unsigned int height,
                           float* publish_time_ms);
    void shutdown();
    bool isActive() const { return m_initialized; }
    const char* name() const { return "syphon"; }
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_SYPHON_OUTPUT_ADAPTER_MAC_HPP
