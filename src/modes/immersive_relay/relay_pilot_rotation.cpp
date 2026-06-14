//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_pilot_rotation.hpp"

#include <algorithm>

RelayPlayer::RelayPlayer()
{
}

RelayPlayer::RelayPlayer(const std::string& player_id,
                         const std::string& display_name)
    : m_player_id(player_id), m_display_name(display_name)
{
}

RelayPilotRotation::RelayPilotRotation()
{
    m_policy = RELAY_HANDOFF_LAP_BASED;
    m_handoff_seconds = 10.0f;
    m_min_handoff_seconds = 6.0f;
    m_handoff_timeout_seconds = 12.0f;
    m_handoff_warning_seconds = 10.0f;
    m_segment_seconds = 90.0f;
    m_operator_confirm_required = false;
    m_operator_confirmed = false;
    reset();
}

void RelayPilotRotation::configure(const std::vector<RelayPlayer>& players,
                                   RelayHandoffPolicy policy,
                                   float handoff_seconds,
                                   float min_handoff_seconds,
                                   float handoff_timeout_seconds,
                                   float handoff_warning_seconds,
                                   float segment_seconds,
                                   bool operator_confirm_required)
{
    m_players = players;
    m_policy = policy;
    m_handoff_seconds = handoff_seconds > 0.0f ? handoff_seconds : 10.0f;
    m_min_handoff_seconds = min_handoff_seconds > 0.0f
        ? min_handoff_seconds : 6.0f;
    m_handoff_timeout_seconds = handoff_timeout_seconds > 0.0f
        ? handoff_timeout_seconds : 12.0f;
    m_handoff_warning_seconds = handoff_warning_seconds > 0.0f
        ? handoff_warning_seconds : 10.0f;
    m_segment_seconds = segment_seconds > 0.0f ? segment_seconds : 90.0f;
    m_operator_confirm_required = operator_confirm_required;
    reset();
}

void RelayPilotRotation::reset()
{
    m_active_index = m_players.empty() ? -1 : 0;
    m_pending_index = -1;
    m_previous_index = -1;
    m_handoff_active = false;
    m_handoff_warning_active = false;
    m_handoff_pending_safe_zone = false;
    m_operator_confirmed = false;
    m_handoff_started_at = 0.0f;
    m_last_handoff_at = 0.0f;
    m_next_timeout_at = m_segment_seconds;
    m_handoff_reason.clear();
    m_checkpoint_id = -1;
    m_completed_handoffs = 0;
}

int RelayPilotRotation::findPlayerIndex(const std::string& player_id) const
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i].m_player_id == player_id)
            return (int)i;
    }
    return -1;
}

int RelayPilotRotation::getNextPilotIndex() const
{
    if (m_players.empty())
        return -1;
    if (m_active_index < 0)
        return 0;
    return (m_active_index + 1) % (int)m_players.size();
}

bool RelayPilotRotation::policyAllowsCheckpoint() const
{
    return m_policy == RELAY_HANDOFF_CHECKPOINT_OR_TIMEOUT ||
           m_policy == RELAY_HANDOFF_CHECKPOINT_ONLY;
}

bool RelayPilotRotation::policyAllowsTimeout() const
{
    return m_policy == RELAY_HANDOFF_CHECKPOINT_OR_TIMEOUT ||
           m_policy == RELAY_HANDOFF_FIXED_TIME_SEGMENTS;
}

bool RelayPilotRotation::beginHandoff(int pending_index,
                                      const std::string& reason,
                                      int checkpoint_id,
                                      float race_time,
                                      bool safe_now)
{
    if (m_policy == RELAY_HANDOFF_SINGLE_PILOT_DEMO ||
        m_players.size() < 2 || pending_index < 0 ||
        pending_index >= (int)m_players.size() ||
        pending_index == m_active_index)
    {
        return false;
    }

    m_pending_index = pending_index;
    m_previous_index = m_active_index;
    m_handoff_reason = reason;
    m_checkpoint_id = checkpoint_id;
    m_handoff_warning_active = false;
    m_operator_confirmed = false;

    if (!safe_now)
    {
        m_handoff_pending_safe_zone = true;
        return true;
    }

    m_handoff_active = true;
    m_handoff_pending_safe_zone = false;
    m_handoff_started_at = race_time;
    return true;
}

