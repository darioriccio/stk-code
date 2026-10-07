//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_role_manager.hpp"

#include <algorithm>

RelayCrewActionDefinition::RelayCrewActionDefinition()
{
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_cooldown_ms = 0;
    m_score_value = 0;
}

RelayCrewActionDefinition::RelayCrewActionDefinition(
    const std::string& type, RelayCrewRole role, int cooldown_ms,
    int score_value)
    : m_type(type), m_role(role), m_cooldown_ms(cooldown_ms),
      m_score_value(score_value)
{
}

RelayCrewRoleAssignment::RelayCrewRoleAssignment()
{
    m_is_pilot = false;
}

RelayCrewActionRequest::RelayCrewActionRequest()
{
    m_state_version_seen = 0;
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_amount = 0;
    m_duration_ms = 0;
}

RelayCrewActionAck::RelayCrewActionAck()
{
    m_accepted = false;
    m_duplicate = false;
    m_score_delta = 0;
    m_total_score = 0;
    m_cooldown_ms = 0;
    m_state_version = 0;
}

RelayCrewTrackEffect::RelayCrewTrackEffect()
{
}

RelayCrewTrackEffect::RelayCrewTrackEffect(const std::string& key,
                                           const std::string& value)
    : m_key(key), m_value(value)
{
}

RelayCrewCooldownSnapshot::RelayCrewCooldownSnapshot()
{
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_remaining_ms = 0;
}

RelayCrewActionHistory::RelayCrewActionHistory()
{
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_score_delta = 0;
    m_cooldown_ms = 0;
    m_state_version = 0;
}

CrewRoleManager::CrewRoleManager()
{
}

const std::vector<RelayCrewActionDefinition>&
CrewRoleManager::getDefinitions()
{
    static std::vector<RelayCrewActionDefinition> definitions;
    if (!definitions.empty())
        return definitions;

    definitions.push_back(RelayCrewActionDefinition(
        "nav_select_route", RELAY_CREW_ROLE_NAVIGATOR, 7000, 90));
    definitions.push_back(RelayCrewActionDefinition(
        "nav_mark_hazard", RELAY_CREW_ROLE_NAVIGATOR, 6000, 70));
    definitions.push_back(RelayCrewActionDefinition(
        "nav_call_pace_note", RELAY_CREW_ROLE_NAVIGATOR, 5000, 50));
    definitions.push_back(RelayCrewActionDefinition(
        "nav_reveal_shortcut", RELAY_CREW_ROLE_NAVIGATOR, 14000, 110));
    definitions.push_back(RelayCrewActionDefinition(
        "nav_request_role", RELAY_CREW_ROLE_NAVIGATOR, 8000, 40));

    definitions.push_back(RelayCrewActionDefinition(
        "engineer_charge_boost", RELAY_CREW_ROLE_ENGINEER, 3000, 35));
    definitions.push_back(RelayCrewActionDefinition(
        "engineer_release_boost", RELAY_CREW_ROLE_ENGINEER, 12000, 160));
    definitions.push_back(RelayCrewActionDefinition(
        "engineer_dump_heat", RELAY_CREW_ROLE_ENGINEER, 10000, 80));
    definitions.push_back(RelayCrewActionDefinition(
        "engineer_stabilize_grip", RELAY_CREW_ROLE_ENGINEER, 14000, 100));
    definitions.push_back(RelayCrewActionDefinition(
        "engineer_rebalance_power", RELAY_CREW_ROLE_ENGINEER, 9000, 85));

    definitions.push_back(RelayCrewActionDefinition(
        "defender_raise_shield", RELAY_CREW_ROLE_DEFENDER, 12000, 110));
    definitions.push_back(RelayCrewActionDefinition(
        "defender_cleanse_status", RELAY_CREW_ROLE_DEFENDER, 12000, 90));
    definitions.push_back(RelayCrewActionDefinition(
        "defender_absorb_hit", RELAY_CREW_ROLE_DEFENDER, 9000, 95));
    definitions.push_back(RelayCrewActionDefinition(
        "defender_repair_armor_small", RELAY_CREW_ROLE_DEFENDER, 10000, 70));
    definitions.push_back(RelayCrewActionDefinition(
        "defender_emergency_bubble", RELAY_CREW_ROLE_DEFENDER, 20000, 140));

    definitions.push_back(RelayCrewActionDefinition(
        "hacker_open_gate", RELAY_CREW_ROLE_HACKER, 10000, 120));
    definitions.push_back(RelayCrewActionDefinition(
        "hacker_close_hazard_barrier", RELAY_CREW_ROLE_HACKER, 12000, 100));
    definitions.push_back(RelayCrewActionDefinition(
        "hacker_rotate_ramp", RELAY_CREW_ROLE_HACKER, 14000, 110));
    definitions.push_back(RelayCrewActionDefinition(
        "hacker_arm_shortcut", RELAY_CREW_ROLE_HACKER, 15000, 130));
    definitions.push_back(RelayCrewActionDefinition(
        "hacker_disable_trap", RELAY_CREW_ROLE_HACKER, 11000, 105));

    definitions.push_back(RelayCrewActionDefinition(
        "gunner_lock_target", RELAY_CREW_ROLE_GUNNER, 4000, 60));
    definitions.push_back(RelayCrewActionDefinition(
        "gunner_fire_utility", RELAY_CREW_ROLE_GUNNER, 10000, 115));
    definitions.push_back(RelayCrewActionDefinition(
        "gunner_emp_zone", RELAY_CREW_ROLE_GUNNER, 16000, 135));
    definitions.push_back(RelayCrewActionDefinition(
        "gunner_drop_decoy", RELAY_CREW_ROLE_GUNNER, 12000, 90));
    definitions.push_back(RelayCrewActionDefinition(
        "gunner_tag_rival", RELAY_CREW_ROLE_GUNNER, 8000, 85));

    return definitions;
}

