//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/immersive_render_config.hpp"

#include <cstdlib>

namespace ImmersiveRelay
{

namespace
{
bool equals(const std::string& a, const char* b)
{
    return a == b;
}
}

ImmersiveRenderConfig::ImmersiveRenderConfig()
{
    m_output_mode = IMMERSIVE_OUTPUT_NONE;
    m_projection_mode = IMMERSIVE_PROJECTION_NONE;
    m_camera_mode = IMMERSIVE_CAMERA_STABILIZED_CHASE;
    // Venue default stays on the safe preset until Mac mini M1 base acceptance.
    m_domemaster_size = 1536;
    m_projection_width = 1536;
    m_projection_height = 1536;
    m_cubemap_size = 768;
    m_yaw_deg = 0.0f;
    m_pitch_deg = 35.0f;
    m_roll_deg = 0.0f;
    m_dome_tilt_deg = 0.0f;
    m_horizon_stabilization = true;
    m_roll_limit_deg = 12.0f;
    m_debug_grid = false;
    m_stats_enabled = false;
    m_syphon_server_name = "SuperTuxKart Immersive Relay Domemaster";
    m_forward_offset = 0.25f;
    m_vertical_offset = 0.40f;
    m_lookahead_offset = 8.0f;
    m_smoothing_factor = 0.16f;
    m_face_fov_deg = 120.0f;
}

bool ImmersiveRenderConfig::isEnabled() const
{
    return m_output_mode != IMMERSIVE_OUTPUT_NONE &&
           projectionIsAvailable(m_projection_mode);
}

bool ImmersiveRenderConfig::wantsSyphon() const
{
    return m_output_mode == IMMERSIVE_OUTPUT_SYPHON;
}

const char* outputModeToString(ImmersiveOutputMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_OUTPUT_WINDOW: return "window";
    case IMMERSIVE_OUTPUT_SYPHON: return "syphon";
    case IMMERSIVE_OUTPUT_NONE:
    default: return "none";
    }
}

const char* projectionModeToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "fisheye";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "equirectangular";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
        return "cylindrical";
    case IMMERSIVE_PROJECTION_CUBE_FACES:
        return "cube_faces";
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "wall_atlas";
    case IMMERSIVE_PROJECTION_NONE:
    default: return "none";
    }
}

const char* projectionKindToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "fisheye_domemaster";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "equirectangular";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
        return "cylindrical";
    case IMMERSIVE_PROJECTION_CUBE_FACES:
        return "cube_faces";
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "wall_atlas";
    case IMMERSIVE_PROJECTION_NONE:
    default:
        return "none";
    }
}

const char* projectionStatusToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "production";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "stub";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
    case IMMERSIVE_PROJECTION_CUBE_FACES:
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "stub";
    case IMMERSIVE_PROJECTION_NONE:
    default:
        return "disabled";
    }
}

const char* projectionAspectToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "1:1";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "2:1";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
        return "variable";
    case IMMERSIVE_PROJECTION_CUBE_FACES:
        return "6x1_or_atlas";
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "layout_defined";
    case IMMERSIVE_PROJECTION_NONE:
    default:
        return "none";
    }
}

const char* projectionCoverageToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "front-facing 180";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "360x180 full sphere";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
        return "horizontal panorama";
    case IMMERSIVE_PROJECTION_CUBE_FACES:
        return "six cube faces full sphere";
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "venue wall layout";
    case IMMERSIVE_PROJECTION_NONE:
    default:
        return "none";
    }
}

const char* projectionOrientationToString(ImmersiveProjectionMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER:
        return "disc center is active-driver forward; top is camera up; bottom is track/kart down";
    case IMMERSIVE_PROJECTION_EQUIRECTANGULAR:
        return "future: longitude 0 is active-driver forward; horizon centered";
    case IMMERSIVE_PROJECTION_CYLINDRICAL:
        return "future: center is active-driver forward";
    case IMMERSIVE_PROJECTION_CUBE_FACES:
        return "future: +X,-X,+Y,-Y,+Z,-Z face order";
    case IMMERSIVE_PROJECTION_WALL_ATLAS:
        return "future: layout-specific wall coordinates";
    case IMMERSIVE_PROJECTION_NONE:
    default:
        return "none";
    }
}

