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

namespace ImmersiveRelay
{

class ImmersiveRenderPipeline
{
private:
    static ImmersiveRenderPipeline* m_instance;

    ImmersiveRenderConfig m_config;
    ImmersiveRenderStats m_stats;
    bool m_configured;

    ImmersiveRenderPipeline();

public:
    static ImmersiveRenderPipeline* get();
    static void destroy();

    void setConfig(const ImmersiveRenderConfig& config);
    const ImmersiveRenderConfig& getConfig() const { return m_config; }
    const ImmersiveRenderStats& getStats() const { return m_stats; }
    void renderFrame(float dt);
    void shutdown();
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_RENDER_PIPELINE_HPP
