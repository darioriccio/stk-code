//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#if defined(ENABLE_IMMERSIVE_RELAY) && !defined(ENABLE_SYPHON_OUTPUT)

#include "modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.hpp"

#include "utils/log.hpp"
#include "utils/time.hpp"

namespace ImmersiveRelay
{

SyphonOutputAdapterMac::SyphonOutputAdapterMac()
{
    m_server = NULL;
    m_initialized = false;
    m_unavailable_logged = false;
    m_width = 0;
    m_height = 0;
    m_lease_enabled = false;
    m_lease_directory = m_lease_gate = m_lease_data = -1;
    m_writer_gate = m_writer_data = false;
    m_publish_fence = NULL;
}

SyphonOutputAdapterMac::~SyphonOutputAdapterMac()
{
    shutdown();
}

bool SyphonOutputAdapterMac::initialize(const std::string& server_name,
                                       unsigned int width,
                                       unsigned int height)
{
    (void)width;
    (void)height;
    m_server_name = server_name;
    if (!m_unavailable_logged)
    {
        Log::warn("SyphonOutputAdapterMac",
            "Syphon output requested, but this build was compiled without ENABLE_SYPHON_OUTPUT.");
        m_unavailable_logged = true;
    }
    return false;
}

bool SyphonOutputAdapterMac::publish(unsigned int texture,
                                     unsigned int width,
                                     unsigned int height,
                                     float* publish_time_ms)
{
    return publishWithTarget(texture, 0, width, height, publish_time_ms);
}

bool SyphonOutputAdapterMac::publishFrame(const ImmersiveOutputFrame& frame,
                                          float* publish_time_ms)
{
    return publishWithTarget(frame.m_texture, frame.m_texture_target,
                             frame.m_width, frame.m_height, publish_time_ms);
}

bool SyphonOutputAdapterMac::publishWithTarget(unsigned int texture,
                                               unsigned int texture_target,
                                               unsigned int width,
                                               unsigned int height,
                                               float* publish_time_ms)
{
    (void)texture;
    (void)texture_target;
    (void)width;
    (void)height;
    const uint64_t start = StkTime::getMonoTimeMs();
    if (publish_time_ms)
        *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
    if (!m_unavailable_logged)
    {
        Log::warn("SyphonOutputAdapterMac",
            "Cannot publish: ENABLE_SYPHON_OUTPUT is OFF.");
        m_unavailable_logged = true;
    }
    return false;
}

bool SyphonOutputAdapterMac::prepareLease() { return false; }
bool SyphonOutputAdapterMac::acquireWriter() { return false; }
void SyphonOutputAdapterMac::closeLease() {}
void SyphonOutputAdapterMac::pollPublishedLease() {}

void SyphonOutputAdapterMac::shutdown()
{
    m_server = NULL;
    m_initialized = false;
}

} // namespace ImmersiveRelay

#endif