const char* cameraModeToString(ImmersiveCameraMode mode)
{
    switch (mode)
    {
    case IMMERSIVE_CAMERA_POV: return "pov";
    case IMMERSIVE_CAMERA_CHASE: return "chase";
    case IMMERSIVE_CAMERA_COCKPIT: return "cockpit";
    case IMMERSIVE_CAMERA_STABILIZED_CHASE:
    default: return "stabilized_chase";
    }
}

bool parseOutputMode(const std::string& value, ImmersiveOutputMode* mode)
{
    if (equals(value, "none"))
        *mode = IMMERSIVE_OUTPUT_NONE;
    else if (equals(value, "window"))
        *mode = IMMERSIVE_OUTPUT_WINDOW;
    else if (equals(value, "syphon"))
        *mode = IMMERSIVE_OUTPUT_SYPHON;
    else
        return false;
    return true;
}

bool parseProjectionMode(const std::string& value,
                         ImmersiveProjectionMode* mode)
{
    if (equals(value, "none"))
        *mode = IMMERSIVE_PROJECTION_NONE;
    else if (equals(value, "fisheye") ||
             equals(value, "fisheye_domemaster") ||
             equals(value, "domemaster"))
        *mode = IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
    else if (equals(value, "equirect") ||
             equals(value, "equirectangular"))
        *mode = IMMERSIVE_PROJECTION_EQUIRECTANGULAR;
    else if (equals(value, "cylindrical") ||
             equals(value, "panoramic"))
        *mode = IMMERSIVE_PROJECTION_CYLINDRICAL;
    else if (equals(value, "cube_faces") || equals(value, "cube"))
        *mode = IMMERSIVE_PROJECTION_CUBE_FACES;
    else if (equals(value, "wall_atlas") || equals(value, "led_wall"))
        *mode = IMMERSIVE_PROJECTION_WALL_ATLAS;
    else
        return false;
    return true;
}

bool parseCameraMode(const std::string& value, ImmersiveCameraMode* mode)
{
    if (equals(value, "pov"))
        *mode = IMMERSIVE_CAMERA_POV;
    else if (equals(value, "chase"))
        *mode = IMMERSIVE_CAMERA_CHASE;
    else if (equals(value, "stabilized_chase"))
        *mode = IMMERSIVE_CAMERA_STABILIZED_CHASE;
    else if (equals(value, "cockpit"))
        *mode = IMMERSIVE_CAMERA_COCKPIT;
    else
        return false;
    return true;
}

bool parseOnOff(const std::string& value, bool* enabled)
{
    if (equals(value, "on") || equals(value, "true") || equals(value, "1"))
        *enabled = true;
    else if (equals(value, "off") || equals(value, "false") ||
             equals(value, "0"))
        *enabled = false;
    else
        return false;
    return true;
}

bool projectionIsAvailable(ImmersiveProjectionMode mode)
{
    return mode == IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
}

bool projectionIsProduction(ImmersiveProjectionMode mode)
{
    return mode == IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
}

bool projectionIsSquare(ImmersiveProjectionMode mode)
{
    return mode == IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
}

bool projectionIsTwoToOne(ImmersiveProjectionMode mode)
{
    return mode == IMMERSIVE_PROJECTION_EQUIRECTANGULAR;
}

unsigned int sanitizeDomemasterSize(int requested)
{
    if (requested == 1024 || requested == 1536 || requested == 2048)
        return (unsigned int)requested;
    return 2048;
}

unsigned int sanitizeCubemapSize(int requested)
{
    if (requested == 512 || requested == 768 || requested == 1024 ||
        requested == 1536)
        return (unsigned int)requested;
    return 1024;
}

unsigned int sanitizeEquirectWidth(int requested)
{
    if (requested == 2048 || requested == 4096 || requested == 8192)
        return (unsigned int)requested;
    return 2048;
}

void syncLegacyDomemasterSize(ImmersiveRenderConfig* config)
{
    if (!config)
        return;
    if (config->m_projection_mode == IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER)
    {
        config->m_domemaster_size =
            sanitizeDomemasterSize((int)config->m_domemaster_size);
        config->m_projection_width = config->m_domemaster_size;
        config->m_projection_height = config->m_domemaster_size;
    }
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
