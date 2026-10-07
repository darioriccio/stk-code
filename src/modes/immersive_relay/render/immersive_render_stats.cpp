//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/immersive_render_stats.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace ImmersiveRelay
{

ImmersiveRenderStats::ImmersiveRenderStats()
{
    m_output_mode = IMMERSIVE_OUTPUT_NONE;
    m_projection_mode = IMMERSIVE_PROJECTION_NONE;
    m_camera_mode = IMMERSIVE_CAMERA_STABILIZED_CHASE;
    m_domemaster_size = 0;
    m_projection_width = 0;
    m_projection_height = 0;
    m_cubemap_size = 0;
    m_avg_fps = 0.0f;
    m_avg_frame_time_ms = 0.0f;
    m_p95_frame_time_ms = 0.0f;
    m_p99_frame_time_ms = 0.0f;
    m_last_frame_time_ms = 0.0f;
    m_capture_time_ms = 0.0f;
    m_cubemap_render_time_ms = 0.0f;
    m_projection_time_ms = 0.0f;
    m_output_publish_time_ms = 0.0f;
    m_syphon_publish_time_ms = 0.0f;
    m_dropped_frames = 0;
    m_publish_skipped = 0;
    m_active_driver_index = -1;
    m_lap = 0;
    m_total_laps = 0;
    m_yaw_deg = 0.0f;
    m_pitch_deg = 0.0f;
    m_roll_deg = 0.0f;
    m_dome_tilt_deg = 0.0f;
    m_roll_limit_deg = 0.0f;
    m_forward_offset = 0.0f;
    m_vertical_offset = 0.0f;
    m_smoothing_factor = 0.0f;
    m_horizon_stabilization = true;
    m_configuration_applied = false;
    m_true_fulldome = false;
    m_output_active = false;
    m_output_available = false;
    m_syphon_active = false;
}

void ImmersiveRenderStats::resetRolling()
{
    m_frame_times_ms.clear();
    m_avg_fps = 0.0f;
    m_avg_frame_time_ms = 0.0f;
    m_p95_frame_time_ms = 0.0f;
    m_p99_frame_time_ms = 0.0f;
}

void ImmersiveRenderStats::beginFrame(const ImmersiveRenderConfig& config)
{
    m_output_mode = config.m_output_mode;
    m_projection_mode = config.m_projection_mode;
    m_camera_mode = config.m_camera_mode;
    m_domemaster_size = config.m_domemaster_size;
    m_projection_width = config.m_projection_width;
    m_projection_height = config.m_projection_height;
    m_cubemap_size = config.m_cubemap_size;
    m_yaw_deg = config.m_yaw_deg;
    m_pitch_deg = config.m_pitch_deg;
    m_roll_deg = config.m_roll_deg;
    m_dome_tilt_deg = config.m_dome_tilt_deg;
    m_roll_limit_deg = config.m_roll_limit_deg;
    m_forward_offset = config.m_forward_offset;
    m_vertical_offset = config.m_vertical_offset;
    m_smoothing_factor = config.m_smoothing_factor;
    m_horizon_stabilization = config.m_horizon_stabilization;
    m_syphon_server_name = config.m_syphon_server_name;
    m_capture_time_ms = 0.0f;
    m_cubemap_render_time_ms = 0.0f;
    m_projection_time_ms = 0.0f;
    m_output_publish_time_ms = 0.0f;
    m_syphon_publish_time_ms = 0.0f;
    m_true_fulldome = false;
    m_output_active = false;
#ifdef ENABLE_SYPHON_OUTPUT
    m_output_available = config.m_output_mode != IMMERSIVE_OUTPUT_NONE;
#else
    m_output_available =
        config.m_output_mode != IMMERSIVE_OUTPUT_SYPHON &&
        config.m_output_mode != IMMERSIVE_OUTPUT_NONE;
#endif
    m_syphon_active = false;
    m_fallback_reason.clear();
}

void ImmersiveRenderStats::recordFrame(float frame_time_ms)
{
    m_last_frame_time_ms = frame_time_ms;
    m_frame_times_ms.push_back(frame_time_ms);
    while (m_frame_times_ms.size() > 240)
        m_frame_times_ms.pop_front();

    float sum = 0.0f;
    for (unsigned int i = 0; i < m_frame_times_ms.size(); i++)
        sum += m_frame_times_ms[i];
    m_avg_frame_time_ms = m_frame_times_ms.empty()
        ? 0.0f : sum / (float)m_frame_times_ms.size();
    m_avg_fps = m_avg_frame_time_ms > 0.0f
        ? 1000.0f / m_avg_frame_time_ms : 0.0f;
    m_p95_frame_time_ms = percentile(0.95f);
    m_p99_frame_time_ms = percentile(0.99f);
}

float ImmersiveRenderStats::percentile(float pct) const
{
    if (m_frame_times_ms.empty())
        return 0.0f;

    std::vector<float> sorted(m_frame_times_ms.begin(), m_frame_times_ms.end());
    std::sort(sorted.begin(), sorted.end());
    const float f_index = pct * (float)(sorted.size() - 1);
    const size_t index = (size_t)(f_index + 0.5f);
    return sorted[std::min(index, sorted.size() - 1)];
}

std::string ImmersiveRenderStats::jsonEscape(const std::string& input)
{
    std::ostringstream out;
    for (std::string::const_iterator i = input.begin(); i != input.end(); i++)
    {
        switch (*i)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if ((unsigned char)*i < 0x20)
                out << "?";
            else
                out << *i;
            break;
        }
    }
    return out.str();
}

