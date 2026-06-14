//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_CREW_ROLE_MANAGER_HPP
#define HEADER_CREW_ROLE_MANAGER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_pilot_rotation.hpp"

#include <map>
#include <string>
#include <vector>

enum RelayCrewRole
{
    RELAY_CREW_ROLE_UNKNOWN,
    RELAY_CREW_ROLE_NAVIGATOR,
    RELAY_CREW_ROLE_ENGINEER,
    RELAY_CREW_ROLE_DEFENDER,
    RELAY_CREW_ROLE_HACKER,
    RELAY_CREW_ROLE_GUNNER
};

struct RelayCrewActionDefinition
{
    std::string m_type;
    RelayCrewRole m_role;
    int m_cooldown_ms;
    int m_score_value;

    RelayCrewActionDefinition();
    RelayCrewActionDefinition(const std::string& type, RelayCrewRole role,
                              int cooldown_ms, int score_value);
};

struct RelayCrewRoleAssignment
{
    std::string m_player_id;
    std::string m_display_name;
    bool m_is_pilot;
    std::vector<RelayCrewRole> m_roles;

    RelayCrewRoleAssignment();
};

struct RelayCrewActionRequest
{
    std::string m_session_id;
    std::string m_action_id;
    std::string m_idempotency_key;
    unsigned int m_state_version_seen;
    std::string m_player_id;
    RelayCrewRole m_role;
    std::string m_role_name;
    std::string m_type;
    std::string m_target_id;
    std::string m_mode;
    std::string m_payload_json;
    int m_amount;
    int m_duration_ms;

    RelayCrewActionRequest();
};

struct RelayCrewActionAck
{
    bool m_accepted;
    bool m_duplicate;
    std::string m_error;
    std::string m_event_name;
    std::string m_effect_summary;
    std::string m_idempotency_key;
    int m_score_delta;
    int m_total_score;
    int m_cooldown_ms;
    unsigned int m_state_version;

    RelayCrewActionAck();
};

struct RelayCrewTrackEffect
{
    std::string m_key;
    std::string m_value;

    RelayCrewTrackEffect();
    RelayCrewTrackEffect(const std::string& key, const std::string& value);
};

struct RelayCrewCooldownSnapshot
{
    std::string m_player_id;
    RelayCrewRole m_role;
    std::string m_action_type;
    int m_remaining_ms;

    RelayCrewCooldownSnapshot();
};

struct RelayCrewActionHistory
{
    std::string m_player_id;
    RelayCrewRole m_role;
    std::string m_type;
    std::string m_action_id;
    std::string m_idempotency_key;
    std::string m_result;
    std::string m_reason;
    std::string m_effect_summary;
    int m_score_delta;
    int m_cooldown_ms;
    unsigned int m_state_version;

    RelayCrewActionHistory();
};

class CrewRoleManager
{
private:
    std::vector<RelayPlayer> m_players;
    std::vector<RelayCrewRoleAssignment> m_assignments;
    std::map<std::string, int> m_ready_at_ms;

    static const std::vector<RelayCrewActionDefinition>& getDefinitions();
    std::string cooldownKey(const std::string& player_id,
                            const std::string& action_type) const;
    const RelayCrewRoleAssignment* getAssignment(
        const std::string& player_id) const;

public:
    CrewRoleManager();

    void configure(const std::vector<RelayPlayer>& players,
                   int active_pilot_index);
    void resetCooldowns();

    static RelayCrewRole roleFromString(const std::string& role);
    static const char* roleToString(RelayCrewRole role);
    static bool isCrewRole(RelayCrewRole role);
    static const RelayCrewActionDefinition* getActionDefinition(
        const std::string& action_type);
    static std::vector<RelayCrewActionDefinition>
        getActionDefinitionsSnapshot();

    bool playerHasRole(const std::string& player_id,
                       RelayCrewRole role) const;
    bool validateAction(const RelayCrewActionRequest& request,
                        unsigned int current_state_version,
                        int now_ms,
                        std::string* reject_reason,
                        int* cooldown_remaining_ms,
                        RelayCrewActionDefinition* definition) const;
    void markActionAccepted(const RelayCrewActionRequest& request,
                            int now_ms);
    int getCooldownRemainingMs(const std::string& player_id,
                               const std::string& action_type,
                               int now_ms) const;
    std::vector<RelayCrewCooldownSnapshot> getCooldownsSnapshot(
        int now_ms) const;
    std::vector<RelayCrewRoleAssignment> getAssignmentsSnapshot() const;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_CREW_ROLE_MANAGER_HPP
