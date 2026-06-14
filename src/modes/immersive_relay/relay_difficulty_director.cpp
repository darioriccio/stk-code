//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_difficulty_director.hpp"

#include "config/stk_config.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/max_speed.hpp"
#include "modes/world.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <cmath>

RelayDifficultyConfig::RelayDifficultyConfig()
{
    m_enabled = false;
    m_profile = "arcade_balanced";
    m_visibility = "leaderboard_bucket";
    m_min_intensity = 0;
    m_max_intensity = 100;
}

RelayDifficultySample::RelayDifficultySample()
{
    m_dt = 0.0f;
    m_race_time = 0.0f;
    m_human_position = 1;
    m_kart_count = 1;
    m_current_lap = 1;
    m_total_laps = 1;
    m_lap_progress = 0.0f;
    m_completed_handoffs = 0;
    m_handoff_active = false;
    m_on_road = true;
    m_rescue_active = false;
    m_human_speed = 0.0f;
    m_human_max_speed = 0.0f;
    m_gap_to_leader_m = 0.0f;
    m_gap_to_next_m = 0.0f;
}

RelayDifficultySnapshot::RelayDifficultySnapshot()
{
    m_enabled = false;
    m_profile = "arcade_balanced";
    m_intensity = 0;
    m_pressure = "neutral";
    m_human_gap_ms = 0;
    m_human_position = 1;
    m_assist_level = 0;
    m_ai_pace_level = 0;
    m_leaderboard_bucket = "arcade_fixed";
    m_final_lap_lock = false;
    m_handoff_grace = false;
}

RelayDifficultyDirector::RelayDifficultyDirector()
{
    reset();
}

int RelayDifficultyDirector::clampInt(int value, int low, int high)
{
    return std::max(low, std::min(high, value));
}