bool RelayPilotRotation::finishHandoff(float race_time)
{
    if (!m_handoff_active && !m_handoff_pending_safe_zone)
        return false;

    if (m_pending_index >= 0)
    {
        m_previous_index = m_active_index;
        m_active_index = m_pending_index;
        m_completed_handoffs++;
    }

    m_pending_index = -1;
    m_handoff_active = false;
    m_handoff_warning_active = false;
    m_handoff_pending_safe_zone = false;
    m_operator_confirmed = false;
    m_handoff_started_at = 0.0f;
    m_last_handoff_at = race_time;
    m_next_timeout_at = race_time + m_segment_seconds;
    m_handoff_reason.clear();
    m_checkpoint_id = -1;
    return true;
}

bool RelayPilotRotation::update(float race_time, bool safe_now,
                                bool allow_new_handoff)
{
    if (m_handoff_active)
    {
        const float elapsed = race_time - m_handoff_started_at;
        if (elapsed >= m_handoff_timeout_seconds)
        {
            return finishHandoff(race_time);
        }
        if (m_operator_confirm_required)
        {
            if (m_operator_confirmed &&
                elapsed >= m_min_handoff_seconds)
            {
                return finishHandoff(race_time);
            }
            return false;
        }
        if (elapsed >= m_handoff_seconds)
        {
            return finishHandoff(race_time);
        }
        return false;
    }

    if (m_handoff_pending_safe_zone)
    {
        if (!allow_new_handoff)
        {
            m_pending_index = -1;
            m_previous_index = -1;
            m_handoff_pending_safe_zone = false;
            m_handoff_warning_active = false;
            m_operator_confirmed = false;
            m_handoff_reason.clear();
            m_checkpoint_id = -1;
            return true;
        }

        if (safe_now)
        {
            m_handoff_active = true;
            m_handoff_pending_safe_zone = false;
            m_handoff_started_at = race_time;
            return true;
        }
        return false;
    }

    if (!allow_new_handoff ||
        m_policy == RELAY_HANDOFF_SINGLE_PILOT_DEMO ||
        m_players.size() < 2)
    {
        const bool changed = m_handoff_warning_active;
        m_handoff_warning_active = false;
        return changed;
    }

    if (policyAllowsTimeout() && race_time >= m_next_timeout_at)
    {
        return beginHandoff(getNextPilotIndex(), "segment_timeout",
                            -1, race_time, safe_now);
    }

    if (policyAllowsTimeout() &&
        race_time >= m_next_timeout_at - m_handoff_warning_seconds)
    {
        if (!m_handoff_warning_active)
        {
            m_handoff_warning_active = true;
            return true;
        }
    }
    else if (m_handoff_warning_active)
    {
        m_handoff_warning_active = false;
        return true;
    }
    return false;
}

bool RelayPilotRotation::notifyCheckpoint(int checkpoint_id, float race_time,
                                          bool safe_now,
                                          bool allow_new_handoff)
{
    if (!allow_new_handoff)
        return false;

    if (!policyAllowsCheckpoint() || m_handoff_active ||
        m_handoff_pending_safe_zone)
        return false;

    if (race_time < m_next_timeout_at)
        return false;

    return beginHandoff(getNextPilotIndex(), "checkpoint",
                        checkpoint_id, race_time, safe_now);
}

bool RelayPilotRotation::notifyLap(int completed_laps, float race_time,
                                   bool safe_now, bool allow_new_handoff)
{
    if (!allow_new_handoff || m_policy != RELAY_HANDOFF_LAP_BASED ||
        m_handoff_active || m_handoff_pending_safe_zone ||
        completed_laps <= 0)
    {
        return false;
    }

    return beginHandoff(getNextPilotIndex(), "lap_complete",
                        -1000 - completed_laps, race_time, safe_now);
}

