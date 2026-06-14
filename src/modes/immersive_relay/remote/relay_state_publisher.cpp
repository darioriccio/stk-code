//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/remote/relay_state_publisher.hpp"

#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

std::string RelayStatePublisher::jsonEscape(const std::string& input)
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

namespace
{
bool phonePassesFilter(const RelayCrewPhoneState& phone,
                       const std::string& player_filter,
                       const std::string& role_filter)
{
    if (!player_filter.empty() && phone.m_player_id != player_filter)
        return false;
    if (!role_filter.empty() &&
        CrewRoleManager::roleToString(phone.m_role) != role_filter)
        return false;
    return true;
}

void writeMultiplier(std::ostringstream& out, int multiplier_x100)
{
    const std::ios::fmtflags flags = out.flags();
    const std::streamsize precision = out.precision();
    out << std::fixed << std::setprecision(2)
        << ((double)multiplier_x100 / 100.0);
    out.flags(flags);
    out.precision(precision);
}

void writeActionHistory(std::ostringstream& out,
                        const std::vector<RelayCrewActionHistory>& history)
{
    out << "[";
    for (unsigned int i = 0; i < history.size(); i++)
    {
        const RelayCrewActionHistory& entry = history[i];
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"player_id\":\"" << RelayStatePublisher::jsonEscape(entry.m_player_id) << "\"";
        out << ",\"role\":\"" << CrewRoleManager::roleToString(entry.m_role) << "\"";
        out << ",\"type\":\"" << RelayStatePublisher::jsonEscape(entry.m_type) << "\"";
        out << ",\"action_id\":\"" << RelayStatePublisher::jsonEscape(entry.m_action_id) << "\"";
        out << ",\"idempotency_key\":\"" << RelayStatePublisher::jsonEscape(entry.m_idempotency_key) << "\"";
        out << ",\"result\":\"" << RelayStatePublisher::jsonEscape(entry.m_result) << "\"";
        out << ",\"reason\":\"" << RelayStatePublisher::jsonEscape(entry.m_reason) << "\"";
        out << ",\"effect\":\"" << RelayStatePublisher::jsonEscape(entry.m_effect_summary) << "\"";
        out << ",\"score_delta\":" << entry.m_score_delta;
        out << ",\"cooldown_ms\":" << entry.m_cooldown_ms;
        out << ",\"state_version\":" << entry.m_state_version;
        out << "}";
    }
    out << "]";
}

void writeRoleIntMap(std::ostringstream& out,
                     const std::map<RelayCrewRole, int>& values)
{
    out << "{";
    unsigned int emitted = 0;
    for (std::map<RelayCrewRole, int>::const_iterator i = values.begin();
         i != values.end(); i++)
    {
        if (emitted++ > 0)
            out << ",";
        out << "\"" << CrewRoleManager::roleToString(i->first) << "\":"
            << i->second;
    }
    out << "}";
}

void writeStringIntMap(std::ostringstream& out,
                       const std::map<std::string, int>& values)
{
    out << "{";
    unsigned int emitted = 0;
    for (std::map<std::string, int>::const_iterator i = values.begin();
         i != values.end(); i++)
    {
        if (emitted++ > 0)
            out << ",";
        out << "\""
            << RelayStatePublisher::jsonEscape(i->first) << "\":"
            << i->second;
    }
    out << "}";
}

void writeStringList(std::ostringstream& out,
                     const std::vector<std::string>& values)
{
    out << "[";
    for (unsigned int i = 0; i < values.size(); i++)
    {
        if (i > 0)
            out << ",";
        out << "\"" << RelayStatePublisher::jsonEscape(values[i]) << "\"";
    }
    out << "]";
}

void writeArcadeReport(std::ostringstream& out,
                       const RelaySessionReport& report)
{
    if (!report.m_available)
    {
        out << "null";
        return;
    }
    out << "{";
    out << "\"session_id\":\""
        << RelayStatePublisher::jsonEscape(report.m_session_id) << "\"";
    out << ",\"player_count\":" << report.m_player_count;
    out << ",\"track_id\":\""
        << RelayStatePublisher::jsonEscape(report.m_track_id) << "\"";
    out << ",\"total_laps\":" << report.m_total_laps;
    out << ",\"completed_laps\":" << report.m_completed_laps;
    out << ",\"final_result\":\""
        << RelayStatePublisher::jsonEscape(report.m_final_result) << "\"";
    out << ",\"rank\":\""
        << RelayStatePublisher::jsonEscape(report.m_rank) << "\"";
    out << ",\"score\":" << report.m_score;
    out << ",\"combo_count\":" << report.m_combo_count;
    out << ",\"encounters_started\":" << report.m_encounters_started;
    out << ",\"encounters_succeeded\":" << report.m_encounters_succeeded;
    out << ",\"encounters_failed\":" << report.m_encounters_failed;
    out << ",\"actions_by_role\":";
    writeRoleIntMap(out, report.m_actions_by_role);
    out << ",\"driver_turns_by_player\":";
    writeStringIntMap(out, report.m_driver_turns_by_player);
    out << ",\"duration_seconds\":" << report.m_duration_seconds;
    out << ",\"render_profile\":\""
        << RelayStatePublisher::jsonEscape(report.m_render_profile) << "\"";
    out << ",\"projection\":\""
        << RelayStatePublisher::jsonEscape(report.m_projection) << "\"";
    out << ",\"output\":\""
        << RelayStatePublisher::jsonEscape(report.m_output) << "\"";
    out << ",\"crew_mode_enabled\":"
        << (report.m_crew_mode_enabled ? "true" : "false");
    out << ",\"crew_actions_enabled\":"
        << (report.m_crew_actions_enabled ? "true" : "false");
    out << ",\"crew_actions_accepted\":"
        << report.m_crew_actions_accepted;
    out << "}";
}

void writeArcadeSession(std::ostringstream& out,
                        const RelaySessionSnapshot& snapshot)
{
    const RelayPilotRotationSnapshot& r = snapshot.m_rotation;
    const int active_driver = r.m_active_index >= 0 ? r.m_active_index + 1 : 0;
    const int next_driver = r.m_pending_index >= 0
        ? r.m_pending_index + 1
        : (!r.m_next_player_id.empty()
           ? ((r.m_active_index + 1) % std::max(1, snapshot.m_player_count)) + 1
           : 0);
    const int previous_driver =
        r.m_previous_index >= 0 ? r.m_previous_index + 1 : 0;
    const int handoff_countdown =
        (r.m_handoff_remaining_ms + 999) / 1000;
    int remaining_laps =
        std::max(0, snapshot.m_total_laps - snapshot.m_current_lap + 1);
    if (snapshot.m_lap_progress >= 1.0f ||
        snapshot.m_arcade_state == ARCADE_STATE_FINISH_SEQUENCE ||
        snapshot.m_arcade_state == ARCADE_STATE_RESULTS)
    {
        remaining_laps = 0;
    }

    out << "{";
    out << "\"session_id\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_session_id) << "\"";
    out << ",\"state\":\""
        << ArcadeSessionStateMachine::stateToString(
            snapshot.m_arcade_state) << "\"";
    out << ",\"legacy_state\":\""
        << RelaySessionController::stateToString(snapshot.m_state) << "\"";
    out << ",\"configured\":"
        << (snapshot.m_arcade_configured ? "true" : "false");
    out << ",\"player_count\":" << snapshot.m_player_count;
    out << ",\"total_laps\":" << snapshot.m_total_laps;
    out << ",\"current_lap\":" << snapshot.m_current_lap;
    out << ",\"remaining_laps\":" << remaining_laps;
    out << ",\"active_driver\":" << active_driver;
    out << ",\"active_driver_id\":\""
        << RelayStatePublisher::jsonEscape(r.m_active_player_id) << "\"";
    out << ",\"previous_driver\":";
    if (previous_driver > 0)
        out << previous_driver;
    else
        out << "null";
    out << ",\"previous_driver_id\":";
    if (!r.m_previous_player_id.empty())
        out << "\""
            << RelayStatePublisher::jsonEscape(r.m_previous_player_id)
            << "\"";
    else
        out << "null";
    out << ",\"next_driver\":";
    if (next_driver > 0)
        out << next_driver;
    else
        out << "null";
    out << ",\"next_driver_id\":\""
        << RelayStatePublisher::jsonEscape(r.m_next_player_id) << "\"";
    out << ",\"handoff_policy\":\""
        << RelaySessionController::handoffPolicyToString(
               snapshot.m_handoff_policy) << "\"";
    out << ",\"handoff_active\":"
        << (r.m_handoff_active ? "true" : "false");
    out << ",\"input_locked\":"
        << ((r.m_handoff_active || snapshot.m_paused ||
             snapshot.m_arcade_state == ARCADE_STATE_PAUSED)
            ? "true" : "false");
    out << ",\"handoff_countdown\":";
    if (r.m_handoff_active)
        out << handoff_countdown;
    else
        out << "null";
    out << ",\"waiting_for_operator_confirm\":"
        << (r.m_waiting_for_operator_confirm ? "true" : "false");
    out << ",\"countdown\":";
    if (snapshot.m_arcade_state == ARCADE_STATE_COUNTDOWN)
        out << ((snapshot.m_countdown_remaining_ms + 999) / 1000);
    else
        out << "null";
    out << ",\"pilot_turns_by_player\":";
    writeStringIntMap(out, snapshot.m_pilot_turns_by_player);
    out << ",\"can_start\":" << (snapshot.m_can_start ? "true" : "false");
    out << ",\"can_pause\":" << (snapshot.m_can_pause ? "true" : "false");
    out << ",\"can_resume\":" << (snapshot.m_can_resume ? "true" : "false");
    out << ",\"can_reset\":" << (snapshot.m_can_reset ? "true" : "false");
    out << ",\"can_abort\":" << (snapshot.m_can_abort ? "true" : "false");
    out << ",\"can_force_finish\":"
        << (snapshot.m_can_force_finish ? "true" : "false");
    out << ",\"last_error\":";
    if (snapshot.m_arcade_last_error.empty())
        out << "null";
    else
        out << "\"" << RelayStatePublisher::jsonEscape(
            snapshot.m_arcade_last_error) << "\"";
    out << ",\"last_error_recoverable\":"
        << (snapshot.m_arcade_last_error_recoverable ? "true" : "false");
    out << ",\"allowed_transitions\":[";
    const std::vector<ArcadeSessionState> transitions =
        ArcadeSessionStateMachine::allowedTransitions(
            snapshot.m_arcade_state);
    for (unsigned int i = 0; i < transitions.size(); i++)
    {
        if (i > 0)
            out << ",";
        out << "\"" << ArcadeSessionStateMachine::stateToString(
            transitions[i]) << "\"";
    }
    out << "]";
    out << ",\"operator_commands\":";
    writeStringList(out,
        ArcadeSessionStateMachine::allowedOperatorCommands(
            snapshot.m_arcade_state));
    out << ",\"config\":{";
    out << "\"player_count\":" << snapshot.m_player_count;
    out << ",\"track_id\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_track_id) << "\"";
    out << ",\"kart_id\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_kart_id) << "\"";
    out << ",\"difficulty\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_difficulty) << "\"";
    out << ",\"ai_karts\":" << snapshot.m_ai_karts;
    out << ",\"lap_policy\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_lap_policy) << "\"";
    out << ",\"handoff_policy\":\""
        << RelaySessionController::handoffPolicyToString(
               snapshot.m_handoff_policy) << "\"";
    out << ",\"crew_mode\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_crew_mode) << "\"";
    out << ",\"encounter_script\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_encounter_script)
        << "\"";
    out << ",\"render_profile\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_render_profile) << "\"";
    out << ",\"output\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_output) << "\"";
    out << ",\"projection\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_projection) << "\"";
    out << ",\"operator_mode\":\""
        << RelayStatePublisher::jsonEscape(snapshot.m_operator_mode)
        << "\"";
    out << ",\"allow_pause\":"
        << (snapshot.m_allow_pause ? "true" : "false");
    out << ",\"allow_force_finish\":"
        << (snapshot.m_allow_force_finish ? "true" : "false");
    out << ",\"allow_abort\":"
        << (snapshot.m_allow_abort ? "true" : "false");
    out << ",\"handoff_operator_confirm\":"
        << (snapshot.m_require_handoff_confirm ? "true" : "false");
    out << ",\"auto_confirm_handoff\":"
        << (snapshot.m_auto_confirm_handoff ? "true" : "false");
    out << "}";
    out << ",\"report\":";
    if (snapshot.m_arcade_state == ARCADE_STATE_RESULTS ||
        snapshot.m_arcade_state == ARCADE_STATE_FINISH_SEQUENCE)
    {
        writeArcadeReport(out, snapshot.m_report);
    }
    else
    {
        out << "null";
    }
    out << ",\"last_report\":";
    writeArcadeReport(out, snapshot.m_report);
    out << "}";
}
}

