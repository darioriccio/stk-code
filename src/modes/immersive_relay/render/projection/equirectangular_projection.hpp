// Copyright (C) 2026 SuperTuxKart Immersive Relay contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_EQUIRECTANGULAR_PROJECTION_HPP
#define HEADER_EQUIRECTANGULAR_PROJECTION_HPP
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/render/projection/fisheye_domemaster_projection.hpp"
namespace ImmersiveRelay
{
// Shares allocation, face binding and publication with the existing adapter;
// its shader samples the six scene faces directly, never a fisheye texture.
class EquirectangularProjection : public FisheyeDomemasterProjection
{
public:
    std::string id() const { return "equirectangular"; }
    ImmersiveProjectionMode kind() const { return IMMERSIVE_PROJECTION_EQUIRECTANGULAR; }
    bool configure(const ImmersiveRenderConfig& config)
    {
        return validEquirectDimensions(config.m_projection_width, config.m_projection_height) &&
               ensureTarget(config.m_projection_width, config.m_projection_height);
    }
    ProjectionOutputDescription outputDescription() const;
};
}
#endif
#endif