bool RelayPilotRotation::forceHandoff(const std::string& player_id,
                                      float race_time, bool safe_now,
                                      std::string* reject_reason)
{
    int pending_index = -1;
    if (!player_id.empty())
    {
        pending_index = findPlayerIndex(player_id);
        if (pending_index < 0)
        {
            if (reject_reason)
                *reject_reason = "unknown_player_id";
            return false;
        }
    }
    else
    {
        pending_index = getNextPilotIndex();
    }

    if (m_players.size() < 2)
    {
        if (reject_reason)
            *reject_reason = "not_enough_players";
        return false;
    }

    return beginHandoff(pending_index, "operator_forced",
                        -1, race_time, safe_now);
}

bool RelayPilotRotation::skipHandoff(float race_time)
{
    if (m_handoff_active &&
        race_time - m_handoff_started_at < m_min_handoff_seconds)
        return false;

    return finishHandoff(race_time);
}

bool RelayPilotRotation::confirmHandoff(float race_time,
                                        std::string* reject_reason)
{
    if (!m_handoff_active)
    {
        if (reject_reason)
            *reject_reason = "handoff_not_active";
        return false;
    }

    m_operator_confirmed = true;
    if (race_time - m_handoff_started_at < m_min_handoff_seconds)
        return true;

    return finishHandoff(race_time);
}

void RelayPilotRotation::setActiveIndexForTest(int active_index,
                                               float race_time)
{
    if (m_players.empty())
    {
        m_active_index = -1;
        return;
    }
    if (active_index < 0)
        active_index = 0;
    active_index = active_index % (int)m_players.size();
    m_previous_index = m_active_index;
    m_active_index = active_index;
    m_pending_index = -1;
    m_handoff_active = false;
    m_handoff_warning_active = false;
    m_handoff_pending_safe_zone = false;
    m_operator_confirmed = false;
    m_handoff_started_at = 0.0f;
    m_last_handoff_at = race_time;
    m_next_timeout_at = race_time + m_segment_seconds;
    m_handoff_reason.clear();
    m_checkpoint_id = -1;
}

RelayPilotRotationSnapshot
RelayPilotRotation::getSnapshot(float race_time) const
{
    RelayPilotRotationSnapshot s;
    s.m_active_index = m_active_index;
    s.m_pending_index = m_pending_index;
    s.m_handoff_active = m_handoff_active;
    s.m_handoff_warning_active = m_handoff_warning_active;
    s.m_handoff_pending_safe_zone = m_handoff_pending_safe_zone;
    s.m_waiting_for_operator_confirm =
        m_handoff_active && m_operator_confirm_required &&
        !m_operator_confirmed;
    s.m_handoff_remaining_ms = 0;
    s.m_handoff_warning_remaining_ms = 0;
    s.m_completed_handoffs = m_completed_handoffs;
    s.m_previous_index = m_previous_index;
    s.m_handoff_reason = m_handoff_reason;

    if (m_handoff_active)
    {
        const float remaining = std::max(0.0f,
            m_handoff_seconds - (race_time - m_handoff_started_at));
        s.m_handoff_remaining_ms = (int)(remaining * 1000.0f);
    }
    else if (m_handoff_warning_active)
    {
        const float remaining = std::max(0.0f,
            m_next_timeout_at - race_time);
        s.m_handoff_warning_remaining_ms = (int)(remaining * 1000.0f);
    }

    if (m_active_index >= 0 && m_active_index < (int)m_players.size())
    {
        s.m_active_player_id = m_players[m_active_index].m_player_id;
        s.m_active_display_name = m_players[m_active_index].m_display_name;
    }

    if (m_previous_index >= 0 && m_previous_index < (int)m_players.size())
    {
        s.m_previous_player_id = m_players[m_previous_index].m_player_id;
        s.m_previous_display_name = m_players[m_previous_index].m_display_name;
    }

    const int next_index = m_pending_index >= 0
        ? m_pending_index : getNextPilotIndex();
    if (next_index >= 0 && next_index < (int)m_players.size())
    {
        s.m_next_player_id = m_players[next_index].m_player_id;
        s.m_next_display_name = m_players[next_index].m_display_name;
    }

    return s;
}

#endif // ENABLE_IMMERSIVE_RELAY
