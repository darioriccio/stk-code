//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_IMMERSIVE_RENDER_STATS_HPP
#define HEADER_IMMERSIVE_RENDER_STATS_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/immersive_render_config.hpp"

#include <deque>
#include <string>

namespace ImmersiveRelay
{

class ImmersiveRenderStats
{
private:
    std::deque<float> m_frame_times_ms;
    float percentile(float pct) const;
    static std::string jsonEscape(const std::string& input);

public:
    ImmersiveOutputMode m_output_mode;
    ImmersiveProjectionMode m_projection_mode;
    ImmersiveCameraMode m_camera_mode;
    unsigned int m_domemaster_size;
    unsigned int m_projection_width;
    unsigned int m_projection_height;
    unsigned int m_cubemap_size;
    float m_avg_fps;
    float m_avg_frame_time_ms;
    float m_p95_frame_time_ms;
    float m_p99_frame_time_ms;
    float m_last_frame_time_ms;
    float m_capture_time_ms;
    float m_cubemap_render_time_ms;
    float m_projection_time_ms;
    float m_output_publish_time_ms;
    float m_syphon_publish_time_ms;
    unsigned int m_dropped_frames;
    unsigned int m_publish_skipped;
    int m_active_driver_index;
    std::string m_active_driver_id;
    int m_lap;
    int m_total_laps;
    std::string m_current_encounter;
    float m_yaw_deg;
    float m_pitch_deg;
    float m_roll_deg;
    float m_dome_tilt_deg;
    float m_roll_limit_deg;
    float m_forward_offset;
    float m_vertical_offset;
    float m_smoothing_factor;
    bool m_horizon_stabilization;
    bool m_output_active;
    bool m_output_available;
    bool m_syphon_active;
    std::string m_syphon_server_name;
    bool m_true_fulldome;
    std::string m_fallback_reason;

    ImmersiveRenderStats();
    void resetRolling();
    void beginFrame(const ImmersiveRenderConfig& config);
    void recordFrame(float frame_time_ms);
    std::string toJson() const;
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_RENDER_STATS_HPP
