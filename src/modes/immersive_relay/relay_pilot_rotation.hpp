//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_PILOT_ROTATION_HPP
#define HEADER_RELAY_PILOT_ROTATION_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include <string>
#include <vector>

enum RelayHandoffPolicy
{
    RELAY_HANDOFF_CHECKPOINT_OR_TIMEOUT,
    RELAY_HANDOFF_CHECKPOINT_ONLY,
    RELAY_HANDOFF_FIXED_TIME_SEGMENTS,
    RELAY_HANDOFF_LAP_BASED,
    RELAY_HANDOFF_OPERATOR_MANUAL,
    RELAY_HANDOFF_SINGLE_PILOT_DEMO
};

struct RelayPlayer
{
    std::string m_player_id;
    std::string m_display_name;

    RelayPlayer();
    RelayPlayer(const std::string& player_id, const std::string& display_name);
};

struct RelayPilotRotationSnapshot
{
    int m_active_index;
    int m_pending_index;
    bool m_handoff_active;
    bool m_handoff_warning_active;
    bool m_handoff_pending_safe_zone;
    bool m_waiting_for_operator_confirm;
    int m_handoff_remaining_ms;
    int m_handoff_warning_remaining_ms;
    int m_completed_handoffs;
    int m_previous_index;
    std::string m_active_player_id;
    std::string m_active_display_name;
    std::string m_previous_player_id;
    std::string m_previous_display_name;
    std::string m_next_player_id;
    std::string m_next_display_name;
    std::string m_handoff_reason;
};

class RelayPilotRotation
{
private:
    std::vector<RelayPlayer> m_players;
    RelayHandoffPolicy m_policy;
    int m_active_index;
    int m_pending_index;
    int m_previous_index;
    bool m_handoff_active;
    bool m_handoff_warning_active;
    bool m_handoff_pending_safe_zone;
    bool m_operator_confirm_required;
    bool m_operator_confirmed;
    float m_handoff_started_at;
    float m_last_handoff_at;
    float m_next_timeout_at;
    float m_handoff_seconds;
    float m_min_handoff_seconds;
    float m_handoff_timeout_seconds;
    float m_handoff_warning_seconds;
    float m_segment_seconds;
    std::string m_handoff_reason;
    int m_checkpoint_id;
    int m_completed_handoffs;

    int findPlayerIndex(const std::string& player_id) const;
    int getNextPilotIndex() const;
    bool policyAllowsCheckpoint() const;
    bool policyAllowsTimeout() const;
    bool beginHandoff(int pending_index, const std::string& reason,
                      int checkpoint_id, float race_time, bool safe_now);
    bool finishHandoff(float race_time);

public:
    RelayPilotRotation();

    void configure(const std::vector<RelayPlayer>& players,
                   RelayHandoffPolicy policy,
                   float handoff_seconds,
                   float min_handoff_seconds,
                   float handoff_timeout_seconds,
                   float handoff_warning_seconds,
                   float segment_seconds,
                   bool operator_confirm_required=false);
    void reset();
    bool update(float race_time, bool safe_now, bool allow_new_handoff);
    bool notifyCheckpoint(int checkpoint_id, float race_time, bool safe_now,
                          bool allow_new_handoff);
    bool notifyLap(int completed_laps, float race_time, bool safe_now,
                   bool allow_new_handoff);
    bool forceHandoff(const std::string& player_id, float race_time,
                      bool safe_now, std::string* reject_reason);
    bool skipHandoff(float race_time);
    bool confirmHandoff(float race_time, std::string* reject_reason);
    void setActiveIndexForTest(int active_index, float race_time);

    bool isHandoffActive() const { return m_handoff_active; }
    bool isHandoffWarningActive() const { return m_handoff_warning_active; }
    bool isHandoffPendingSafeZone() const { return m_handoff_pending_safe_zone; }
    int getActiveIndex() const { return m_active_index; }
    RelayPilotRotationSnapshot getSnapshot(float race_time) const;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_PILOT_ROTATION_HPP