float RelayDifficultyDirector::clampFloat(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

int RelayDifficultyDirector::roundToInt(float value)
{
    return value >= 0.0f ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

bool RelayDifficultyDirector::isSpecialPressure(const std::string& pressure)
{
    return pressure == "handoff_grace" || pressure == "final_lap_lock";
}

std::string RelayDifficultyDirector::bucketForConfig(
    const RelayDifficultyConfig& config)
{
    if (config.m_enabled)
        return "arcade_adaptive";
    if (config.m_profile == "solo_no_assist")
        return "solo_no_assist";
    return "arcade_fixed";
}

RelayDifficultyConfig RelayDifficultyDirector::sanitizeConfig(
    const RelayDifficultyConfig& config)
{
    RelayDifficultyConfig sanitized = config;
    if (sanitized.m_profile.empty())
        sanitized.m_profile = "arcade_balanced";
    if (sanitized.m_visibility.empty())
        sanitized.m_visibility = "leaderboard_bucket";
    sanitized.m_min_intensity =
        clampInt(sanitized.m_min_intensity, 0, 100);
    sanitized.m_max_intensity =
        clampInt(sanitized.m_max_intensity, 0, 100);
    if (sanitized.m_max_intensity < sanitized.m_min_intensity)
        sanitized.m_max_intensity = sanitized.m_min_intensity;
    return sanitized;
}

void RelayDifficultyDirector::reset()
{
    m_config = RelayDifficultyConfig();
    m_snapshot = RelayDifficultySnapshot();
    m_last_sample = RelayDifficultySample();
    m_original_ai_boost.clear();
    m_pressure = "neutral";
    m_candidate_pressure = "neutral";
    m_candidate_seconds = 0.0f;
    m_sample_accumulator = 0.0f;
    m_intensity = 0.0f;
    m_gap_ms_ema = 0.0f;
    m_offroad_score = 0.0f;
    m_rescue_score = 0.0f;
    m_dominating_seconds = 0.0f;
    m_time_since_handoff = 999.0f;
    m_next_human_assist_time = 0.0f;
    m_next_ai_adjust_time = 0.0f;
    m_last_completed_handoffs = -1;
    m_has_sample = false;
    m_last_rescue_active = false;
}

void RelayDifficultyDirector::setConfig(const RelayDifficultyConfig& config)
{
    RelayDifficultyConfig sanitized = sanitizeConfig(config);
    if (sanitized.m_enabled == m_config.m_enabled &&
        sanitized.m_profile == m_config.m_profile &&
        sanitized.m_visibility == m_config.m_visibility &&
        sanitized.m_min_intensity == m_config.m_min_intensity &&
        sanitized.m_max_intensity == m_config.m_max_intensity)
    {
        return;
    }

    m_config = sanitized;
    m_intensity = clampFloat(m_intensity,
        (float)m_config.m_min_intensity, (float)m_config.m_max_intensity);
    if (!m_config.m_enabled)
    {
        m_intensity = 0.0f;
        m_pressure = "neutral";
        m_candidate_pressure = "neutral";
    }

    m_snapshot.m_enabled = m_config.m_enabled;
    m_snapshot.m_profile = m_config.m_profile;
    m_snapshot.m_intensity = roundToInt(m_intensity);
    m_snapshot.m_pressure = m_pressure;
    m_snapshot.m_leaderboard_bucket = bucketForConfig(m_config);
}

int RelayDifficultyDirector::timeToTicks(float seconds) const
{
    return stk_config ? stk_config->time2Ticks(seconds)
        : (int)(seconds * 60.0f);
}

int RelayDifficultyDirector::levelForIntensity() const
{
    if (m_intensity < 18.0f)
        return 0;
    if (m_intensity < 42.0f)
        return 1;
    if (m_intensity < 68.0f)
        return 2;
    return 3;
}

std::string RelayDifficultyDirector::classifyPressure(
    const RelayDifficultySample& sample, int gap_ms,
    bool final_lap_lock, bool handoff_grace) const
{
    if (handoff_grace)
        return "handoff_grace";
    if (final_lap_lock)
        return "final_lap_lock";

    const bool last_or_near_last =
        sample.m_kart_count >= 3 &&
        sample.m_human_position >= sample.m_kart_count - 1;
    const bool back_half =
        sample.m_kart_count >= 3 &&
        sample.m_human_position > (sample.m_kart_count + 1) / 2;
    const float error_score = m_offroad_score + m_rescue_score;

    if (gap_ms > 12000 ||
        (last_or_near_last && gap_ms > 6000) ||
        (error_score > 4.0f && gap_ms > 3500))
    {
        return "struggling";
    }

    if (gap_ms > 2500 || back_half)
        return "recovering";

    if (sample.m_human_position == 1 && gap_ms < -6000)
        return "dominating";

    return "neutral";
}

int RelayDifficultyDirector::computeTargetIntensity(
    const RelayDifficultySample& sample,
    const std::string& pressure) const
{
    const int gap_ms = roundToInt(m_gap_ms_ema);
    int target = 0;

    if (pressure == "struggling")
    {
        target = 60 + clampInt((gap_ms - 6000) / 600, 0, 30);
        if (!sample.m_on_road || sample.m_rescue_active)
            target += 8;
    }
    else if (pressure == "recovering")
    {
        target = 28 + clampInt(gap_ms / 700, 0, 28);
        if (sample.m_human_position > (sample.m_kart_count + 1) / 2)
            target += 8;
    }
    else if (pressure == "dominating")
    {
        target = 28 + clampInt((-gap_ms - 6000) / 1000, 0, 28);
        if (m_dominating_seconds > 18.0f)
            target += 10;
    }
    else if (pressure == "final_lap_lock")
    {
        target = std::min(roundToInt(m_intensity), 32);
    }
    else if (pressure == "handoff_grace")
    {
        target = std::min(roundToInt(m_intensity), 18);
    }

    if (pressure == "neutral")
        target = 0;

    return clampInt(target, m_config.m_min_intensity,
                    m_config.m_max_intensity);
}

bool RelayDifficultyDirector::update(const RelayDifficultySample& sample)
{
    const RelayDifficultySnapshot previous = m_snapshot;

    m_last_sample = sample;
    m_snapshot.m_enabled = m_config.m_enabled;
    m_snapshot.m_profile = m_config.m_profile;
    m_snapshot.m_leaderboard_bucket = bucketForConfig(m_config);
    m_snapshot.m_human_position = sample.m_human_position;

    if (!m_config.m_enabled)
    {
        m_intensity = 0.0f;
        m_pressure = "neutral";
        m_snapshot.m_intensity = 0;
        m_snapshot.m_pressure = "neutral";
        m_snapshot.m_human_gap_ms = 0;
        m_snapshot.m_assist_level = 0;
        m_snapshot.m_ai_pace_level = 0;
        m_snapshot.m_final_lap_lock = false;
        m_snapshot.m_handoff_grace = false;
        return previous.m_enabled != m_snapshot.m_enabled ||
               previous.m_leaderboard_bucket !=
                   m_snapshot.m_leaderboard_bucket;
    }

    const float dt = clampFloat(sample.m_dt, 0.0f, 1.0f);
    m_sample_accumulator += dt;
    if (m_sample_accumulator < 0.25f && m_has_sample)
        return false;

    const float sample_dt = std::max(0.001f, m_sample_accumulator);
    m_sample_accumulator = 0.0f;

    if (m_last_completed_handoffs < 0)
    {
        m_last_completed_handoffs = sample.m_completed_handoffs;
    }
    else if (sample.m_completed_handoffs != m_last_completed_handoffs)
    {
        m_time_since_handoff = 0.0f;
        m_last_completed_handoffs = sample.m_completed_handoffs;
    }
    else
    {
        m_time_since_handoff += sample_dt;
    }

    if (sample.m_handoff_active)
        m_time_since_handoff = 0.0f;

    m_offroad_score = sample.m_on_road
        ? std::max(0.0f, m_offroad_score - sample_dt * 0.5f)
        : std::min(8.0f, m_offroad_score + sample_dt);
    if (sample.m_rescue_active && !m_last_rescue_active)
        m_rescue_score = std::min(8.0f, m_rescue_score + 2.5f);
    else
        m_rescue_score = std::max(0.0f, m_rescue_score - sample_dt * 0.18f);
    m_last_rescue_active = sample.m_rescue_active;

    const float speed_ref = std::max(8.0f,
        sample.m_human_max_speed > 0.0f
            ? std::min(sample.m_human_max_speed, 32.0f)
            : std::max(sample.m_human_speed, 12.0f));
    const float raw_gap_ms =
        sample.m_gap_to_leader_m > 0.0f
            ? 1000.0f * sample.m_gap_to_leader_m / speed_ref
            : -1000.0f * sample.m_gap_to_next_m / speed_ref;
    if (!m_has_sample)
        m_gap_ms_ema = raw_gap_ms;
    else
    {
        const float alpha = clampFloat(sample_dt / 16.0f, 0.04f, 0.35f);
        m_gap_ms_ema += alpha * (raw_gap_ms - m_gap_ms_ema);
    }
    m_has_sample = true;

    const bool final_lap_lock =
        sample.m_total_laps > 0 &&
        sample.m_current_lap >= sample.m_total_laps &&
        sample.m_lap_progress >= 0.68f;
    const bool handoff_grace =
        sample.m_handoff_active || m_time_since_handoff < 7.0f;
    const int gap_ms = roundToInt(m_gap_ms_ema);
    std::string candidate =
        classifyPressure(sample, gap_ms, final_lap_lock, handoff_grace);

    if (candidate != m_candidate_pressure)
    {
        m_candidate_pressure = candidate;
        m_candidate_seconds = 0.0f;
    }
    else
    {
        m_candidate_seconds += sample_dt;
    }

    if (candidate == "dominating")
        m_dominating_seconds += sample_dt;
    else
        m_dominating_seconds = std::max(0.0f,
            m_dominating_seconds - sample_dt * 0.5f);

    const bool accept_pressure =
        isSpecialPressure(candidate) || candidate == m_pressure ||
        m_candidate_seconds >= 3.0f;
    if (accept_pressure)
        m_pressure = candidate;

    const int target =
        computeTargetIntensity(sample, accept_pressure ? candidate
                                                       : m_pressure);
    float rate_up = m_pressure == "dominating" ? 4.5f : 9.0f;
    float rate_down = 12.0f;
    if (final_lap_lock)
    {
        rate_up = 2.0f;
        rate_down = 4.0f;
    }
    if (handoff_grace)
    {
        rate_up = 0.0f;
        rate_down = 7.0f;
    }

    const float delta = (float)target - m_intensity;
    const float limited_delta = clampFloat(delta,
        -rate_down * sample_dt, rate_up * sample_dt);
    m_intensity = clampFloat(m_intensity + limited_delta,
        (float)m_config.m_min_intensity,
        (float)m_config.m_max_intensity);

    int assist = 0;
    int ai_pace = 0;
    const int level = levelForIntensity();
    if (m_pressure == "struggling")
    {
        assist = level;
        ai_pace = level >= 3 ? -2 : (level > 0 ? -1 : 0);
    }
    else if (m_pressure == "recovering")
    {
        assist = std::min(2, level);
        ai_pace = level >= 2 ? -1 : 0;
    }
    else if (m_pressure == "dominating")
    {
        assist = 0;
        ai_pace = level >= 3 ? 2 : (level > 0 ? 1 : 0);
    }
    else if (m_pressure == "final_lap_lock")
    {
        assist = std::min(1, level);
        ai_pace = 0;
    }

    if (m_pressure == "handoff_grace")
    {
        assist = 0;
        ai_pace = 0;
    }

    m_snapshot.m_intensity = roundToInt(m_intensity);
    m_snapshot.m_pressure = m_pressure;
    m_snapshot.m_human_gap_ms = gap_ms;
    m_snapshot.m_assist_level = assist;
    m_snapshot.m_ai_pace_level = ai_pace;
    m_snapshot.m_final_lap_lock = final_lap_lock;
    m_snapshot.m_handoff_grace = handoff_grace;

    const int previous_bucket = previous.m_intensity / 10;
    const int current_bucket = m_snapshot.m_intensity / 10;
    const bool notable_change =
        previous.m_enabled != m_snapshot.m_enabled ||
        previous.m_pressure != m_snapshot.m_pressure ||
        previous.m_assist_level != m_snapshot.m_assist_level ||
        previous.m_ai_pace_level != m_snapshot.m_ai_pace_level ||
        previous.m_leaderboard_bucket != m_snapshot.m_leaderboard_bucket ||
        previous_bucket != current_bucket;

    if (notable_change)
    {
        Log::info("ImmersiveRelay",
                  "Adaptive difficulty pressure=%s intensity=%d gap_ms=%d "
                  "position=%d assist=%d ai_pace=%d bucket=%s.",
                  m_snapshot.m_pressure.c_str(),
                  m_snapshot.m_intensity,
                  m_snapshot.m_human_gap_ms,
                  m_snapshot.m_human_position,
                  m_snapshot.m_assist_level,
                  m_snapshot.m_ai_pace_level,
                  m_snapshot.m_leaderboard_bucket.c_str());
    }

    return previous.m_enabled != m_snapshot.m_enabled ||
           previous.m_pressure != m_snapshot.m_pressure ||
           previous.m_assist_level != m_snapshot.m_assist_level ||
           previous.m_ai_pace_level != m_snapshot.m_ai_pace_level ||
           previous.m_leaderboard_bucket != m_snapshot.m_leaderboard_bucket;
}

void RelayDifficultyDirector::restoreAiBoost(World* world)
{
    if (!world)
    {
        m_original_ai_boost.clear();
        return;
    }

    for (std::map<int, bool>::const_iterator i = m_original_ai_boost.begin();
         i != m_original_ai_boost.end(); i++)
    {
        if (i->first < 0 || i->first >= (int)world->getNumKarts())
            continue;
        AbstractKart* kart = world->getKart(i->first);
        if (kart && !kart->isEliminated())
            kart->setBoostAI(i->second);
    }
    m_original_ai_boost.clear();
}

void RelayDifficultyDirector::applyHumanAssist(AbstractKart* kart)
{
    if (!kart || kart->isEliminated() || kart->hasFinishedRace())
        return;
    if (m_snapshot.m_assist_level <= 0)
        return;
    if (m_last_sample.m_race_time < m_next_human_assist_time)
        return;

    const int level = m_snapshot.m_assist_level;
    m_next_human_assist_time =
        m_last_sample.m_race_time + (level >= 2 ? 2.0f : 3.5f);

    if (m_snapshot.m_human_gap_ms > 3000 && kart->getEnergy() < 8.5f)
    {
        const float energy = level >= 2 ? 0.18f : 0.10f;
        kart->setEnergy(std::min(10.0f, kart->getEnergy() + energy));
    }

    const float max_speed = m_last_sample.m_human_max_speed > 0.0f
        ? m_last_sample.m_human_max_speed : kart->getCurrentMaxSpeed();
    const bool needs_recovery_push =
        !m_last_sample.m_on_road ||
        m_last_sample.m_rescue_active ||
        (max_speed > 0.0f && m_last_sample.m_human_speed < max_speed * 0.72f);

    if (level >= 2 && needs_recovery_push && !kart->getKartAnimation())
    {
        kart->increaseMaxSpeed(MaxSpeed::MS_INCREASE_RUBBER,
            0.7f + 0.35f * (float)level,
            24.0f + 8.0f * (float)level,
            timeToTicks(0.7f + 0.2f * (float)level),
            timeToTicks(0.9f));
        if (max_speed > 0.0f && m_last_sample.m_human_speed < max_speed * 0.45f)
            kart->adjustSpeed(1.012f + 0.004f * (float)level);
    }

    if (level >= 3 && (m_last_sample.m_rescue_active ||
                       !m_last_sample.m_on_road))
    {
        kart->setInvulnerableTicks(timeToTicks(0.8f));
    }
}

void RelayDifficultyDirector::applyAiPacing(World* world,
                                            unsigned int local_players)
{
    if (!world)
        return;

    if (m_snapshot.m_ai_pace_level <= 0)
        restoreAiBoost(world);

    if (m_snapshot.m_ai_pace_level == 0)
        return;

    if (m_snapshot.m_ai_pace_level > 0)
    {
        for (unsigned int i = local_players; i < world->getNumKarts(); i++)
        {
            AbstractKart* kart = world->getKart((int)i);
            if (!kart || kart->isEliminated() || kart->hasFinishedRace())
                continue;
            if (m_original_ai_boost.find((int)i) ==
                m_original_ai_boost.end())
            {
                m_original_ai_boost[(int)i] = kart->getBoostAI();
            }
            kart->setBoostAI(true);
        }
        return;
    }

    if (m_last_sample.m_race_time < m_next_ai_adjust_time)
        return;

    const int level = -m_snapshot.m_ai_pace_level;
    const float speed_factor = level >= 2 ? 0.985f : 0.993f;
    const float speed_cap = level >= 2 ? 0.94f : 0.97f;
    m_next_ai_adjust_time = m_last_sample.m_race_time + 0.9f;

    for (unsigned int i = local_players; i < world->getNumKarts(); i++)
    {
        AbstractKart* kart = world->getKart((int)i);
        if (!kart || kart->isEliminated() || kart->hasFinishedRace())
            continue;

        kart->setSlowdown(MaxSpeed::MS_DECREASE_AI,
            speed_cap, timeToTicks(0.25f));
        if (std::fabs(kart->getSpeed()) >
            std::max(10.0f, m_last_sample.m_human_speed + 1.5f))
        {
            kart->adjustSpeed(speed_factor);
        }
    }
}

void RelayDifficultyDirector::apply(AbstractKart* player_kart, World* world,
                                    unsigned int local_players)
{
    if (!m_config.m_enabled)
    {
        restoreAiBoost(world);
        return;
    }

    if (m_snapshot.m_handoff_grace)
    {
        restoreAiBoost(world);
        return;
    }

    applyHumanAssist(player_kart);
    applyAiPacing(world, local_players);
}

#endif // ENABLE_IMMERSIVE_RELAY