void CrewRoleManager::configure(const std::vector<RelayPlayer>& players,
                                int active_pilot_index,
                                const std::vector<std::string>& connected_player_ids)
{
    m_players = players;
    m_assignments.clear();

    for (unsigned int i = 0; i < players.size(); i++)
    {
        RelayCrewRoleAssignment assignment;
        assignment.m_player_id = players[i].m_player_id;
        assignment.m_display_name = players[i].m_display_name;
        assignment.m_is_pilot = (int)i == active_pilot_index;
        m_assignments.push_back(assignment);
    }

    std::vector<int> crew_indices;
    for (unsigned int i = 0; i < m_assignments.size(); i++)
    {
        if (!m_assignments[i].m_is_pilot &&
            std::find(connected_player_ids.begin(), connected_player_ids.end(),
                      m_assignments[i].m_player_id) != connected_player_ids.end())
            crew_indices.push_back((int)i);
    }

    // In solo play the optional phone provides support for the shared kart.
    if (crew_indices.empty() && m_assignments.size() == 1 &&
        std::find(connected_player_ids.begin(), connected_player_ids.end(),
                  m_assignments[0].m_player_id) != connected_player_ids.end())
        crew_indices.push_back(0);
    if (crew_indices.empty())
        return;

    if (crew_indices.size() == 1)
    {
        RelayCrewRole all_roles[] = {
            RELAY_CREW_ROLE_NAVIGATOR,
            RELAY_CREW_ROLE_ENGINEER,
            RELAY_CREW_ROLE_DEFENDER,
            RELAY_CREW_ROLE_HACKER,
            RELAY_CREW_ROLE_GUNNER
        };
        m_assignments[crew_indices[0]].m_roles.assign(
            all_roles, all_roles + 5);
        return;
    }

    if (crew_indices.size() == 2)
    {
        m_assignments[crew_indices[0]].m_roles.push_back(
            RELAY_CREW_ROLE_NAVIGATOR);
        m_assignments[crew_indices[0]].m_roles.push_back(
            RELAY_CREW_ROLE_HACKER);
        m_assignments[crew_indices[1]].m_roles.push_back(
            RELAY_CREW_ROLE_ENGINEER);
        m_assignments[crew_indices[1]].m_roles.push_back(
            RELAY_CREW_ROLE_DEFENDER);
        m_assignments[crew_indices[1]].m_roles.push_back(
            RELAY_CREW_ROLE_GUNNER);
        return;
    }

    if (crew_indices.size() == 3)
    {
        m_assignments[crew_indices[0]].m_roles.push_back(
            RELAY_CREW_ROLE_NAVIGATOR);
        m_assignments[crew_indices[1]].m_roles.push_back(
            RELAY_CREW_ROLE_ENGINEER);
        m_assignments[crew_indices[2]].m_roles.push_back(
            RELAY_CREW_ROLE_DEFENDER);
        m_assignments[crew_indices[2]].m_roles.push_back(
            RELAY_CREW_ROLE_HACKER);
        m_assignments[crew_indices[2]].m_roles.push_back(
            RELAY_CREW_ROLE_GUNNER);
        return;
    }

    if (crew_indices.size() == 4)
    {
        m_assignments[crew_indices[0]].m_roles.push_back(
            RELAY_CREW_ROLE_NAVIGATOR);
        m_assignments[crew_indices[1]].m_roles.push_back(
            RELAY_CREW_ROLE_ENGINEER);
        m_assignments[crew_indices[2]].m_roles.push_back(
            RELAY_CREW_ROLE_DEFENDER);
        m_assignments[crew_indices[3]].m_roles.push_back(
            RELAY_CREW_ROLE_HACKER);
        m_assignments[crew_indices[3]].m_roles.push_back(
            RELAY_CREW_ROLE_GUNNER);
        return;
    }

    RelayCrewRole full_roles[] = {
        RELAY_CREW_ROLE_NAVIGATOR,
        RELAY_CREW_ROLE_ENGINEER,
        RELAY_CREW_ROLE_DEFENDER,
        RELAY_CREW_ROLE_HACKER,
        RELAY_CREW_ROLE_GUNNER
    };
    for (int i = 0; i < 5 && i < (int)crew_indices.size(); i++)
        m_assignments[crew_indices[i]].m_roles.push_back(full_roles[i]);
}

