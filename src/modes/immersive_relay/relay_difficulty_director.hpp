//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_DIFFICULTY_DIRECTOR_HPP
#define HEADER_RELAY_DIFFICULTY_DIRECTOR_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include <map>
#include <string>

class AbstractKart;
class World;

struct RelayDifficultyConfig
{
    bool m_enabled;
    std::string m_profile;
    std::string m_visibility;
    int m_min_intensity;
    int m_max_intensity;

    RelayDifficultyConfig();
};

struct RelayDifficultySample
{
    float m_dt;
    float m_race_time;
    int m_human_position;
    int m_kart_count;
    int m_current_lap;
    int m_total_laps;
    float m_lap_progress;
    int m_completed_handoffs;
    bool m_handoff_active;
    bool m_on_road;
    bool m_rescue_active;
    float m_human_speed;
    float m_human_max_speed;
    float m_gap_to_leader_m;
    float m_gap_to_next_m;

    RelayDifficultySample();
};

struct RelayDifficultySnapshot
{
    bool m_enabled;
    std::string m_profile;
    int m_intensity;
    std::string m_pressure;
    int m_human_gap_ms;
    int m_human_position;
    int m_assist_level;
    int m_ai_pace_level;
    std::string m_leaderboard_bucket;
    bool m_final_lap_lock;
    bool m_handoff_grace;

    RelayDifficultySnapshot();
};

class RelayDifficultyDirector
{
private:
    RelayDifficultyConfig m_config;
    RelayDifficultySnapshot m_snapshot;
    RelayDifficultySample m_last_sample;
    std::map<int, bool> m_original_ai_boost;
    std::string m_pressure;
    std::string m_candidate_pressure;
    float m_candidate_seconds;
    float m_sample_accumulator;
    float m_intensity;
    float m_gap_ms_ema;
    float m_offroad_score;
    float m_rescue_score;
    float m_dominating_seconds;
    float m_time_since_handoff;
    float m_next_human_assist_time;
    float m_next_ai_adjust_time;
    int m_last_completed_handoffs;
    bool m_has_sample;
    bool m_last_rescue_active;

    static int clampInt(int value, int low, int high);
    static float clampFloat(float value, float low, float high);
    static int roundToInt(float value);
    static bool isSpecialPressure(const std::string& pressure);
    static std::string bucketForConfig(const RelayDifficultyConfig& config);
    int timeToTicks(float seconds) const;
    int levelForIntensity() const;
    int computeTargetIntensity(const RelayDifficultySample& sample,
                               const std::string& pressure) const;
    std::string classifyPressure(const RelayDifficultySample& sample,
                                 int gap_ms,
                                 bool final_lap_lock,
                                 bool handoff_grace) const;
    void restoreAiBoost(World* world);
    void applyHumanAssist(AbstractKart* kart);
    void applyAiPacing(World* world, unsigned int local_players);

public:
    RelayDifficultyDirector();

    static RelayDifficultyConfig sanitizeConfig(
        const RelayDifficultyConfig& config);

    void reset();
    void setConfig(const RelayDifficultyConfig& config);
    bool update(const RelayDifficultySample& sample);
    void apply(AbstractKart* player_kart, World* world,
               unsigned int local_players);
    const RelayDifficultySnapshot& getSnapshot() const { return m_snapshot; }
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_DIFFICULTY_DIRECTOR_HPP
