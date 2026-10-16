//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_IMMERSIVE_RENDER_PIPELINE_HPP
#define HEADER_IMMERSIVE_RENDER_PIPELINE_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/immersive_render_config.hpp"
#include "modes/immersive_relay/render/immersive_render_stats.hpp"

#include <mutex>

namespace ImmersiveRelay
{

class ImmersiveRenderPipeline
{
private:
    static ImmersiveRenderPipeline* m_instance;

    ImmersiveRenderConfig m_config;
    ImmersiveRenderConfig m_last_applied_config;
    bool m_has_applied_config;
    ImmersiveRenderStats m_stats;
    ImmersiveRenderStats m_stats_snapshot;
    mutable std::mutex m_snapshot_mutex;
    bool m_configured;

    ImmersiveRenderPipeline();

public:
    static ImmersiveRenderPipeline* get();
    static void destroy();

    void setConfig(const ImmersiveRenderConfig& config);
    ImmersiveRenderConfig getConfig() const;
    ImmersiveRenderStats getStats() const;
    void publishStatsSnapshot();
    void rejectRenderConfiguration(const std::string& reason);
    void renderFrame(float dt);
    void shutdown();
    std::string prepareScintillaOutput();
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_RENDER_PIPELINE_HPP