void CrewRoleManager::resetCooldowns()
{
    m_ready_at_ms.clear();
}

RelayCrewRole CrewRoleManager::roleFromString(const std::string& role)
{
    if (role == "navigator")
        return RELAY_CREW_ROLE_NAVIGATOR;
    if (role == "engineer")
        return RELAY_CREW_ROLE_ENGINEER;
    if (role == "defender")
        return RELAY_CREW_ROLE_DEFENDER;
    if (role == "hacker")
        return RELAY_CREW_ROLE_HACKER;
    if (role == "gunner")
        return RELAY_CREW_ROLE_GUNNER;
    return RELAY_CREW_ROLE_UNKNOWN;
}

const char* CrewRoleManager::roleToString(RelayCrewRole role)
{
    switch (role)
    {
    case RELAY_CREW_ROLE_NAVIGATOR: return "navigator";
    case RELAY_CREW_ROLE_ENGINEER: return "engineer";
    case RELAY_CREW_ROLE_DEFENDER: return "defender";
    case RELAY_CREW_ROLE_HACKER: return "hacker";
    case RELAY_CREW_ROLE_GUNNER: return "gunner";
    case RELAY_CREW_ROLE_UNKNOWN: break;
    }
    return "unknown";
}

bool CrewRoleManager::isCrewRole(RelayCrewRole role)
{
    return role == RELAY_CREW_ROLE_NAVIGATOR ||
           role == RELAY_CREW_ROLE_ENGINEER ||
           role == RELAY_CREW_ROLE_DEFENDER ||
           role == RELAY_CREW_ROLE_HACKER ||
           role == RELAY_CREW_ROLE_GUNNER;
}

const RelayCrewActionDefinition*
CrewRoleManager::getActionDefinition(const std::string& action_type)
{
    const std::vector<RelayCrewActionDefinition>& definitions =
        getDefinitions();
    for (unsigned int i = 0; i < definitions.size(); i++)
    {
        if (definitions[i].m_type == action_type)
            return &definitions[i];
    }
    return NULL;
}

std::vector<RelayCrewActionDefinition>
CrewRoleManager::getActionDefinitionsSnapshot()
{
    return getDefinitions();
}

std::string CrewRoleManager::cooldownKey(const std::string& player_id,
                                         const std::string& action_type) const
{
    (void)player_id;
    return action_type;
}

const RelayCrewRoleAssignment* CrewRoleManager::getAssignment(
    const std::string& player_id) const
{
    for (unsigned int i = 0; i < m_assignments.size(); i++)
    {
        if (m_assignments[i].m_player_id == player_id)
            return &m_assignments[i];
    }
    return NULL;
}

bool CrewRoleManager::playerHasRole(const std::string& player_id,
                                    RelayCrewRole role) const
{
    const RelayCrewRoleAssignment* assignment = getAssignment(player_id);
    if (!assignment || (assignment->m_is_pilot && m_assignments.size() != 1))
        return false;

    return std::find(assignment->m_roles.begin(), assignment->m_roles.end(),
                     role) != assignment->m_roles.end();
}

