//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/projection/projection_debug_grid.hpp"

namespace ImmersiveRelay
{

const char* ProjectionDebugGrid::markerSummary()
{
    return "radial lines, concentric rings, horizon, forward, side/up/down and dome tilt markers";
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