std::string RelayStatePublisher::stateJson(
    const RelaySessionSnapshot& snapshot)
{
    return stateJson(snapshot, "", "");
}

std::string RelayStatePublisher::stateJson(
    const RelaySessionSnapshot& snapshot,
    const std::string& player_filter,
    const std::string& role_filter)
{
    const RelayPilotRotationSnapshot& r = snapshot.m_rotation;
    std::ostringstream out;
    out << "{";
    out << "\"ok\":true";
    out << ",\"relay_enabled\":" << (snapshot.m_runtime_enabled ? "true" : "false");
    out << ",\"arcade_ui\":" << (snapshot.m_arcade_ui ? "true" : "false");
    out << ",\"race_attached\":" << (snapshot.m_race_attached ? "true" : "false");
    out << ",\"paused\":" << (snapshot.m_paused ? "true" : "false");
    out << ",\"state\":\"" << RelaySessionController::stateToString(snapshot.m_state) << "\"";
    out << ",\"arcade_state\":\""
        << ArcadeSessionStateMachine::stateToString(
            snapshot.m_arcade_state) << "\"";
    out << ",\"state_version\":" << snapshot.m_state_version;
    out << ",\"session_id\":\"" << jsonEscape(snapshot.m_session_id) << "\"";
    out << ",\"preset\":\"" << jsonEscape(snapshot.m_preset) << "\"";
    out << ",\"track_id\":\"" << jsonEscape(snapshot.m_track_id) << "\"";
    out << ",\"gameplay_seconds\":" << snapshot.m_gameplay_seconds;
    out << ",\"slot_seconds\":" << snapshot.m_slot_seconds;
    out << ",\"player_count\":" << snapshot.m_player_count;
    out << ",\"race_laps\":" << snapshot.m_race_laps;
    out << ",\"current_lap\":" << snapshot.m_current_lap;
    out << ",\"lap\":" << snapshot.m_current_lap;
    out << ",\"total_laps\":" << snapshot.m_total_laps;
    out << ",\"lap_progress\":" << snapshot.m_lap_progress;
    out << ",\"current_checkpoint\":" << snapshot.m_current_checkpoint;
    out << ",\"track_distance\":" << snapshot.m_track_distance;
    out << ",\"race_phase\":\"" << jsonEscape(snapshot.m_race_phase) << "\"";
    out << ",\"arcade_session\":";
    writeArcadeSession(out, snapshot);
    out << ",\"test_accelerated_progress\":"
        << (snapshot.m_test_accelerated_progress ? "true" : "false");
    out << ",\"elapsed_ms\":" << snapshot.m_elapsed_ms;
    out << ",\"remaining_ms\":" << snapshot.m_remaining_ms;
    out << ",\"active_pilot_index\":" << r.m_active_index;
    out << ",\"active_pilot_id\":\"" << jsonEscape(r.m_active_player_id) << "\"";
    out << ",\"active_pilot_name\":\"" << jsonEscape(r.m_active_display_name) << "\"";
    out << ",\"active_driver\":{";
    out << "\"index\":" << r.m_active_index;
    out << ",\"player_id\":\"" << jsonEscape(r.m_active_player_id) << "\"";
    out << ",\"display_name\":\"" << jsonEscape(r.m_active_display_name) << "\"";
    out << "}";
    out << ",\"next_pilot_id\":\"" << jsonEscape(r.m_next_player_id) << "\"";
    out << ",\"next_pilot_name\":\"" << jsonEscape(r.m_next_display_name) << "\"";
    out << ",\"handoff_policy\":\""
        << RelaySessionController::handoffPolicyToString(
               snapshot.m_handoff_policy) << "\"";
    out << ",\"handoff_active\":" << (r.m_handoff_active ? "true" : "false");
    out << ",\"handoff_warning_active\":"
        << (r.m_handoff_warning_active ? "true" : "false");
    out << ",\"handoff_warning_in_ms\":"
        << r.m_handoff_warning_remaining_ms;
    out << ",\"handoff_pending_safe_zone\":"
        << (r.m_handoff_pending_safe_zone ? "true" : "false");
    out << ",\"handoff_in_ms\":" << r.m_handoff_remaining_ms;
    out << ",\"handoff_reason\":\"" << jsonEscape(r.m_handoff_reason) << "\"";
    out << ",\"completed_handoffs\":" << r.m_completed_handoffs;
    out << ",\"crew_score\":" << snapshot.m_crew_score;
    out << ",\"crew_multiplier\":";
    writeMultiplier(out, snapshot.m_crew_multiplier_x100);
    out << ",\"crew_multiplier_x100\":" << snapshot.m_crew_multiplier_x100;
    out << ",\"engineer_boost_charge\":"
        << snapshot.m_engineer_boost_charge;
    out << ",\"boost_charge\":" << snapshot.m_engineer_boost_charge;
    out << ",\"engine_heat\":" << snapshot.m_engine_heat;
    out << ",\"crew_heat\":" << snapshot.m_engine_heat;
    out << ",\"armor\":" << snapshot.m_armor;
    out << ",\"crew_armor\":" << snapshot.m_armor;
    out << ",\"crew_actions_accepted\":"
        << snapshot.m_crew_actions_accepted;
    out << ",\"encounters_started\":" << snapshot.m_encounters_started;
    out << ",\"encounters_succeeded\":" << snapshot.m_encounters_succeeded;
    out << ",\"encounters_failed\":" << snapshot.m_encounters_failed;
    out << ",\"combo_count\":" << snapshot.m_combo_count;
    out << ",\"active_route_id\":\""
        << jsonEscape(snapshot.m_active_route_id) << "\"";
    out << ",\"last_crew_event\":\""
        << jsonEscape(snapshot.m_last_crew_event) << "\"";
    out << ",\"last_crew_effect\":\""
        << jsonEscape(snapshot.m_last_crew_effect) << "\"";
    out << ",\"adaptive_difficulty_enabled\":"
        << (snapshot.m_adaptive_difficulty.m_enabled ? "true" : "false");
    out << ",\"adaptive_difficulty_profile\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_profile) << "\"";
    out << ",\"adaptive_difficulty_intensity\":"
        << snapshot.m_adaptive_difficulty.m_intensity;
    out << ",\"adaptive_pressure\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_pressure) << "\"";
    out << ",\"human_gap_ms\":"
        << snapshot.m_adaptive_difficulty.m_human_gap_ms;
    out << ",\"human_position\":"
        << snapshot.m_adaptive_difficulty.m_human_position;
    out << ",\"assist_level\":"
        << snapshot.m_adaptive_difficulty.m_assist_level;
    out << ",\"ai_pace_level\":"
        << snapshot.m_adaptive_difficulty.m_ai_pace_level;
    out << ",\"leaderboard_bucket\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_leaderboard_bucket)
        << "\"";
    out << ",\"adaptive_difficulty\":{";
    out << "\"enabled\":"
        << (snapshot.m_adaptive_difficulty.m_enabled ? "true" : "false");
    out << ",\"profile\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_profile) << "\"";
    out << ",\"visibility\":\""
        << jsonEscape(
            snapshot.m_adaptive_difficulty_config.m_visibility) << "\"";
    out << ",\"min_intensity\":"
        << snapshot.m_adaptive_difficulty_config.m_min_intensity;
    out << ",\"max_intensity\":"
        << snapshot.m_adaptive_difficulty_config.m_max_intensity;
    out << ",\"intensity\":"
        << snapshot.m_adaptive_difficulty.m_intensity;
    out << ",\"pressure\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_pressure) << "\"";
    out << ",\"human_gap_ms\":"
        << snapshot.m_adaptive_difficulty.m_human_gap_ms;
    out << ",\"human_position\":"
        << snapshot.m_adaptive_difficulty.m_human_position;
    out << ",\"assist_level\":"
        << snapshot.m_adaptive_difficulty.m_assist_level;
    out << ",\"ai_pace_level\":"
        << snapshot.m_adaptive_difficulty.m_ai_pace_level;
    out << ",\"final_lap_lock\":"
        << (snapshot.m_adaptive_difficulty.m_final_lap_lock
            ? "true" : "false");
    out << ",\"handoff_grace\":"
        << (snapshot.m_adaptive_difficulty.m_handoff_grace
            ? "true" : "false");
    out << ",\"leaderboard_bucket\":\""
        << jsonEscape(snapshot.m_adaptive_difficulty.m_leaderboard_bucket)
        << "\"";
    out << "}";
    out << ",\"crew_assignments\":[";
    for (unsigned int i = 0; i < snapshot.m_crew_assignments.size(); i++)
    {
        const RelayCrewRoleAssignment& a = snapshot.m_crew_assignments[i];
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"player_id\":\"" << jsonEscape(a.m_player_id) << "\"";
        out << ",\"display_name\":\"" << jsonEscape(a.m_display_name) << "\"";
        out << ",\"is_pilot\":" << (a.m_is_pilot ? "true" : "false");
        out << ",\"combined_roles\":"
            << (a.m_roles.size() > 1 ? "true" : "false");
        out << ",\"roles\":[";
        for (unsigned int r_i = 0; r_i < a.m_roles.size(); r_i++)
        {
            if (r_i > 0)
                out << ",";
            out << "\"" << CrewRoleManager::roleToString(a.m_roles[r_i])
                << "\"";
        }
        out << "]}";
    }
    out << "]";
    out << ",\"pilot_turns_by_player\":{";
    unsigned int emitted_turns = 0;
    for (std::map<std::string, int>::const_iterator i =
             snapshot.m_pilot_turns_by_player.begin();
         i != snapshot.m_pilot_turns_by_player.end(); i++)
    {
        if (emitted_turns++ > 0)
            out << ",";
        out << "\"" << jsonEscape(i->first) << "\":" << i->second;
    }
    out << "}";
    out << ",\"actions_accepted_by_role\":{";
    unsigned int emitted_role_actions = 0;
    for (std::map<RelayCrewRole, int>::const_iterator i =
             snapshot.m_actions_accepted_by_role.begin();
         i != snapshot.m_actions_accepted_by_role.end(); i++)
    {
        if (emitted_role_actions++ > 0)
            out << ",";
        out << "\"" << CrewRoleManager::roleToString(i->first) << "\":"
            << i->second;
    }
    out << "}";
    out << ",\"score_by_role\":{";
    unsigned int emitted_role_scores = 0;
    for (std::map<RelayCrewRole, int>::const_iterator i =
             snapshot.m_score_by_role.begin();
         i != snapshot.m_score_by_role.end(); i++)
    {
        if (emitted_role_scores++ > 0)
            out << ",";
        out << "\"" << CrewRoleManager::roleToString(i->first) << "\":"
            << i->second;
    }
    out << "}";
    out << ",\"role_assignments\":[";
    for (unsigned int i = 0; i < snapshot.m_crew_assignments.size(); i++)
    {
        const RelayCrewRoleAssignment& a = snapshot.m_crew_assignments[i];
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"player_id\":\"" << jsonEscape(a.m_player_id) << "\"";
        out << ",\"display_name\":\"" << jsonEscape(a.m_display_name) << "\"";
        out << ",\"is_pilot\":" << (a.m_is_pilot ? "true" : "false");
        out << ",\"combined_roles\":"
            << (a.m_roles.size() > 1 ? "true" : "false");
        out << ",\"roles\":[";
        for (unsigned int r_i = 0; r_i < a.m_roles.size(); r_i++)
        {
            if (r_i > 0)
                out << ",";
            out << "\"" << CrewRoleManager::roleToString(a.m_roles[r_i])
                << "\"";
        }
        out << "]}";
    }
    out << "]";
    out << ",\"track_effects\":[";
    for (unsigned int i = 0; i < snapshot.m_track_effects.size(); i++)
    {
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"key\":\""
            << jsonEscape(snapshot.m_track_effects[i].m_key) << "\"";
        out << ",\"value\":\""
            << jsonEscape(snapshot.m_track_effects[i].m_value) << "\"";
        out << "}";
    }
    out << "]";
    out << ",\"cooldowns\":[";
    for (unsigned int i = 0; i < snapshot.m_cooldowns.size(); i++)
    {
        const RelayCrewCooldownSnapshot& cooldown = snapshot.m_cooldowns[i];
        if (i > 0)
            out << ",";
        out << "{";
        out << "\"player_id\":\"" << jsonEscape(cooldown.m_player_id) << "\"";
        out << ",\"role\":\""
            << CrewRoleManager::roleToString(cooldown.m_role) << "\"";
        out << ",\"action_type\":\""
            << jsonEscape(cooldown.m_action_type) << "\"";
        out << ",\"remaining_ms\":" << cooldown.m_remaining_ms;
        out << ",\"ready\":"
            << (cooldown.m_remaining_ms <= 0 ? "true" : "false");
        out << "}";
    }
    out << "]";
    out << ",\"last_accepted_actions\":";
    writeActionHistory(out, snapshot.m_last_accepted_actions);
    out << ",\"last_rejected_actions\":";
    writeActionHistory(out, snapshot.m_last_rejected_actions);
    out << ",\"active_encounter\":{";
    out << "\"active\":" << (snapshot.m_encounter.m_active ? "true" : "false");
    out << ",\"id\":\"" << jsonEscape(snapshot.m_encounter.m_id) << "\"";
    out << ",\"title\":\""
        << jsonEscape(snapshot.m_encounter.m_title) << "\"";
    out << ",\"stage\":\""
        << jsonEscape(snapshot.m_encounter.m_stage) << "\"";
    out << ",\"state\":\""
        << jsonEscape(snapshot.m_encounter.m_state) << "\"";
    out << ",\"trigger_type\":\""
        << jsonEscape(snapshot.m_encounter.m_trigger_type) << "\"";
    out << ",\"combo_tag\":\""
        << jsonEscape(snapshot.m_encounter.m_combo_tag) << "\"";
    out << ",\"dome_callout\":\""
        << jsonEscape(snapshot.m_encounter.m_dome_callout) << "\"";
    out << ",\"remaining_ms\":" << snapshot.m_encounter.m_remaining_ms;
    out << ",\"required_actions\":"
        << snapshot.m_encounter.m_required_actions;
    out << ",\"completed_actions\":"
        << snapshot.m_encounter.m_completed_actions;
    out << ",\"roles\":[";
    for (unsigned int i = 0; i < snapshot.m_encounter.m_roles.size(); i++)
    {
        if (i > 0)
            out << ",";
        out << "\""
            << CrewRoleManager::roleToString(
                snapshot.m_encounter.m_roles[i]) << "\"";
    }
    out << "]}";
    out << ",\"immersive_render\":"
        << ImmersiveRelay::ImmersiveRenderPipeline::get()
               ->getStats().toJson();
    out << ",\"phone_states\":[";
    unsigned int emitted_phone_states = 0;
    for (unsigned int i = 0; i < snapshot.m_phone_states.size(); i++)
    {
        const RelayCrewPhoneState& phone = snapshot.m_phone_states[i];
        if (!phonePassesFilter(phone, player_filter, role_filter))
            continue;
        if (emitted_phone_states > 0)
            out << ",";
        emitted_phone_states++;
        out << "{";
        out << "\"player_id\":\"" << jsonEscape(phone.m_player_id) << "\"";
        out << ",\"role\":\""
            << CrewRoleManager::roleToString(phone.m_role) << "\"";
        out << ",\"lap\":" << phone.m_lap;
        out << ",\"total_laps\":" << phone.m_total_laps;
        out << ",\"active_driver\":{";
        out << "\"index\":" << phone.m_active_driver_index;
        out << ",\"player_id\":\""
            << jsonEscape(phone.m_active_driver_id) << "\"";
        out << "}";
        out << ",\"encounter_id\":\""
            << jsonEscape(phone.m_encounter_id) << "\"";
        out << ",\"next_encounter_id\":\""
            << jsonEscape(phone.m_next_encounter_id) << "\"";
        out << ",\"title\":\"" << jsonEscape(phone.m_title) << "\"";
        out << ",\"message\":\"" << jsonEscape(phone.m_message) << "\"";
        out << ",\"resource_text\":\""
            << jsonEscape(phone.m_resource_text) << "\"";
        out << ",\"remaining_ms\":" << phone.m_remaining_ms;
        out << ",\"primary\":" << (phone.m_primary ? "true" : "false");
        out << ",\"actions\":[";
        for (unsigned int a_i = 0; a_i < phone.m_actions.size(); a_i++)
        {
            const RelayCrewPhoneAction& action = phone.m_actions[a_i];
            if (a_i > 0)
                out << ",";
            out << "{";
            out << "\"type\":\"" << jsonEscape(action.m_type) << "\"";
            out << ",\"label\":\"" << jsonEscape(action.m_label) << "\"";
            out << ",\"role\":\""
                << CrewRoleManager::roleToString(action.m_role) << "\"";
            out << ",\"target_key\":\""
                << jsonEscape(action.m_target_key) << "\"";
            out << ",\"target_id\":\""
                << jsonEscape(action.m_target_id) << "\"";
            out << ",\"mode_key\":\""
                << jsonEscape(action.m_mode_key) << "\"";
            out << ",\"mode\":\"" << jsonEscape(action.m_mode) << "\"";
            out << ",\"amount\":" << action.m_amount;
            out << ",\"available\":"
                << (action.m_available ? "true" : "false");
            out << ",\"unavailable_reason\":\""
                << jsonEscape(action.m_unavailable_reason) << "\"";
            out << ",\"cooldown_ms\":" << action.m_cooldown_ms;
            out << "}";
        }
        out << "]}";
    }
    out << "]";
    out << "}";
    return out.str();
}