bool CrewRoleManager::validateAction(const RelayCrewActionRequest& request,
                                     unsigned int current_state_version,
                                     int now_ms,
                                     std::string* reject_reason,
                                     int* cooldown_remaining_ms,
                                     RelayCrewActionDefinition* definition) const
{
    (void)current_state_version;
    if (cooldown_remaining_ms)
        *cooldown_remaining_ms = 0;

    const RelayCrewActionDefinition* def =
        getActionDefinition(request.m_type);
    if (!def)
    {
        if (reject_reason)
            *reject_reason = "unknown_crew_action";
        return false;
    }

    if (definition)
        *definition = *def;

    RelayCrewRole role = request.m_role == RELAY_CREW_ROLE_UNKNOWN
        ? def->m_role : request.m_role;
    if (role != def->m_role)
    {
        if (reject_reason)
            *reject_reason = "role_action_mismatch";
        return false;
    }

    const RelayCrewRoleAssignment* assignment =
        getAssignment(request.m_player_id);
    if (!assignment)
    {
        if (reject_reason)
            *reject_reason = "unknown_crew_player";
        return false;
    }
    if (assignment->m_is_pilot && m_assignments.size() != 1)
    {
        if (reject_reason)
            *reject_reason = "active_pilot_cannot_use_crew_action";
        return false;
    }
    if (!playerHasRole(request.m_player_id, def->m_role))
    {
        if (reject_reason)
            *reject_reason = "role_not_assigned_to_player";
        return false;
    }

    const std::string key = cooldownKey(request.m_player_id, request.m_type);
    std::map<std::string, int>::const_iterator ready_at =
        m_ready_at_ms.find(key);
    if (ready_at != m_ready_at_ms.end() && ready_at->second > now_ms)
    {
        if (cooldown_remaining_ms)
            *cooldown_remaining_ms = ready_at->second - now_ms;
        if (reject_reason)
            *reject_reason = "cooldown";
        return false;
    }

    return true;
}

void CrewRoleManager::markActionAccepted(
    const RelayCrewActionRequest& request, int now_ms)
{
    const RelayCrewActionDefinition* def =
        getActionDefinition(request.m_type);
    if (!def)
        return;

    m_ready_at_ms[cooldownKey(request.m_player_id, request.m_type)] =
        now_ms + def->m_cooldown_ms;
}

int CrewRoleManager::getCooldownRemainingMs(
    const std::string& player_id, const std::string& action_type,
    int now_ms) const
{
    const std::string key = cooldownKey(player_id, action_type);
    std::map<std::string, int>::const_iterator ready_at =
        m_ready_at_ms.find(key);
    if (ready_at == m_ready_at_ms.end() || ready_at->second <= now_ms)
        return 0;
    return ready_at->second - now_ms;
}

std::vector<RelayCrewCooldownSnapshot>
CrewRoleManager::getCooldownsSnapshot(int now_ms) const
{
    std::vector<RelayCrewCooldownSnapshot> cooldowns;
    const std::vector<RelayCrewActionDefinition>& definitions =
        getDefinitions();

    for (unsigned int i = 0; i < m_assignments.size(); i++)
    {
        const RelayCrewRoleAssignment& assignment = m_assignments[i];
        if (assignment.m_is_pilot && m_assignments.size() != 1)
            continue;

        for (unsigned int r = 0; r < assignment.m_roles.size(); r++)
        {
            const RelayCrewRole role = assignment.m_roles[r];
            for (unsigned int d = 0; d < definitions.size(); d++)
            {
                if (definitions[d].m_role != role)
                    continue;

                RelayCrewCooldownSnapshot cooldown;
                cooldown.m_player_id = assignment.m_player_id;
                cooldown.m_role = role;
                cooldown.m_action_type = definitions[d].m_type;
                cooldown.m_remaining_ms = getCooldownRemainingMs(
                    assignment.m_player_id, definitions[d].m_type, now_ms);
                cooldowns.push_back(cooldown);
            }
        }
    }
    return cooldowns;
}

std::vector<RelayCrewRoleAssignment>
CrewRoleManager::getAssignmentsSnapshot() const
{
    return m_assignments;
}

#endif // ENABLE_IMMERSIVE_RELAY
