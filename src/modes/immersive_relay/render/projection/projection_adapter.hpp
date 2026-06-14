//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_PROJECTION_ADAPTER_HPP
#define HEADER_PROJECTION_ADAPTER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/full_sphere_capture.hpp"
#include "modes/immersive_relay/render/immersive_render_config.hpp"

#include <string>

namespace ImmersiveRelay
{

class ImmersiveRenderStats;

struct ProjectionOutputDescription
{
    ProjectionOutputDescription()
    {
        m_kind = IMMERSIVE_PROJECTION_NONE;
        m_width = 0;
        m_height = 0;
        m_square = false;
        m_two_to_one = false;
        m_debug_grid_supported = false;
        m_production = false;
    }

    ImmersiveProjectionMode m_kind;
    std::string m_id;
    std::string m_status;
    unsigned int m_width;
    unsigned int m_height;
    std::string m_aspect;
    std::string m_coverage;
    std::string m_orientation;
    bool m_square;
    bool m_two_to_one;
    bool m_debug_grid_supported;
    bool m_production;
};

class ProjectionAdapter
{
public:
    virtual ~ProjectionAdapter() {}
    virtual std::string id() const = 0;
    virtual ImmersiveProjectionMode kind() const = 0;
    virtual bool configure(const ImmersiveRenderConfig& config) = 0;
    virtual bool renderProjection(const FullSphereCapture& capture,
                                  const ImmersiveRenderConfig& config,
                                  ImmersiveRenderStats& stats) = 0;
    virtual ProjectionOutputDescription outputDescription() const = 0;
    virtual unsigned int getTexture() const = 0;
    virtual unsigned int getTextureTarget() const = 0;
    virtual unsigned int getWidth() const = 0;
    virtual unsigned int getHeight() const = 0;
    virtual void destroy() = 0;
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_PROJECTION_ADAPTER_HPP