std::string RelayStatePublisher::healthJson(
    const RelaySessionSnapshot& snapshot, bool api_active, int api_port)
{
    std::ostringstream out;
    out << "{";
    out << "\"ok\":true";
    out << ",\"relay_enabled\":" << (snapshot.m_runtime_enabled ? "true" : "false");
    out << ",\"arcade_ui\":" << (snapshot.m_arcade_ui ? "true" : "false");
    out << ",\"api_active\":" << (api_active ? "true" : "false");
    out << ",\"api_port\":" << api_port;
    out << ",\"state\":\"" << RelaySessionController::stateToString(snapshot.m_state) << "\"";
    out << ",\"state_version\":" << snapshot.m_state_version;
    out << "}";
    return out.str();
}

std::string RelayStatePublisher::capabilitiesJson(
    const RelaySessionSnapshot& snapshot, bool api_active, int api_port)
{
    std::ostringstream out;
    out << "{";
    out << "\"ok\":true";
    out << ",\"api_version\":1";
    out << ",\"service\":\"immersive_supertuxkart\"";
    out << ",\"status\":\"" << (api_active ? "ready" : "starting") << "\"";
    out << ",\"relay_enabled\":"
        << (snapshot.m_runtime_enabled ? "true" : "false");
    out << ",\"base_path\":\"/immersive/v1\"";
    out << ",\"default_base_url\":\"http://127.0.0.1:8890/immersive/v1\"";
    out << ",\"api_port\":" << api_port;
    out << ",\"token_required\":true";
    out << ",\"auth_headers\":[\"X-STK-Relay-Token\",\"Authorization: Bearer\"]";
    out << ",\"bind_policy\":\"trusted_localhost_default\"";
    out << ",\"cors_policy\":\"localhost_only\"";
    out << ",\"phone_routing\":\"phones_talk_to_tholustos_not_stk\"";
    out << ",\"video_flow\":\"stk_to_tholustos_syphon_only\"";
    out << ",\"supported_outputs\":[\"none\",\"window\"";
#ifdef ENABLE_SYPHON_OUTPUT
    out << ",\"syphon\"";
#endif
    out << "]";
    out << ",\"output_adapters\":{";
    out << "\"none\":{\"status\":\"production\"}";
    out << ",\"window\":{\"status\":\"production\"}";
    out << ",\"syphon\":{\"status\":\""
#ifdef ENABLE_SYPHON_OUTPUT
        << "available"
#else
        << "unavailable_in_this_build"
#endif
        << "\",\"server_name\":\"SuperTuxKart Immersive Relay Domemaster\"}";
    out << "}";
    out << ",\"supported_projections\":[";
    out << "{\"type\":\"fisheye_domemaster\",\"status\":\"production\","
           "\"aspect\":\"1:1\",\"coverage\":\"front-facing 180\"}";
    out << ",{\"type\":\"equirectangular\",\"status\":\"stub\"}";
    out << ",{\"type\":\"cylindrical\",\"status\":\"stub\"}";
    out << ",{\"type\":\"cube_faces\",\"status\":\"stub\"}";
    out << ",{\"type\":\"wall_atlas\",\"status\":\"stub\"}";
    out << "]";
    out << ",\"default_syphon_server_name\":"
           "\"SuperTuxKart Immersive Relay Domemaster\"";
    out << ",\"default_render_profile\":\"fulldome_safe\"";
    out << ",\"render_profiles\":{";
    out << "\"debug\":{\"projection\":\"fisheye_domemaster\","
           "\"domemaster_size\":1024,\"cubemap_size\":512,"
           "\"status\":\"production\"}";
    out << ",\"fulldome_safe\":{\"projection\":\"fisheye_domemaster\","
           "\"domemaster_size\":1536,\"cubemap_size\":768,"
           "\"status\":\"venue_default\"}";
    out << ",\"target\":{\"projection\":\"fisheye_domemaster\","
           "\"domemaster_size\":2048,\"cubemap_size\":1024,"
           "\"status\":\"pending_mac_mini_m1_benchmark\"}";
    out << "}";
    out << ",\"camera_presets\":{";
    out << "\"safe\":{\"mode\":\"stabilized_chase\","
           "\"pitch_deg\":35,\"forward_offset\":0.25,"
           "\"vertical_offset\":0.40,\"smoothing_factor\":0.16,"
           "\"horizon_stabilization\":true,\"roll_limit_deg\":12}";
    out << "}";
    out << ",\"player_count\":{\"min\":2,\"max\":6}";
    out << ",\"lap_policy\":\"player_count_based\"";
    out << ",\"handoff_policy\":\"lap_based_handoff\"";
    out << ",\"crew_roles\":[\"navigator\",\"engineer\",\"defender\","
           "\"hacker\",\"gunner\"]";
    out << ",\"crew_error_codes\":[\"wrong_role\",\"wrong_window\","
           "\"cooldown\",\"already_used\",\"encounter_expired\","
           "\"stale_state\",\"invalid_token\",\"invalid_payload\"]";
    out << ",\"endpoints\":[";
    out << "\"GET /state\"";
    out << ",\"GET /operator/state\"";
    out << ",\"GET /capabilities\"";
    out << ",\"POST /operator/configure\"";
    out << ",\"POST /operator/start\"";
    out << ",\"POST /operator/countdown\"";
    out << ",\"POST /operator/pause\"";
    out << ",\"POST /operator/resume\"";
    out << ",\"POST /operator/abort\"";
    out << ",\"POST /operator/reset\"";
    out << ",\"POST /operator/force-finish\"";
    out << ",\"POST /operator/handoff/confirm\"";
    out << ",\"POST /operator/render-preset\"";
    out << ",\"POST /render/config\"";
    out << ",\"POST /crew/action\"";
    out << "]";
    out << "}";
    return out.str();
}