std::string ImmersiveRenderStats::toJson() const
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"immersive_output_mode\":\"" << outputModeToString(m_output_mode) << "\"";
    out << ",\"immersive_projection\":\"" << projectionModeToString(m_projection_mode) << "\"";
    out << ",\"domemaster_size\":" << m_domemaster_size;
    out << ",\"projection_width\":" << m_projection_width;
    out << ",\"projection_height\":" << m_projection_height;
    out << ",\"cubemap_size\":" << m_cubemap_size;
    out << ",\"fps\":" << m_avg_fps;
    out << ",\"frame_time_ms\":" << m_avg_frame_time_ms;
    out << ",\"last_frame_time_ms\":" << m_last_frame_time_ms;
    out << ",\"capture_time_ms\":" << m_capture_time_ms;
    out << ",\"p95_frame_time_ms\":" << m_p95_frame_time_ms;
    out << ",\"p99_frame_time_ms\":" << m_p99_frame_time_ms;
    out << ",\"cubemap_render_time_ms\":" << m_cubemap_render_time_ms;
    out << ",\"projection_time_ms\":" << m_projection_time_ms;
    out << ",\"output_publish_time_ms\":" << m_output_publish_time_ms;
    out << ",\"syphon_publish_time_ms\":" << m_syphon_publish_time_ms;
    out << ",\"dropped_frames\":" << m_dropped_frames;
    out << ",\"publish_skipped\":" << m_publish_skipped;
    out << ",\"active_driver_index\":" << m_active_driver_index;
    out << ",\"active_driver_id\":\"" << jsonEscape(m_active_driver_id) << "\"";
    out << ",\"lap\":" << m_lap;
    out << ",\"total_laps\":" << m_total_laps;
    out << ",\"current_encounter\":\"" << jsonEscape(m_current_encounter) << "\"";
    out << ",\"camera_mode\":\"" << cameraModeToString(m_camera_mode) << "\"";
    out << ",\"yaw_deg\":" << m_yaw_deg;
    out << ",\"pitch_deg\":" << m_pitch_deg;
    out << ",\"roll_deg\":" << m_roll_deg;
    out << ",\"dome_tilt_deg\":" << m_dome_tilt_deg;
    out << ",\"roll_limit_deg\":" << m_roll_limit_deg;
    out << ",\"forward_offset\":" << m_forward_offset;
    out << ",\"vertical_offset\":" << m_vertical_offset;
    out << ",\"smoothing_factor\":" << m_smoothing_factor;
    out << ",\"horizon_stabilization\":"
        << (m_horizon_stabilization ? "true" : "false");
    out << ",\"camera_settings\":{";
    out << "\"camera_mode\":\"" << cameraModeToString(m_camera_mode) << "\"";
    out << ",\"yaw_deg\":" << m_yaw_deg;
    out << ",\"pitch_deg\":" << m_pitch_deg;
    out << ",\"roll_deg\":" << m_roll_deg;
    out << ",\"dome_tilt_deg\":" << m_dome_tilt_deg;
    out << ",\"roll_limit_deg\":" << m_roll_limit_deg;
    out << ",\"forward_offset\":" << m_forward_offset;
    out << ",\"vertical_offset\":" << m_vertical_offset;
    out << ",\"smoothing_factor\":" << m_smoothing_factor;
    out << ",\"horizon_stabilization\":"
        << (m_horizon_stabilization ? "true" : "false");
    out << "}";
    out << ",\"configuration_applied\":" << (m_configuration_applied ? "true" : "false");
    out << ",\"enabled\":"
        << (m_output_mode != IMMERSIVE_OUTPUT_NONE &&
            projectionIsAvailable(m_projection_mode) ? "true" : "false");
    out << ",\"capture\":{";
    out << "\"type\":\"full_sphere_cubemap\"";
    out << ",\"cubemap_size\":" << m_cubemap_size;
    out << ",\"faces\":6";
    out << "}";
    out << ",\"projection\":{";
    out << "\"type\":\"" << projectionKindToString(m_projection_mode) << "\"";
    out << ",\"status\":\"" << projectionStatusToString(m_projection_mode)
        << "\"";
    out << ",\"width\":" << m_projection_width;
    out << ",\"height\":" << m_projection_height;
    out << ",\"aspect\":\"" << projectionAspectToString(m_projection_mode)
        << "\"";
    out << ",\"coverage\":\"" << projectionCoverageToString(m_projection_mode)
        << "\"";
    out << ",\"orientation\":\""
        << jsonEscape(projectionOrientationToString(m_projection_mode))
        << "\"";
    out << ",\"debug_grid\":"
        << (projectionIsAvailable(m_projection_mode) ? "true" : "false");
    out << "}";
    out << ",\"output\":{";
    out << "\"type\":\"" << outputModeToString(m_output_mode) << "\"";
    out << ",\"available\":" << (m_output_available ? "true" : "false");
    out << ",\"active\":" << (m_output_active ? "true" : "false");
    out << ",\"server_name\":\"" << jsonEscape(m_syphon_server_name) << "\"";
    out << "}";
    out << ",\"camera\":{";
    out << "\"mode\":\"" << cameraModeToString(m_camera_mode) << "\"";
    out << ",\"active_driver\":" << m_active_driver_index;
    out << ",\"active_driver_id\":\"" << jsonEscape(m_active_driver_id) << "\"";
    out << ",\"yaw_deg\":" << m_yaw_deg;
    out << ",\"pitch_deg\":" << m_pitch_deg;
    out << ",\"roll_deg\":" << m_roll_deg;
    out << ",\"dome_tilt_deg\":" << m_dome_tilt_deg;
    out << ",\"forward_offset\":" << m_forward_offset;
    out << ",\"vertical_offset\":" << m_vertical_offset;
    out << ",\"smoothing_factor\":" << m_smoothing_factor;
    out << ",\"horizon_stabilization\":"
        << (m_horizon_stabilization ? "true" : "false");
    out << ",\"roll_limit_deg\":" << m_roll_limit_deg;
    out << "}";
    out << ",\"stats\":{";
    out << "\"fps\":" << m_avg_fps;
    out << ",\"frame_time_ms\":" << m_avg_frame_time_ms;
    out << ",\"capture_ms\":" << m_capture_time_ms;
    out << ",\"projection_ms\":" << m_projection_time_ms;
    out << ",\"output_ms\":" << m_output_publish_time_ms;
    out << ",\"p95_ms\":" << m_p95_frame_time_ms;
    out << ",\"p99_ms\":" << m_p99_frame_time_ms;
    out << ",\"dropped_frames\":" << m_dropped_frames;
    out << ",\"publish_skipped\":" << m_publish_skipped;
    out << "}";
    out << ",\"supported_projections\":[";
    const ImmersiveProjectionMode modes[] = {
        IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER,
        IMMERSIVE_PROJECTION_EQUIRECTANGULAR,
        IMMERSIVE_PROJECTION_CYLINDRICAL,
        IMMERSIVE_PROJECTION_CUBE_FACES,
        IMMERSIVE_PROJECTION_WALL_ATLAS
    };
    for (unsigned int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"type\":\"" << projectionKindToString(modes[i]) << "\"";
        out << ",\"status\":\"" << projectionStatusToString(modes[i])
            << "\"";
        out << ",\"available\":"
            << (projectionIsAvailable(modes[i]) ? "true" : "false");
        out << ",\"aspect\":\"" << projectionAspectToString(modes[i])
            << "\"";
        out << ",\"coverage\":\"" << projectionCoverageToString(modes[i])
            << "\"";
        out << "}";
    }
    out << "]";
    out << ",\"syphon_on\":" << (m_syphon_active ? "true" : "false");
    out << ",\"server_name\":\"" << jsonEscape(m_syphon_server_name) << "\"";
    out << ",\"venue_validation\":\"pending\"";
    out << ",\"true_fulldome\":" << (m_true_fulldome ? "true" : "false");
    out << ",\"fallback_reason\":\"" << jsonEscape(m_fallback_reason) << "\"";
    out << "}";
    return out.str();
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
