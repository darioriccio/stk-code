//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_IMMERSIVE_RENDER_CONFIG_HPP
#define HEADER_IMMERSIVE_RENDER_CONFIG_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include <string>

namespace ImmersiveRelay
{

enum ImmersiveOutputMode
{
    IMMERSIVE_OUTPUT_NONE,
    IMMERSIVE_OUTPUT_WINDOW,
    IMMERSIVE_OUTPUT_SYPHON
};

enum ImmersiveProjectionMode
{
    IMMERSIVE_PROJECTION_NONE,
    IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER,
    IMMERSIVE_PROJECTION_FISHEYE = IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER,
    IMMERSIVE_PROJECTION_EQUIRECTANGULAR,
    IMMERSIVE_PROJECTION_CYLINDRICAL,
    IMMERSIVE_PROJECTION_CUBE_FACES,
    IMMERSIVE_PROJECTION_WALL_ATLAS
};

enum ImmersiveCameraMode
{
    IMMERSIVE_CAMERA_POV,
    IMMERSIVE_CAMERA_CHASE,
    IMMERSIVE_CAMERA_STABILIZED_CHASE,
    IMMERSIVE_CAMERA_COCKPIT
};

struct ImmersiveRenderConfig
{
    ImmersiveOutputMode m_output_mode;
    ImmersiveProjectionMode m_projection_mode;
    ImmersiveCameraMode m_camera_mode;
    unsigned int m_domemaster_size;
    unsigned int m_projection_width;
    unsigned int m_projection_height;
    unsigned int m_cubemap_size;
    float m_yaw_deg;
    float m_pitch_deg;
    float m_roll_deg;
    float m_dome_tilt_deg;
    bool m_horizon_stabilization;
    float m_roll_limit_deg;
    bool m_debug_grid;
    bool m_stats_enabled;
    std::string m_syphon_server_name;
    float m_forward_offset;
    float m_vertical_offset;
    float m_lookahead_offset;
    float m_smoothing_factor;
    float m_face_fov_deg;

    ImmersiveRenderConfig();
    bool isEnabled() const;
    bool wantsSyphon() const;
};

const char* outputModeToString(ImmersiveOutputMode mode);
const char* projectionModeToString(ImmersiveProjectionMode mode);
const char* projectionKindToString(ImmersiveProjectionMode mode);
const char* projectionStatusToString(ImmersiveProjectionMode mode);
const char* projectionAspectToString(ImmersiveProjectionMode mode);
const char* projectionCoverageToString(ImmersiveProjectionMode mode);
const char* projectionOrientationToString(ImmersiveProjectionMode mode);
const char* cameraModeToString(ImmersiveCameraMode mode);

bool parseOutputMode(const std::string& value, ImmersiveOutputMode* mode);
bool parseProjectionMode(const std::string& value,
                         ImmersiveProjectionMode* mode);
bool parseCameraMode(const std::string& value, ImmersiveCameraMode* mode);
bool parseOnOff(const std::string& value, bool* enabled);
bool projectionIsAvailable(ImmersiveProjectionMode mode);
bool projectionIsProduction(ImmersiveProjectionMode mode);
bool projectionIsSquare(ImmersiveProjectionMode mode);
bool projectionIsTwoToOne(ImmersiveProjectionMode mode);
unsigned int sanitizeDomemasterSize(int requested);
unsigned int sanitizeCubemapSize(int requested);
unsigned int sanitizeEquirectWidth(int requested);
bool validEquirectDimensions(unsigned int width, unsigned int height);
void syncLegacyDomemasterSize(ImmersiveRenderConfig* config);

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_RENDER_CONFIG_HPP
