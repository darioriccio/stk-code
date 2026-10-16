// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Test boundaries only: no GPU/world or adaptive-difficulty execution. Native
// controller, rotation, arcade transitions and legacy role/director are real.
#include "io/file_manager.hpp"
#include "modes/immersive_relay/relay_difficulty_director.hpp"
#include <algorithm>
FileManager* file_manager=nullptr;
RelayDifficultyConfig::RelayDifficultyConfig():m_enabled(false),m_profile("arcade_balanced"),
    m_visibility("leaderboard_bucket"),m_min_intensity(0),m_max_intensity(100){}
RelayDifficultySnapshot::RelayDifficultySnapshot():m_enabled(false),m_profile("arcade_balanced"),
    m_intensity(0),m_pressure("neutral"),m_human_gap_ms(0),m_human_position(1),m_assist_level(0),
    m_ai_pace_level(0),m_leaderboard_bucket("arcade_fixed"),m_final_lap_lock(false),m_handoff_grace(false){}
RelayDifficultyConfig RelayDifficultyDirector::sanitizeConfig(const RelayDifficultyConfig& config)
{
    RelayDifficultyConfig out=config;
    if(out.m_profile.empty())out.m_profile="arcade_balanced";
    if(out.m_visibility.empty())out.m_visibility="leaderboard_bucket";
    out.m_min_intensity=std::max(0,std::min(100,out.m_min_intensity));
    out.m_max_intensity=std::max(out.m_min_intensity,std::max(0,std::min(100,out.m_max_intensity)));
    return out;
}