std::string RelayStatePublisher::okJson(
    const RelaySessionSnapshot& snapshot, const std::string& event_name)
{
    std::ostringstream out;
    out << "{";
    out << "\"ok\":true";
    out << ",\"event\":\"" << jsonEscape(event_name) << "\"";
    out << ",\"state_version\":" << snapshot.m_state_version;
    out << ",\"state\":\"" << RelaySessionController::stateToString(snapshot.m_state) << "\"";
    out << "}";
    return out.str();
}

std::string RelayStatePublisher::crewActionJson(
    const RelaySessionSnapshot& snapshot, const RelayCrewActionAck& ack)
{
    std::ostringstream out;
    out << "{";
    out << "\"ok\":true";
    out << ",\"event\":\"" << jsonEscape(ack.m_event_name) << "\"";
    out << ",\"accepted\":" << (ack.m_accepted ? "true" : "false");
    out << ",\"duplicate\":" << (ack.m_duplicate ? "true" : "false");
    out << ",\"idempotency_key\":\""
        << jsonEscape(ack.m_idempotency_key) << "\"";
    out << ",\"state_version\":" << snapshot.m_state_version;
    out << ",\"state\":\""
        << RelaySessionController::stateToString(snapshot.m_state) << "\"";
    out << ",\"effect\":\"" << jsonEscape(ack.m_effect_summary) << "\"";
    out << ",\"score_delta\":" << ack.m_score_delta;
    out << ",\"crew_score\":" << ack.m_total_score;
    out << ",\"cooldown_ms\":" << ack.m_cooldown_ms;
    out << ",\"crew_multiplier_x100\":"
        << snapshot.m_crew_multiplier_x100;
    out << "}";
    return out.str();
}

std::string RelayStatePublisher::errorJson(const std::string& error,
                                           unsigned int state_version)
{
    return errorJson(error, state_version, 0);
}

std::string RelayStatePublisher::errorJson(const std::string& error,
                                           unsigned int state_version,
                                           int retry_after_ms)
{
    std::ostringstream out;
    out << "{";
    out << "\"ok\":false";
    out << ",\"error\":\"" << jsonEscape(error) << "\"";
    out << ",\"state_version\":" << state_version;
    if (retry_after_ms > 0)
        out << ",\"retry_after_ms\":" << retry_after_ms;
    out << "}";
    return out.str();
}

#endif // ENABLE_IMMERSIVE_RELAY
