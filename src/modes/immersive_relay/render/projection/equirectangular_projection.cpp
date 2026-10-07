// Copyright (C) 2026 SuperTuxKart Immersive Relay contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/render/projection/equirectangular_projection.hpp"
namespace ImmersiveRelay
{
ProjectionOutputDescription EquirectangularProjection::outputDescription() const
{
    ProjectionOutputDescription result;
    result.m_kind=kind(); result.m_id=id();
    result.m_status=projectionStatusToString(kind());
    result.m_width=getWidth(); result.m_height=getHeight();
    result.m_aspect=projectionAspectToString(kind());
    result.m_coverage=projectionCoverageToString(kind());
    result.m_orientation=projectionOrientationToString(kind());
    result.m_square=false; result.m_two_to_one=true;
    result.m_debug_grid_supported=true; result.m_production=true;
    return result;
}
}
#endif
