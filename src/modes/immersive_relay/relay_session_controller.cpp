//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "modes/immersive_relay/scintilla_mission_result_json.hpp"
#include "modes/immersive_relay/scintilla_crew_result_json.hpp"
#include "modes/immersive_relay/scintilla_crew_report_file.hpp"

#include "race/race_manager.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

RelaySessionController* RelaySessionController::m_instance = NULL;

namespace
{
bool isContinuousCrewAction(const std::string& type)
{
    static const char* types[] = {"nav_select_route", "nav_call_pace_note",
        "engineer_charge_boost", "engineer_release_boost", "engineer_dump_heat",
        "defender_raise_shield", "defender_cleanse_status", "hacker_open_gate",
        "hacker_disable_trap", "gunner_fire_utility", "gunner_emp_zone", "gunner_drop_decoy"};
    return std::find(types, types + 12, type) != types + 12;
}

std::string crewActionLabel(const std::string& type)
{
    static const std::map<std::string, std::string> labels = {
        {"nav_select_route", "Segnala la linea"}, {"nav_call_pace_note", "Dai il ritmo"},
        {"nav_mark_hazard", "Segnala pericolo"}, {"nav_reveal_shortcut", "Segnala bonus"},
        {"nav_request_role", "Richiedi supporto"},
        {"engineer_charge_boost", "Carica turbo"}, {"engineer_release_boost", "Usa turbo"},
        {"engineer_dump_heat", "Raffredda motore"}, {"engineer_stabilize_grip", "Stabilizza"},
        {"engineer_rebalance_power", "Bilancia potenza"},
        {"defender_raise_shield", "Attiva scudo"}, {"defender_cleanse_status", "Ripulisci kart"},
        {"defender_absorb_hit", "Assorbi colpo"}, {"defender_repair_armor_small", "Ripara"},
        {"defender_emergency_bubble", "Scudo emergenza"},
        {"hacker_open_gate", "Sincronizza impulso"}, {"hacker_disable_trap", "Neutralizza disturbo"},
        {"hacker_close_hazard_barrier", "Impulso protettivo"}, {"hacker_rotate_ramp", "Impulso potenza"},
        {"hacker_arm_shortcut", "Attiva bonus squadra"},
        {"gunner_lock_target", "Aggancia avversario"}, {"gunner_fire_utility", "Rallenta avversario"},
        {"gunner_emp_zone", "Impulso collettivo"}, {"gunner_drop_decoy", "Diversivo"},
        {"gunner_tag_rival", "Segnala avversario"}
    };
    const auto found = labels.find(type);
    return found == labels.end() ? type : found->second;
}

std::string roleTitle(RelayCrewRole role)
{
    std::string title = CrewRoleManager::roleToString(role);
    if (!title.empty())
        title[0] = (char)std::toupper(title[0]);
    return title + " console";
}

RelayCrewPhoneAction makeDefaultPhoneAction(
    const RelayCrewActionDefinition& definition)
{
    RelayCrewPhoneAction action;
    action.m_type = definition.m_type;
    action.m_label = definition.m_type;
    action.m_role = definition.m_role;

    if (definition.m_type == "nav_select_route")
    {
        action.m_target_key = "route_id";
        action.m_target_id = "race_line";
    }
    else if (definition.m_type == "nav_mark_hazard")
    {
        action.m_target_key = "hazard_id";
        action.m_target_id = "debug_hazard";
    }
    else if (definition.m_type == "nav_call_pace_note")
    {
        action.m_target_key = "note_id";
        action.m_target_id = "race_rhythm";
    }
    else if (definition.m_type == "nav_reveal_shortcut")
    {
        action.m_target_key = "shortcut_id";
        action.m_target_id = "debug_shortcut";
    }
    else if (definition.m_type == "nav_request_role")
    {
        action.m_target_key = "role_id";
        action.m_target_id = "engineer";
    }
    else if (definition.m_type == "engineer_charge_boost")
    {
        action.m_amount = 25;
    }
    else if (definition.m_type == "engineer_release_boost")
    {
        action.m_mode_key = "mode";
        action.m_mode = "straight";
    }
    else if (definition.m_type == "engineer_stabilize_grip")
    {
        action.m_mode_key = "mode";
        action.m_mode = "grip";
    }
    else if (definition.m_type == "engineer_rebalance_power")
    {
        action.m_mode_key = "target";
        action.m_mode = "balanced";
    }
    else if (definition.m_type == "defender_raise_shield")
    {
        action.m_target_key = "direction";
        action.m_target_id = "front";
    }
    else if (definition.m_type == "defender_cleanse_status")
    {
        action.m_target_key = "status_id";
        action.m_target_id = "current";
    }
    else if (definition.m_type == "defender_absorb_hit")
    {
        action.m_target_key = "threat_id";
        action.m_target_id = "debug_threat";
    }
    else if (definition.m_type == "hacker_open_gate")
    {
        action.m_target_key = "gate_id";
        action.m_target_id = "sync_impulse";
    }
    else if (definition.m_type == "hacker_close_hazard_barrier")
    {
        action.m_target_key = "barrier_id";
        action.m_target_id = "debug_barrier";
    }
    else if (definition.m_type == "hacker_rotate_ramp")
    {
        action.m_target_key = "ramp_id";
        action.m_target_id = "debug_ramp";
        action.m_mode_key = "angle_state";
        action.m_mode = "attack";
    }
    else if (definition.m_type == "hacker_arm_shortcut")
    {
        action.m_target_key = "shortcut_id";
        action.m_target_id = "debug_shortcut";
    }
    else if (definition.m_type == "hacker_disable_trap")
    {
        action.m_target_key = "trap_id";
        action.m_target_id = "interference";
    }
    else if (definition.m_type == "gunner_lock_target" ||
             definition.m_type == "gunner_fire_utility")
    {
        action.m_target_key = "target_id";
        action.m_target_id = "nearest";
    }
    else if (definition.m_type == "gunner_emp_zone")
    {
        action.m_target_key = "zone_id";
        action.m_target_id = "nearby";
    }
    else if (definition.m_type == "gunner_tag_rival")
    {
        action.m_target_key = "rival_id";
        action.m_target_id = "nearest";
    }

    return action;
}

bool phoneStateMatches(const RelayCrewPhoneState& state,
                       const std::string& player_id,
                       RelayCrewRole role)
{
    return state.m_player_id == player_id && state.m_role == role;
}

bool phoneStateHasAction(const RelayCrewPhoneState& state,
                         const std::string& action_type)
{
    for (unsigned int i = 0; i < state.m_actions.size(); i++)
    {
        if (state.m_actions[i].m_type == action_type)
            return true;
    }
    return false;
}

std::string makeArcadeSessionId()
{
    const long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    return "arcade-" + std::to_string(ms);
}

std::string escapeReportJson(const std::string& value)
{
    std::ostringstream out;
    for (std::string::const_iterator i = value.begin(); i != value.end(); ++i)
    {
        switch (*i)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << *i; break;
        }
    }
    return out.str();
}

void writeRoleReportMap(std::ostream& file,
                        const std::map<RelayCrewRole, int>& values)
{
    file << "{";
    bool first = true;
    for (std::map<RelayCrewRole, int>::const_iterator i = values.begin();
         i != values.end(); ++i)
    {
        if (!first)
            file << ",";
        first = false;
        file << "\"" << CrewRoleManager::roleToString(i->first)
             << "\":" << i->second;
    }
    file << "}";
}

void writeStringIntReportMap(std::ostream& file,
                             const std::map<std::string, int>& values)
{
    file << "{";
    bool first = true;
    for (std::map<std::string, int>::const_iterator i = values.begin();
         i != values.end(); ++i)
    {
        if (!first)
            file << ",";
        first = false;
        file << "\"" << escapeReportJson(i->first)
             << "\":" << i->second;
    }
    file << "}";
}
}

RelaySessionConfig::RelaySessionConfig()
{
    m_session_id = "relay-local";
    m_preset = "standard_arcade";
    m_track_id = "";
    m_kart_id = "tux";
    m_difficulty = "easy";
    m_ai_karts = 3;
    m_lap_policy = "player_count_based";
    m_crew_mode = "enabled";
    m_encounter_script = "snowtuxpeak_relay_encounters";
    m_render_profile = "fulldome_safe";
    m_output = "syphon";
    m_projection = "fisheye_domemaster";
    m_operator_mode = "arcade";
    m_allow_pause = true;
    m_allow_force_finish = true;
    m_allow_abort = true;
    m_require_handoff_confirm = true;
    m_auto_confirm_handoff = false;
    m_music_enabled = true;
    m_sfx_enabled = true;
    m_music_volume = 0.7f;
    m_sfx_volume = 0.8f;
    m_briefing_seconds = 6;
    m_countdown_seconds = 3;
    m_player_count = 1;
    m_arcade_ui = false;
    m_test_accelerated_progress = false;
    m_gameplay_seconds = 420;
    m_slot_seconds = 720;
    m_handoff_policy = RELAY_HANDOFF_LAP_BASED;
    m_handoff_seconds = 10.0f;
    m_min_handoff_seconds = 6.0f;
    m_handoff_timeout_seconds = 12.0f;
    m_handoff_warning_seconds = 10.0f;
    m_segment_seconds = 90.0f;
    m_final_sprint_no_handoff_seconds = 30.0f;
}

RelaySessionReport::RelaySessionReport()
{
    m_available = false;
    m_player_count = 0;
    m_total_laps = 0;
    m_completed_laps = 0;
    m_final_result = "";
    m_rank = "";
    m_score = 0;
    m_combo_count = 0;
    m_encounters_started = 0;
    m_encounters_succeeded = 0;
    m_encounters_failed = 0;
    m_duration_seconds = 0;
    m_crew_mode_enabled = false;
    m_crew_actions_enabled = false;
    m_crew_actions_accepted = 0;
}

RelayLocalApiOptions::RelayLocalApiOptions()
{
    m_enabled = false;
    m_bind_host = "127.0.0.1";
    m_port = 8890;
    m_token = "immersive-dev-token";
}

RelaySessionController::RelaySessionController()
{
    m_runtime_enabled = false;
    m_operator_controlled = false;
    m_resume_countdown = false;
    m_state_before_pause = ARCADE_STATE_RUNNING;
    m_race_attached = false;
    m_world_elapsed_ms = 0;
    m_paused = false;
    m_presentation_ready = true;
    m_scintilla_start_permitted = true;
    m_crew_required = false;
    m_crew_start_permitted = true;
    m_crew_handoff_ready_required = false;
    m_crew_pending_administrative_handoff = false;
    m_crew_observed_completed_handoffs = 0;
    m_crew_administrative_handoffs = 0;
    m_mission_required = false;
    m_mission_presentation_held = false;
    m_spatial_world_held = false;
    m_spatial_resume_revision = 0;
    m_spatial_resume_at_us = 0;
    m_state = RELAY_STATE_ATTRACT;
    m_state_before_handoff = RELAY_STATE_RUNNING;
    m_state_version = 1;
    m_active_config = m_default_config;
    m_arcade_configured = false;
    m_countdown_remaining_ms = 0;
    m_countdown_started_at = 0.0f;
    m_session_started_at = 0.0f;
    m_session_finished_at = 0.0f;
    m_last_race_time = 0.0f;
    m_rotation_time = 0.0f;
    m_last_safe_for_handoff = true;
    resetCrewStateLocked();
}

RelaySessionController::~RelaySessionController()
{
}

RelaySessionController* RelaySessionController::get()
{
    if (!m_instance)
        m_instance = new RelaySessionController();
    return m_instance;
}

void RelaySessionController::destroy()
{
    delete m_instance;
    m_instance = NULL;
}

bool RelaySessionController::crewBlocksGameplayLocked() const
{
    // The physical transfer/countdown must be able to create the next driver
    // before that driver's new logical bindings can be verified. Physics is
    // independently held by m_rotation throughout this stage A exception.
    return m_crew_required && !m_crew_start_permitted &&
        !m_rotation.isHandoffActive();
}

bool RelaySessionController::missionBlocksGameplayLocked() const
{
    return (m_mission_required && (m_mission_presentation_held || m_mission_result.valid)) ||
        crewBlocksGameplayLocked();
}

void RelaySessionController::resetCrewAdmissionLocked()
{
    if(m_crew_required)m_crew_start_permitted=false;
    m_crew_handoff_ready_required=false;
    m_crew_pending_administrative_handoff=false;
    m_crew_observed_completed_handoffs=0;
    m_crew_administrative_handoffs=0;
}

bool RelaySessionController::isWorldHeldLocked() const
{
    return m_runtime_enabled &&
        (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
         m_crew_handoff_ready_required || m_paused || m_rotation.isHandoffActive() ||
         (m_operator_controlled && !isSessionPlayableStateLocked()));
}

void RelaySessionController::bumpVersionLocked()
{
    const bool held = isWorldHeldLocked();
    if (held != m_spatial_world_held)
    {
        m_spatial_world_held = held;
        if (!held)
        {
            ++m_spatial_resume_revision;
            if (m_spatial_resume_revision == 0) m_spatial_resume_revision = 1;
            m_spatial_resume_at_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }
    m_state_version++;
    if (m_state_version == 0)
        m_state_version = 1;
}

bool RelaySessionController::transitionArcadeLocked(
    ArcadeSessionState state, const std::string& reason,
    std::string* reject_reason)
{
    if (!m_arcade_state_machine.transitionTo(state, reason, reject_reason))
        return false;
    bumpVersionLocked();
    return true;
}

bool RelaySessionController::isFinishedStateLocked() const
{
    return m_state == RELAY_STATE_FINISHED_GOLD ||
        m_state == RELAY_STATE_FINISHED_SILVER ||
        m_state == RELAY_STATE_FINISHED_BRONZE ||
        m_state == RELAY_STATE_FINISHED_TIMEOUT ||
        m_state == RELAY_STATE_FINISHED_OPERATOR_ABORT ||
        m_state == RELAY_STATE_RESULTS;
}

bool RelaySessionController::isSessionPlayableStateLocked() const
{
    const ArcadeSessionState arcade_state =
        m_arcade_state_machine.getState();
    return arcade_state == ARCADE_STATE_RUNNING ||
        arcade_state == ARCADE_STATE_LAP_HANDOFF ||
        arcade_state == ARCADE_STATE_FINAL_LAP ||
        arcade_state == ARCADE_STATE_PAUSED;
}

RelaySessionReport RelaySessionController::buildReportLocked(
    const std::string& final_result) const
{
    RelaySessionReport report;
    report.m_available = true;
    report.m_session_id = m_active_config.m_session_id;
    report.m_player_count = (int)m_players.size();
    report.m_track_id = m_active_config.m_track_id;
    report.m_total_laps = m_total_laps;
    report.m_completed_laps =
        m_lap_progress >= 1.0f ? m_current_lap :
        std::max(0, m_current_lap - 1);
    if (final_result == "finished_operator_abort")
        report.m_completed_laps = std::max(0, m_current_lap - 1);
    report.m_final_result = final_result;
    report.m_score = m_crew_score;
    report.m_combo_count = m_event_director.getComboCount();
    report.m_encounters_started = m_event_director.getEncountersStarted();
    report.m_encounters_succeeded = m_event_director.getEncountersSucceeded();
    report.m_encounters_failed = m_event_director.getEncountersFailed();
    report.m_actions_by_role = m_actions_accepted_by_role;
    report.m_driver_turns_by_player = m_pilot_turns_by_player;
    const float finish_time = m_session_finished_at > 0.0f
        ? m_session_finished_at : m_last_race_time;
    report.m_duration_seconds =
        std::max(0, (int)(finish_time - m_session_started_at));
    report.m_render_profile = m_active_config.m_render_profile;
    report.m_projection = m_active_config.m_projection;
    report.m_output = m_active_config.m_output;
    report.m_crew_mode_enabled = m_active_config.m_crew_mode == "enabled";
    report.m_crew_actions_enabled = m_crew_actions_accepted > 0;
    report.m_crew_actions_accepted = m_crew_actions_accepted;
    report.m_mission = m_mission_result;
    report.m_scintilla_crew = m_crew_result;

    if (final_result == "finished_operator_abort")
        report.m_rank = "aborted";
    else if (m_crew_score >= 22000 &&
             report.m_encounters_failed == 0 &&
             report.m_completed_laps >= report.m_total_laps)
        report.m_rank = "perfect";
    else if (m_crew_score >= 12000)
        report.m_rank = "gold";
    else if (m_crew_score >= 7000)
        report.m_rank = "silver";
    else
        report.m_rank = "bronze";

    if (report.m_final_result.empty())
        report.m_final_result = "finished_" + report.m_rank;
    return report;
}

void RelaySessionController::publishReportLocked(
    const std::string& final_result)
{
    // A mission report is publishable only once its immutable terminal facts
    // exist. Retrying/late input never rebuilds it with changed crew statistics.
    if(m_mission_required && (!m_mission_result.valid ||
       (m_last_report.m_available && m_last_report.m_mission.valid))) return;
    // Missing SC5 roster evidence is an incomplete SC5 report, never a legacy
    // race. The gameplay gate remains held until a complete new configuration.
    if(m_mission_required&&m_crew_required&&!m_crew_result.valid)return;
    m_session_finished_at = m_last_race_time;
    m_last_report = buildReportLocked(final_result);

    const char* report_dir = std::getenv("STK_RELAY_REPORT_DIR");
    if (report_dir && report_dir[0])
    {
        std::string path = report_dir;
        if (!path.empty() && path[path.size() - 1] != '/')
            path += "/";
        path += m_last_report.m_scintilla_crew.valid
            ? ScintillaCrew::reportFileName(m_last_report.m_session_id,m_last_report.m_scintilla_crew)
            : m_last_report.m_session_id + ".json";
        std::ostringstream file;
        {
            file << "{";
            file << "\"session_id\":\""
                 << escapeReportJson(m_last_report.m_session_id) << "\"";
            file << ",\"player_count\":" << m_last_report.m_player_count;
            file << ",\"track_id\":\""
                 << escapeReportJson(m_last_report.m_track_id) << "\"";
            file << ",\"total_laps\":" << m_last_report.m_total_laps;
            file << ",\"completed_laps\":" << m_last_report.m_completed_laps;
            file << ",\"final_result\":\""
                 << escapeReportJson(m_last_report.m_final_result) << "\"";
            file << ",\"rank\":\""
                 << escapeReportJson(m_last_report.m_rank) << "\"";
            file << ",\"score\":" << m_last_report.m_score;
            file << ",\"combo_count\":" << m_last_report.m_combo_count;
            file << ",\"encounters_started\":"
                 << m_last_report.m_encounters_started;
            file << ",\"encounters_succeeded\":"
                 << m_last_report.m_encounters_succeeded;
            file << ",\"encounters_failed\":"
                 << m_last_report.m_encounters_failed;
            file << ",\"actions_by_role\":";
            writeRoleReportMap(file, m_last_report.m_actions_by_role);
            file << ",\"driver_turns_by_player\":";
            writeStringIntReportMap(file,
                                    m_last_report.m_driver_turns_by_player);
            file << ",\"duration_seconds\":"
                 << m_last_report.m_duration_seconds;
            file << ",\"render_profile\":\""
                 << escapeReportJson(m_last_report.m_render_profile) << "\"";
            file << ",\"projection\":\""
                 << escapeReportJson(m_last_report.m_projection) << "\"";
            file << ",\"output\":\""
                 << escapeReportJson(m_last_report.m_output) << "\"";
            file << ",\"crew_mode_enabled\":"
                 << (m_last_report.m_crew_mode_enabled ? "true" : "false");
            file << ",\"crew_actions_enabled\":"
                 << (m_last_report.m_crew_actions_enabled ? "true" : "false");
            file << ",\"crew_actions_accepted\":"
                 << m_last_report.m_crew_actions_accepted;
            file << ",\"mission\":";
            ScintillaMission::writeResultJSON(file,m_last_report.m_mission);
            file << ",\"crew\":";
            ScintillaCrew::writeResultJSON(file,m_last_report.m_scintilla_crew);
            if(m_last_report.m_scintilla_crew.valid)
                file << ",\"leaderboard_bucket\":\"" << m_last_report.m_scintilla_crew.bucket << '"';
            file << "}\n";
        }
        if(m_last_report.m_scintilla_crew.valid) {
            if(!ScintillaCrew::writeReportExclusive(path,file.str()))
                Log::warn("ImmersiveRelayArcade","SC5 report archive was not created (existing or unavailable): %s",path.c_str());
        } else {std::ofstream legacy(path.c_str());if(legacy)legacy<<file.str();}
    }

    Log::info("ImmersiveRelayArcade",
              "Session report ready session_id=%s result=%s score=%d rank=%s.",
              m_last_report.m_session_id.c_str(),
              m_last_report.m_final_result.c_str(),
              m_last_report.m_score,
              m_last_report.m_rank.c_str());
}

ScintillaCrew::Observation RelaySessionController::crewReportObservationLocked() const
{
    const auto rotation=m_rotation.getSnapshot(m_rotation_time);
    if(!m_crew_report_observation.available||m_crew_report_observation.snapshot.participants.size()!=m_players.size()||
       rotation.m_active_index<0||rotation.m_completed_handoffs<0)return ScintillaCrew::Observation();
    for(unsigned i=0;i<m_players.size();++i)
        if(m_players[i].m_player_id!=m_crew_report_observation.snapshot.participants[i].assignment.participantId)return ScintillaCrew::Observation();
    return ScintillaCrew::observeNativeTurn(m_crew_report_observation,unsigned(rotation.m_active_index),
        uint64_t(rotation.m_completed_handoffs),m_crew_administrative_handoffs);
}

void RelaySessionController::finalizeMissionArtificialLocked(const std::string& reason)
{
    if(!m_mission_required || m_mission_result.valid) return;
    // Observations include race identity even before physical preparation.
    // Exactly one terminal record per race: sequence 1 is scoped by raceEpoch.
    m_mission_result=m_mission_observation;
    m_mission_result.valid=true;m_mission_result.natural=false;
    m_mission_result.finishSequence=1;
    m_mission_result.officialTeamRacePosition=0;
    m_mission_result.nativeFinishTime=0;
    m_mission_result.terminationReason=reason;
    m_mission_result.outcome="not_completed";
    m_crew_result=ScintillaCrew::freezeResult(crewReportObservationLocked(),m_mission_result);
    if(m_crew_result.valid) {
        m_crew_result.comparable=false;
        m_crew_result.rotationAnomalies=std::max(m_crew_result.rotationAnomalies,m_crew_administrative_handoffs);
    }
}

void RelaySessionController::resetArcadeRuntimeLocked(
    bool keep_configuration)
{
    m_paused = false;
    m_resume_countdown = false;
    m_countdown_remaining_ms = 0;
    m_countdown_started_at = 0.0f;
    m_session_started_at = 0.0f;
    m_session_finished_at = 0.0f;
    if (!keep_configuration)
        m_arcade_configured = false;
}

void RelaySessionController::resetCrewStateLocked()
{
    m_pending_crew_actions.clear();
    m_track_effects.clear();
    m_idempotency_signatures.clear();
    m_idempotency_acks.clear();
    m_rate_ready_at_ms.clear();
    m_last_accepted_actions.clear();
    m_last_rejected_actions.clear();
    m_crew_score = 0;
    m_crew_multiplier_x100 = 100;
    m_engineer_boost_charge = 0;
    m_engine_heat = 0;
    m_armor = 100;
    m_crew_actions_accepted = 0;
    m_current_lap = 1;
    m_total_laps = getRecommendedLapCount(m_active_config.m_player_count);
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "pre_race";
    m_pilot_turns_by_player.clear();
    m_recorded_pilot_laps.clear();
    m_actions_accepted_by_role.clear();
    m_score_by_role.clear();
    resetAdaptiveDifficultyLocked();
    m_active_route_id.clear();
    m_last_crew_event.clear();
    m_last_crew_effect.clear();
    m_crew_roles.resetCooldowns();
    m_event_director.reset();
}

void RelaySessionController::resetAdaptiveDifficultyLocked()
{
    m_active_config.m_adaptive_difficulty =
        RelayDifficultyDirector::sanitizeConfig(
            m_active_config.m_adaptive_difficulty);
    m_adaptive_difficulty = RelayDifficultySnapshot();
    m_adaptive_difficulty.m_enabled =
        m_active_config.m_adaptive_difficulty.m_enabled;
    m_adaptive_difficulty.m_profile =
        m_active_config.m_adaptive_difficulty.m_profile;
    m_adaptive_difficulty.m_leaderboard_bucket =
        m_active_config.m_adaptive_difficulty.m_enabled
            ? "arcade_adaptive" : "arcade_fixed";
}

void RelaySessionController::configureCrewRolesLocked()
{
    const RelayPilotRotationSnapshot rotation=m_rotation.getSnapshot(m_rotation_time);
    if(rotation.m_completed_handoffs!=m_crew_observed_completed_handoffs)
    {
        // Every successful driver commit reaches this method while m_mutex is
        // still held, before any public world/input query can observe release.
        if(m_crew_required)
        {
            m_crew_start_permitted=false;
            m_crew_handoff_ready_required=true;
        }
        if(rotation.m_completed_handoffs>m_crew_observed_completed_handoffs &&
            m_crew_pending_administrative_handoff && m_crew_administrative_handoffs<UINT64_MAX)
            ++m_crew_administrative_handoffs;
        m_crew_observed_completed_handoffs=rotation.m_completed_handoffs;
        m_crew_pending_administrative_handoff=false;
    }
    else if(!rotation.m_handoff_active && !rotation.m_handoff_warning_active &&
            !rotation.m_handoff_pending_safe_zone)
        m_crew_pending_administrative_handoff=false;
    m_crew_roles.configure(m_players, m_rotation.getActiveIndex(),
        m_active_config.m_crew_mode == "enabled"
            ? m_connected_player_ids : std::vector<std::string>());
    m_event_director.setAvailableRoles(m_crew_roles.getAssignmentsSnapshot());
}

void RelaySessionController::recordPilotTurnForLapLocked(int lap)
{
    if (lap <= 0 || m_players.empty())
        return;
    if (std::find(m_recorded_pilot_laps.begin(),
                  m_recorded_pilot_laps.end(), lap) !=
        m_recorded_pilot_laps.end())
    {
        return;
    }

    const int active_index = (lap - 1) % (int)m_players.size();
    if (active_index < 0 || active_index >= (int)m_players.size())
        return;

    const std::string& player_id = m_players[active_index].m_player_id;
    m_pilot_turns_by_player[player_id]++;
    m_recorded_pilot_laps.push_back(lap);
}

CrewEncounterTriggerContext
RelaySessionController::buildTriggerContextLocked() const
{
    CrewEncounterTriggerContext context;
    context.m_track_id = m_active_config.m_track_id;
    context.m_current_lap = m_current_lap;
    context.m_total_laps = m_total_laps;
    context.m_lap_progress = m_lap_progress;
    context.m_checkpoint_id = m_current_checkpoint;
    context.m_distance = m_track_distance;
    context.m_player_count = (int)m_players.size();
    context.m_race_phase = m_race_phase;
    const RelayPilotRotationSnapshot rotation =
        m_rotation.getSnapshot(m_rotation_time);
    context.m_active_driver_index = rotation.m_active_index;
    context.m_active_driver_id = rotation.m_active_player_id;
    return context;
}

void RelaySessionController::consumeDirectorTrackEffectsLocked()
{
    const std::vector<RelayCrewTrackEffect> effects =
        m_event_director.consumeTrackEffects();
    for (unsigned int i = 0; i < effects.size(); i++)
        m_track_effects[effects[i].m_key] = effects[i].m_value;
}

void RelaySessionController::setRuntimeEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_runtime_enabled == enabled)
        return;
    m_runtime_enabled = enabled;
    if (enabled &&
        m_arcade_state_machine.getState() == ARCADE_STATE_BOOT)
    {
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_ATTRACT, "runtime_enabled", NULL);
    }
    else if (!enabled)
    {
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_SHUTDOWN, "runtime_disabled", NULL);
    }
    bumpVersionLocked();
}

bool RelaySessionController::isRuntimeEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runtime_enabled;
}

void RelaySessionController::setOperatorControlled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_operator_controlled = enabled;
    bumpVersionLocked();
}

bool RelaySessionController::isWorldHeld() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return isWorldHeldLocked();
}

RelaySpatialGameplayState RelaySessionController::getSpatialGameplayState() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    RelaySpatialGameplayState state;
    state.m_active = !isWorldHeldLocked();
    state.m_resume_revision = m_spatial_resume_revision;
    state.m_resume_at_us = m_spatial_resume_at_us;
    return state;
}

void RelaySessionController::worldLoadFailed(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    finalizeMissionArtificialLocked("world_load_failed");
    if(m_mission_required) publishReportLocked("finished_operator_abort");
    m_race_attached = false;
    m_loaded_track_id.clear();
    m_state = RELAY_STATE_ERROR_RECOVERABLE;
    m_arcade_state_machine.fail(reason, true, NULL);
    bumpVersionLocked();
}

void RelaySessionController::setDefaultPreset(const std::string& preset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_config.m_preset = preset;
    applyPresetLocked(&m_default_config);
    bumpVersionLocked();
}

void RelaySessionController::setDefaultPlayerCount(int player_count)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_config.m_player_count = sanitizePlayerCount(player_count);
    bumpVersionLocked();
}

void RelaySessionController::setArcadeUIEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_default_config.m_arcade_ui == enabled &&
        m_active_config.m_arcade_ui == enabled)
    {
        return;
    }
    m_default_config.m_arcade_ui = enabled;
    m_active_config.m_arcade_ui = enabled;
    bumpVersionLocked();
}

bool RelaySessionController::isArcadeUIEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runtime_enabled &&
        (m_default_config.m_arcade_ui || m_active_config.m_arcade_ui);
}

bool RelaySessionController::setDefaultHandoffPolicy(const std::string& policy)
{
    RelayHandoffPolicy parsed;
    if (!parseHandoffPolicy(policy, &parsed))
        return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_config.m_handoff_policy = parsed;
    bumpVersionLocked();
    return true;
}

bool RelaySessionController::parseHandoffPolicy(const std::string& value,
                                                RelayHandoffPolicy* policy)
{
    if (value == "checkpoint_or_timeout")
        *policy = RELAY_HANDOFF_CHECKPOINT_OR_TIMEOUT;
    else if (value == "checkpoint_only")
        *policy = RELAY_HANDOFF_CHECKPOINT_ONLY;
    else if (value == "fixed_time_segments")
        *policy = RELAY_HANDOFF_FIXED_TIME_SEGMENTS;
    else if (value == "lap_based_handoff")
        *policy = RELAY_HANDOFF_LAP_BASED;
    else if (value == "operator_manual_handoff")
        *policy = RELAY_HANDOFF_OPERATOR_MANUAL;
    else if (value == "single_pilot_demo")
        *policy = RELAY_HANDOFF_SINGLE_PILOT_DEMO;
    else
        return false;
    return true;
}

const char* RelaySessionController::handoffPolicyToString(
    RelayHandoffPolicy policy)
{
    switch (policy)
    {
    case RELAY_HANDOFF_CHECKPOINT_OR_TIMEOUT:
        return "checkpoint_or_timeout";
    case RELAY_HANDOFF_CHECKPOINT_ONLY:
        return "checkpoint_only";
    case RELAY_HANDOFF_FIXED_TIME_SEGMENTS:
        return "fixed_time_segments";
    case RELAY_HANDOFF_LAP_BASED:
        return "lap_based_handoff";
    case RELAY_HANDOFF_OPERATOR_MANUAL:
        return "operator_manual_handoff";
    case RELAY_HANDOFF_SINGLE_PILOT_DEMO:
        return "single_pilot_demo";
    }
    return "checkpoint_or_timeout";
}

const char* RelaySessionController::stateToString(RelaySessionState state)
{
    switch (state)
    {
    case RELAY_STATE_ATTRACT: return "attract";
    case RELAY_STATE_LOADING: return "loading";
    case RELAY_STATE_READY_FOR_PLAYERS: return "ready_for_players";
    case RELAY_STATE_BRIEFING: return "briefing";
    case RELAY_STATE_COUNTDOWN: return "countdown";
    case RELAY_STATE_RUNNING: return "running";
    case RELAY_STATE_HANDOFF: return "handoff";
    case RELAY_STATE_LAST_MINUTE: return "last_minute";
    case RELAY_STATE_FINAL_SPRINT: return "final_sprint";
    case RELAY_STATE_FINISHED_GOLD: return "finished_gold";
    case RELAY_STATE_FINISHED_SILVER: return "finished_silver";
    case RELAY_STATE_FINISHED_BRONZE: return "finished_bronze";
    case RELAY_STATE_FINISHED_TIMEOUT: return "finished_timeout";
    case RELAY_STATE_FINISHED_OPERATOR_ABORT:
        return "finished_operator_abort";
    case RELAY_STATE_RESULTS: return "results";
    case RELAY_STATE_RESETTING: return "resetting";
    case RELAY_STATE_ERROR_RECOVERABLE: return "error_recoverable";
    case RELAY_STATE_ERROR_FATAL: return "error_fatal";
    }
    return "error_recoverable";
}

int RelaySessionController::getRecommendedLapCount(int player_count,
                                                   int requested_laps)
{
    (void)requested_laps;
    if (player_count < 1)
        player_count = 1;
    if (player_count > 6)
        player_count = 6;

    if (player_count <= 3)
        return 6;
    if (player_count == 4)
        return 8;
    if (player_count == 5)
        return 10;
    return 12;
}

void RelaySessionController::setLocalApiOptions(bool enabled,
                                                const std::string& bind_host,
                                                int port,
                                                const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_api_options.m_enabled = enabled;
    if (!bind_host.empty())
        m_api_options.m_bind_host = bind_host;
    if (port > 0)
        m_api_options.m_port = port;
    if (!token.empty())
        m_api_options.m_token = token;
    bumpVersionLocked();
}

RelayLocalApiOptions RelaySessionController::getLocalApiOptions() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_api_options;
}

RelaySessionConfig RelaySessionController::getDefaultConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_default_config;
}

int RelaySessionController::sanitizePlayerCount(int player_count) const
{
    if (player_count < 1)
        return 1;
    if (player_count > 6)
        return 6;
    return player_count;
}

void RelaySessionController::applyPresetLocked(
    RelaySessionConfig* config) const
{
    if (config->m_preset == "quick_play")
    {
        config->m_gameplay_seconds = 300;
        config->m_slot_seconds = 480;
    }
    else if (config->m_preset == "premium_ride")
    {
        config->m_gameplay_seconds = 660;
        config->m_slot_seconds = 900;
    }
    else
    {
        config->m_preset = "standard_arcade";
        config->m_gameplay_seconds = 420;
        config->m_slot_seconds = 720;
    }
}

void RelaySessionController::ensureDefaultPlayersLocked(
    const RelaySessionConfig& config,
    std::vector<RelayPlayer>* players) const
{
    if (players->size() > 6)
        players->resize(6);
    if (!players->empty())
        return;

    const int count = sanitizePlayerCount(config.m_player_count);
    for (int i = 0; i < count; i++)
    {
        const std::string id = "p" + std::to_string(i + 1);
        const std::string name = "Player " + std::to_string(i + 1);
        players->push_back(RelayPlayer(id, name));
    }
}

bool RelaySessionController::allowsNewHandoffLocked(float race_time) const
{
    (void)race_time;
    if (m_players.size() < 2)
        return false;
    if (m_active_config.m_handoff_policy == RELAY_HANDOFF_SINGLE_PILOT_DEMO)
        return false;
    if (m_state == RELAY_STATE_FINISHED_GOLD ||
        m_state == RELAY_STATE_FINISHED_SILVER ||
        m_state == RELAY_STATE_FINISHED_BRONZE ||
        m_state == RELAY_STATE_FINISHED_TIMEOUT ||
        m_state == RELAY_STATE_FINISHED_OPERATOR_ABORT ||
        m_state == RELAY_STATE_RESULTS)
        return false;

    if (m_active_config.m_handoff_policy == RELAY_HANDOFF_LAP_BASED)
        return m_current_lap <= m_total_laps;

    const float remaining =
        (float)m_active_config.m_gameplay_seconds - race_time;
    return remaining > m_active_config.m_final_sprint_no_handoff_seconds;
}

void RelaySessionController::startSessionLocked(
    const RelaySessionConfig& config,
    const std::vector<RelayPlayer>& players)
{
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
        (m_crew_required && (!m_crew_start_permitted || m_crew_handoff_ready_required))) return;
    m_active_config = config;
    if (m_active_config.m_session_id.empty())
        m_active_config.m_session_id = makeArcadeSessionId();
    applyPresetLocked(&m_active_config);
    m_active_config.m_adaptive_difficulty =
        RelayDifficultyDirector::sanitizeConfig(
            m_active_config.m_adaptive_difficulty);

    std::vector<RelayPlayer> active_players = players;
    ensureDefaultPlayersLocked(m_active_config, &active_players);
    m_active_config.m_player_count =
        sanitizePlayerCount((int)active_players.size());
    if (m_active_config.m_player_count < 2)
        m_active_config.m_handoff_policy = RELAY_HANDOFF_SINGLE_PILOT_DEMO;
    m_total_laps = getRecommendedLapCount(m_active_config.m_player_count);
    m_current_lap = 1;
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "lap_start";

    m_players = active_players;
    m_rotation.configure(m_players, m_active_config.m_handoff_policy,
                         m_active_config.m_handoff_seconds,
                         m_active_config.m_min_handoff_seconds,
                         m_active_config.m_handoff_timeout_seconds,
                         m_active_config.m_handoff_warning_seconds,
                         m_active_config.m_segment_seconds,
                         m_active_config.m_require_handoff_confirm &&
                         !m_active_config.m_auto_confirm_handoff);
    if (m_active_config.m_test_accelerated_progress && !m_race_attached)
    {
        m_race_attached = true;
        Log::info("ImmersiveRelayArcade",
                  "Using virtual race attachment for accelerated test session.");
    }
    resetCrewStateLocked();
    m_total_laps = getRecommendedLapCount(m_active_config.m_player_count);
    m_current_lap = 1;
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "lap_start";
    recordPilotTurnForLapLocked(1);
    configureCrewRolesLocked();
    m_event_director.configure(m_active_config.m_track_id,
                               m_total_laps,
                               m_active_config.m_player_count);
    m_event_director.update(buildTriggerContextLocked(), 0);
    consumeDirectorTrackEffectsLocked();

    m_paused = false;
    m_state = RELAY_STATE_RUNNING;
    m_state_before_handoff = RELAY_STATE_RUNNING;
    m_arcade_configured = true;
    m_countdown_remaining_ms = 0;
    m_last_race_time = 0.0f;
    m_rotation_time = 0.0f;
    m_session_started_at = 0.0f;
    m_session_finished_at = 0.0f;
    m_arcade_state_machine.clearError();
    if (m_arcade_state_machine.getState() != ARCADE_STATE_RUNNING)
    {
        std::string ignored;
        if (m_arcade_state_machine.getState() == ARCADE_STATE_BOOT)
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_ATTRACT, "session_start_boot", &ignored);
        if (m_arcade_state_machine.getState() == ARCADE_STATE_ATTRACT ||
            m_arcade_state_machine.getState() == ARCADE_STATE_IDLE ||
            m_arcade_state_machine.getState() == ARCADE_STATE_RESULTS)
        {
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_SESSION_CONFIGURED,
                "session_start_configured", &ignored);
        }
        if (m_arcade_state_machine.getState() ==
            ARCADE_STATE_SESSION_CONFIGURED)
        {
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_READY_FOR_PLAYERS,
                "session_start_ready", &ignored);
        }
        if (m_arcade_state_machine.getState() ==
            ARCADE_STATE_READY_FOR_PLAYERS)
        {
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_BRIEFING, "session_start_briefing",
                &ignored);
        }
        if (m_arcade_state_machine.getState() == ARCADE_STATE_BRIEFING)
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_COUNTDOWN, "session_start_countdown",
                &ignored);
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_RUNNING, "session_started", &ignored);
    }
    bumpVersionLocked();

    Log::info("ImmersiveRelay",
        "Started relay session '%s' preset=%s players=%lu.",
        m_active_config.m_session_id.c_str(),
        m_active_config.m_preset.c_str(),
        (unsigned long)m_players.size());
}

bool RelaySessionController::startSession(
    const RelaySessionConfig& config,
    const std::vector<RelayPlayer>& players,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
        (m_crew_required && (!m_crew_start_permitted || m_crew_handoff_ready_required)))
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (!m_runtime_enabled)
    {
        if (reject_reason)
            *reject_reason = "relay_runtime_not_enabled";
        return false;
    }
    startSessionLocked(config, players);
    return true;
}

bool RelaySessionController::configureArcadeLocked(
    const RelaySessionConfig& config,
    const std::vector<RelayPlayer>& players,
    std::string* reject_reason)
{
    if (!m_runtime_enabled)
    {
        if (reject_reason)
            *reject_reason = "relay_runtime_not_enabled";
        return false;
    }
    const ArcadeSessionState current_state =
        m_arcade_state_machine.getState();
    if (current_state == ARCADE_STATE_RUNNING ||
        current_state == ARCADE_STATE_LAP_HANDOFF ||
        current_state == ARCADE_STATE_FINAL_LAP ||
        current_state == ARCADE_STATE_PAUSED ||
        current_state == ARCADE_STATE_COUNTDOWN ||
        current_state == ARCADE_STATE_BRIEFING)
    {
        if (reject_reason)
            *reject_reason = "already_running";
        return false;
    }
    if (!std::isfinite(config.m_handoff_seconds) || config.m_handoff_seconds < 6.0f || config.m_handoff_seconds > 60.0f ||
        !std::isfinite(config.m_min_handoff_seconds) || config.m_min_handoff_seconds < 6.0f || config.m_min_handoff_seconds > 30.0f ||
        config.m_handoff_seconds < config.m_min_handoff_seconds ||
        !std::isfinite(config.m_music_volume) || config.m_music_volume < 0.0f || config.m_music_volume > 1.0f ||
        !std::isfinite(config.m_sfx_volume) || config.m_sfx_volume < 0.0f || config.m_sfx_volume > 1.0f)
    {
        if (reject_reason) *reject_reason = "invalid_session_settings";
        return false;
    }
    if (config.m_player_count < 1 || config.m_player_count > 6)
    {
        if (reject_reason)
            *reject_reason = "invalid_player_count";
        return false;
    }
    if (config.m_lap_policy != "player_count_based")
    {
        if (reject_reason)
            *reject_reason = "invalid_lap_policy";
        return false;
    }
    if (config.m_handoff_policy != RELAY_HANDOFF_LAP_BASED &&
        !(config.m_player_count == 1 &&
          config.m_handoff_policy == RELAY_HANDOFF_SINGLE_PILOT_DEMO))
    {
        if (reject_reason)
            *reject_reason = "invalid_handoff_policy";
        return false;
    }
    if (config.m_projection != "fisheye_domemaster" && config.m_projection != "equirectangular")
    {
        if (reject_reason)
            *reject_reason = "invalid_projection";
        return false;
    }
    if (config.m_render_profile != "debug" &&
        config.m_render_profile != "safe" &&
        config.m_render_profile != "target" &&
        config.m_render_profile != "fulldome_debug" &&
        config.m_render_profile != "fulldome_safe" &&
        config.m_render_profile != "fulldome_target" &&
        config.m_render_profile != "equirectangular_debug" &&
        config.m_render_profile != "equirectangular_safe" &&
        config.m_render_profile != "equirectangular_target")
    {
        if (reject_reason)
            *reject_reason = "invalid_render_preset";
        return false;
    }
    if (config.m_track_id.empty())
    {
        if (reject_reason)
            *reject_reason = "invalid_track";
        return false;
    }

    RelaySessionConfig configured = config;
    configured.m_player_count = sanitizePlayerCount(configured.m_player_count);
    if (configured.m_session_id.empty())
        configured.m_session_id = makeArcadeSessionId();
    applyPresetLocked(&configured);
    configured.m_adaptive_difficulty =
        RelayDifficultyDirector::sanitizeConfig(
            configured.m_adaptive_difficulty);

    if (!players.empty() && (int)players.size() != configured.m_player_count)
    {
        if (reject_reason) *reject_reason = "invalid_players";
        return false;
    }
    std::vector<RelayPlayer> active_players = players;
    ensureDefaultPlayersLocked(configured, &active_players);
    std::vector<std::string> unique_ids;
    for (const RelayPlayer& player : active_players)
    {
        if (player.m_player_id.empty() || player.m_display_name.size() > 256 ||
            std::find(unique_ids.begin(), unique_ids.end(), player.m_player_id) != unique_ids.end())
        {
            if (reject_reason) *reject_reason = "invalid_players";
            return false;
        }
        unique_ids.push_back(player.m_player_id);
    }

    resetCrewAdmissionLocked();
    m_connected_player_ids.clear();
    m_resume_countdown = false;
    m_active_config = configured;
    if (m_active_config.m_player_count == 1)
        m_active_config.m_handoff_policy = RELAY_HANDOFF_SINGLE_PILOT_DEMO;
    m_operator_controlled = true;
    m_players = active_players;
    m_total_laps = getRecommendedLapCount(m_active_config.m_player_count);
    m_current_lap = 1;
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "configured";
    m_arcade_configured = true;
    m_paused = false;
    m_countdown_remaining_ms = 0;
    m_last_report = RelaySessionReport();
    m_rotation.configure(m_players, m_active_config.m_handoff_policy,
                         m_active_config.m_handoff_seconds,
                         m_active_config.m_min_handoff_seconds,
                         m_active_config.m_handoff_timeout_seconds,
                         m_active_config.m_handoff_warning_seconds,
                         m_active_config.m_segment_seconds,
                         m_active_config.m_require_handoff_confirm &&
                         !m_active_config.m_auto_confirm_handoff);
    resetCrewStateLocked();
    m_total_laps = getRecommendedLapCount(m_active_config.m_player_count);
    configureCrewRolesLocked();
    m_event_director.configure(m_active_config.m_track_id,
                               m_total_laps,
                               m_active_config.m_player_count);

    if (m_arcade_state_machine.getState() == ARCADE_STATE_BOOT)
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_ATTRACT, "operator_configure_boot", NULL);
    std::string ignored;
    m_arcade_state_machine.clearError();
    if (m_arcade_state_machine.getState() !=
        ARCADE_STATE_SESSION_CONFIGURED)
    {
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_SESSION_CONFIGURED,
            "operator_configure", &ignored);
    }
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_READY_FOR_PLAYERS,
        "operator_configure_ready", &ignored);
    m_state = RELAY_STATE_READY_FOR_PLAYERS;
    bumpVersionLocked();
    Log::info("ImmersiveRelayArcade",
              "Configured arcade session '%s' players=%d laps=%d track=%s kart=%s render=%s output=%s.",
              m_active_config.m_session_id.c_str(),
              m_active_config.m_player_count,
              m_total_laps,
              m_active_config.m_track_id.c_str(),
              m_active_config.m_kart_id.c_str(),
              m_active_config.m_render_profile.c_str(),
              m_active_config.m_output.c_str());
    return true;
}

bool RelaySessionController::configureArcadeSession(
    const RelaySessionConfig& config,
    const std::vector<RelayPlayer>& players,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return configureArcadeLocked(config, players, reject_reason);
}

bool RelaySessionController::setCrewPresence(
    const std::string& session_id, const std::vector<std::string>& connected,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_arcade_configured || session_id != m_active_config.m_session_id)
    {
        if (reject_reason) *reject_reason = "wrong_session";
        return false;
    }
    std::vector<std::string> validated;
    for (const std::string& id : connected)
    {
        const auto player = std::find_if(m_players.begin(), m_players.end(),
            [&id](const RelayPlayer& p) { return p.m_player_id == id; });
        if (player == m_players.end() ||
            std::find(validated.begin(), validated.end(), id) != validated.end())
        {
            if (reject_reason) *reject_reason = "invalid_connected_player_ids";
            return false;
        }
        validated.push_back(id);
    }
    if (m_connected_player_ids == validated) return true;
    m_connected_player_ids = validated;
    configureCrewRolesLocked();
    bumpVersionLocked();
    return true;
}

void RelaySessionController::setScintillaStartPermitted(bool permitted)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_scintilla_start_permitted==permitted)return;
    m_scintilla_start_permitted=permitted;bumpVersionLocked();
}

void RelaySessionController::setCrewStartState(bool required,bool permitted)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const bool allowed=!required||permitted;
    if(m_crew_required==required&&m_crew_start_permitted==allowed)return;
    m_crew_required=required;m_crew_start_permitted=allowed;
    if(!required)m_crew_handoff_ready_required=false;
    bumpVersionLocked();
}

uint64_t RelaySessionController::getCrewAdministrativeHandoffs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_crew_administrative_handoffs;
}

bool RelaySessionController::crewHandoffReadyRequired() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_crew_handoff_ready_required;
}

bool RelaySessionController::acknowledgeCrewHandoffReadyLocked(
    const std::string& player_id,std::string* reject_reason)
{
    if(!m_runtime_enabled || !m_arcade_configured || !m_race_attached ||
        !m_crew_handoff_ready_required || !m_crew_required || !m_crew_start_permitted ||
        !m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
        m_rotation.isHandoffActive())
    {
        if(reject_reason)*reject_reason="scintilla_not_prepared";
        return false;
    }
    const auto rotation=m_rotation.getSnapshot(m_rotation_time);
    if(!player_id.empty()&&player_id!=rotation.m_active_player_id)
    {
        if(reject_reason)*reject_reason="not_expected_pilot";
        return false;
    }
    const auto state=m_arcade_state_machine.getState();
    if(state!=ARCADE_STATE_RUNNING&&state!=ARCADE_STATE_FINAL_LAP&&state!=ARCADE_STATE_PAUSED)
    {
        if(reject_reason)*reject_reason="wrong_state";
        return false;
    }
    if(m_paused)
    {
        const auto next=m_current_lap>=m_total_laps?ARCADE_STATE_FINAL_LAP:ARCADE_STATE_RUNNING;
        if(!transitionArcadeLocked(next,"crew_handoff_ready",reject_reason))return false;
        m_paused=false;m_state=next==ARCADE_STATE_FINAL_LAP?RELAY_STATE_FINAL_SPRINT:RELAY_STATE_RUNNING;
        m_race_phase=next==ARCADE_STATE_FINAL_LAP?"final_lap":"racing";
    }
    // Stage A already supplied the physical transfer/countdown. The current
    // pilot/operator explicitly releases only this prepared stage B latch.
    m_crew_handoff_ready_required=false;
    bumpVersionLocked();return true;
}

void RelaySessionController::setPresentationReady(bool ready)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_presentation_ready == ready) return;
    m_presentation_ready = ready;
    bumpVersionLocked();
}

void RelaySessionController::beginMissionRace(
    bool required,const ScintillaMission::Result& observation,const ScintillaCrew::Observation& crew)
{
    if(!observation.raceEpoch || observation.valid) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_mission_observation.raceEpoch&&observation.raceEpoch<=m_mission_observation.raceEpoch)return;
    if(m_mission_required&&m_mission_observation.raceEpoch&&
       observation.raceEpoch!=m_mission_observation.raceEpoch) {
        finalizeMissionArtificialLocked("race_replaced");publishReportLocked("finished_operator_abort");
    }
    if(required || m_mission_required) m_last_report=RelaySessionReport();
    resetCrewAdmissionLocked();
    m_mission_required=required;m_mission_presentation_held=false;
    m_mission_result=ScintillaMission::Result();m_mission_observation=observation;
    m_crew_result=ScintillaCrew::Result();m_crew_report_observation=ScintillaCrew::boundObservation(crew,observation)?crew:ScintillaCrew::Observation();
    if(required) m_scintilla_start_permitted=false;
    bumpVersionLocked();
}

void RelaySessionController::observeMission(
    bool required,const ScintillaMission::Result& observation,const ScintillaCrew::Observation& crew)
{
    if(!observation.raceEpoch || !observation.missionRevision) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_mission_result.valid ||
       (m_mission_observation.raceEpoch &&
        observation.raceEpoch!=m_mission_observation.raceEpoch)) return;
    const bool changed=required!=m_mission_required || required;
    m_mission_required=required;m_mission_observation=observation;
    m_crew_report_observation=ScintillaCrew::boundObservation(crew,observation)?crew:ScintillaCrew::Observation();
    if(changed) bumpVersionLocked();
}

void RelaySessionController::setMissionPresentationHeld(bool held)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_mission_presentation_held==held) return;
    m_mission_presentation_held=held;bumpVersionLocked();
}

ScintillaMission::Result RelaySessionController::getMissionResult() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_mission_result;
}

ScintillaCrew::Result RelaySessionController::getCrewResult() const
{
    std::lock_guard<std::mutex> lock(m_mutex);return m_crew_result;
}

void RelaySessionController::finishMissionArtificial(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_mission_required || m_mission_result.valid) return;
    finalizeMissionArtificialLocked(reason);
    m_paused=false;m_state=RELAY_STATE_FINISHED_OPERATOR_ABORT;
    m_race_phase=reason;publishReportLocked("finished_operator_abort");
    m_arcade_state_machine.transitionTo(ARCADE_STATE_FINISH_SEQUENCE,reason,NULL);
    m_arcade_state_machine.transitionTo(ARCADE_STATE_RESULTS,reason+"_results",NULL);
    bumpVersionLocked();
}

bool RelaySessionController::pilotReady(const std::string& player_id,
                                         std::string* reject_reason)
{
    {
        std::lock_guard<std::mutex> gate(m_mutex);
        if(!m_scintilla_start_permitted || missionBlocksGameplayLocked()){if(reject_reason)*reject_reason="scintilla_not_prepared";return false;}
        if(m_crew_handoff_ready_required)return acknowledgeCrewHandoffReadyLocked(player_id,reject_reason);
    }

    // Public methods acquire the mutex individually; no world is replaced here.
    const RelaySessionSnapshot state = getSnapshot();
    if (!state.m_presentation_ready)
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    const std::string expected = state.m_rotation.m_handoff_active
        ? state.m_rotation.m_next_player_id : state.m_rotation.m_active_player_id;
    if (!player_id.empty() && player_id != expected)
    {
        if (reject_reason) *reject_reason = "not_expected_pilot";
        return false;
    }
    if (state.m_paused && state.m_rotation.m_handoff_active)
    {
        if (!operatorResume(reject_reason)) return false;
        return confirmHandoff(reject_reason);
    }
    if (state.m_paused)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
        { if (reject_reason) *reject_reason = "presentation_not_ready"; return false; }
        if (!transitionArcadeLocked(ARCADE_STATE_COUNTDOWN, "pilot_ready_resume", reject_reason))
            return false;
        m_resume_countdown = m_state_before_pause != ARCADE_STATE_COUNTDOWN || m_resume_countdown;
        m_paused = false;
        m_state = RELAY_STATE_COUNTDOWN;
        m_race_phase = "resume_countdown";
        m_countdown_remaining_ms = 3000;
        return true;
    }
    if (state.m_rotation.m_handoff_active) return confirmHandoff(reject_reason);
    return operatorCountdown(false, reject_reason);
}

bool RelaySessionController::operatorStart(std::string* reject_reason)
{
    {
        std::lock_guard<std::mutex> gate(m_mutex);
        if(!m_scintilla_start_permitted || missionBlocksGameplayLocked()){if(reject_reason)*reject_reason="scintilla_not_prepared";return false;}
        if(m_crew_handoff_ready_required)return acknowledgeCrewHandoffReadyLocked("",reject_reason);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (!m_race_attached && !m_active_config.m_test_accelerated_progress)
    {
        if (reject_reason)
            *reject_reason = "race_not_loaded";
        return false;
    }
    if (!m_arcade_configured)
    {
        if (reject_reason)
            *reject_reason = "session_not_configured";
        return false;
    }
    const ArcadeSessionState state = m_arcade_state_machine.getState();
    if (state == ARCADE_STATE_BRIEFING ||
        state == ARCADE_STATE_COUNTDOWN ||
        state == ARCADE_STATE_RUNNING)
    {
        if (reject_reason)
            *reject_reason = state == ARCADE_STATE_RUNNING
                ? "already_running" : "wrong_state";
        return false;
    }
    if (state != ARCADE_STATE_READY_FOR_PLAYERS &&
        state != ARCADE_STATE_SESSION_CONFIGURED &&
        state != ARCADE_STATE_RESULTS)
    {
        if (reject_reason)
            *reject_reason = "wrong_state";
        return false;
    }
    if (state == ARCADE_STATE_RESULTS)
    {
        std::string ignored;
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_READY_FOR_PLAYERS,
            "operator_start_from_results", &ignored);
    }
    if (!transitionArcadeLocked(ARCADE_STATE_BRIEFING,
                                "operator_start", reject_reason))
        return false;
    m_state = RELAY_STATE_BRIEFING;
    m_race_phase = "briefing";
    Log::info("ImmersiveRelayArcade",
              "Operator start accepted session_id=%s.",
              m_active_config.m_session_id.c_str());
    return true;
}

bool RelaySessionController::startRunningFromConfiguredLocked(
    std::string* reject_reason)
{
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (!m_arcade_configured)
    {
        if (reject_reason)
            *reject_reason = "session_not_configured";
        return false;
    }
    startSessionLocked(m_active_config, m_players);
    return true;
}

bool RelaySessionController::operatorCountdown(bool skip,
                                               std::string* reject_reason)
{
    {
        std::lock_guard<std::mutex> gate(m_mutex);
        if(!m_scintilla_start_permitted || missionBlocksGameplayLocked()){if(reject_reason)*reject_reason="scintilla_not_prepared";return false;}
        if(m_crew_handoff_ready_required)return acknowledgeCrewHandoffReadyLocked("",reject_reason);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (!m_arcade_configured)
    {
        if (reject_reason)
            *reject_reason = "session_not_configured";
        return false;
    }
    const ArcadeSessionState state = m_arcade_state_machine.getState();
    if (state != ARCADE_STATE_BRIEFING &&
        state != ARCADE_STATE_READY_FOR_PLAYERS &&
        state != ARCADE_STATE_SESSION_CONFIGURED)
    {
        if (reject_reason)
            *reject_reason = state == ARCADE_STATE_RUNNING
                ? "already_running" : "wrong_state";
        return false;
    }
    std::string ignored;
    if (state == ARCADE_STATE_READY_FOR_PLAYERS ||
        state == ARCADE_STATE_SESSION_CONFIGURED)
    {
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_BRIEFING, "operator_countdown_briefing",
            &ignored);
    }
    if (!transitionArcadeLocked(ARCADE_STATE_COUNTDOWN,
                                "operator_countdown", reject_reason))
        return false;
    m_resume_countdown = false;
    m_state = RELAY_STATE_COUNTDOWN;
    m_race_phase = "countdown";
    m_countdown_started_at = m_last_race_time;
    m_countdown_remaining_ms =
        std::max(0, m_active_config.m_countdown_seconds) * 1000;
    if (skip || m_countdown_remaining_ms == 0)
        return startRunningFromConfiguredLocked(reject_reason);
    return true;
}

bool RelaySessionController::operatorPause(std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_config.m_allow_pause)
    {
        if (reject_reason)
            *reject_reason = "pause_not_allowed";
        return false;
    }
    const ArcadeSessionState state = m_arcade_state_machine.getState();
    if (state == ARCADE_STATE_PAUSED)
        return true;
    if (state != ARCADE_STATE_RUNNING &&
        state != ARCADE_STATE_LAP_HANDOFF &&
        state != ARCADE_STATE_FINAL_LAP &&
        state != ARCADE_STATE_COUNTDOWN)
    {
        if (reject_reason)
            *reject_reason = "wrong_state";
        return false;
    }
    m_state_before_pause = state;
    m_paused = true;
    if (!transitionArcadeLocked(ARCADE_STATE_PAUSED,
                                "operator_pause", reject_reason))
        return false;
    m_state = RELAY_STATE_RUNNING;
    Log::info("ImmersiveRelayArcade", "Operator paused session.");
    return true;
}

bool RelaySessionController::operatorResume(std::string* reject_reason)
{
    {
        std::lock_guard<std::mutex> gate(m_mutex);
        if(!m_scintilla_start_permitted || missionBlocksGameplayLocked()){if(reject_reason)*reject_reason="scintilla_not_prepared";return false;}
        if(m_crew_handoff_ready_required)return acknowledgeCrewHandoffReadyLocked("",reject_reason);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (m_arcade_state_machine.getState() != ARCADE_STATE_PAUSED)
    {
        if (reject_reason)
            *reject_reason = "not_paused";
        return false;
    }
    m_paused = false;
    ArcadeSessionState next = ARCADE_STATE_RUNNING;
    if (m_current_lap >= m_total_laps)
        next = ARCADE_STATE_FINAL_LAP;
    if (m_rotation.isHandoffActive())
        next = ARCADE_STATE_LAP_HANDOFF;
    if (m_state_before_pause == ARCADE_STATE_COUNTDOWN)
        next = ARCADE_STATE_COUNTDOWN;
    if (!transitionArcadeLocked(next, "operator_resume", reject_reason))
        return false;
    m_state = next == ARCADE_STATE_LAP_HANDOFF ? RELAY_STATE_HANDOFF
        : (next == ARCADE_STATE_FINAL_LAP ? RELAY_STATE_FINAL_SPRINT
           : (next == ARCADE_STATE_COUNTDOWN ? RELAY_STATE_COUNTDOWN : RELAY_STATE_RUNNING));
    Log::info("ImmersiveRelayArcade", "Operator resumed session.");
    return true;
}

bool RelaySessionController::operatorAbort(std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_config.m_allow_abort)
    {
        if (reject_reason)
            *reject_reason = "abort_not_allowed";
        return false;
    }
    const ArcadeSessionState state = m_arcade_state_machine.getState();
    if (state == ARCADE_STATE_ATTRACT ||
        state == ARCADE_STATE_IDLE ||
        state == ARCADE_STATE_READY_FOR_PLAYERS ||
        state == ARCADE_STATE_SESSION_CONFIGURED ||
        state == ARCADE_STATE_RESULTS)
    {
        if (reject_reason)
            *reject_reason = "wrong_state";
        return false;
    }
    m_paused = false;
    m_state = RELAY_STATE_FINISHED_OPERATOR_ABORT;
    m_race_phase = "abort";
    finalizeMissionArtificialLocked("operator_abort");
    publishReportLocked("finished_operator_abort");
    if (!transitionArcadeLocked(ARCADE_STATE_FINISH_SEQUENCE,
                                "operator_abort", reject_reason))
        return false;
    transitionArcadeLocked(ARCADE_STATE_RESULTS,
                           "operator_abort_results", NULL);
    return true;
}

bool RelaySessionController::operatorReset(std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const ArcadeSessionState before = m_arcade_state_machine.getState();
    if (before == ARCADE_STATE_RESETTING)
    {
        if (reject_reason)
            *reject_reason = "reset_in_progress";
        return false;
    }
    finalizeMissionArtificialLocked("operator_reset");
    if(m_mission_required&&m_mission_result.valid)publishReportLocked("finished_operator_abort");
    transitionArcadeLocked(ARCADE_STATE_RESETTING,
                           "operator_reset_begin", NULL);
    const bool keep_configuration = m_arcade_configured;
    // A reset starts a new native race; the previous SC4 preparation cannot
    // authorize even the short interval before the world-reset callback.
    resetCrewAdmissionLocked();
    if (m_mission_required) m_scintilla_start_permitted = false;
    m_paused = false;
    m_state = RELAY_STATE_RESETTING;
    m_state_before_handoff = RELAY_STATE_RUNNING;
    m_rotation.configure(m_players, m_active_config.m_handoff_policy,
                         m_active_config.m_handoff_seconds,
                         m_active_config.m_min_handoff_seconds,
                         m_active_config.m_handoff_timeout_seconds,
                         m_active_config.m_handoff_warning_seconds,
                         m_active_config.m_segment_seconds,
                         m_active_config.m_require_handoff_confirm &&
                         !m_active_config.m_auto_confirm_handoff);
    resetCrewStateLocked();
    resetArcadeRuntimeLocked(keep_configuration);
    m_total_laps = getRecommendedLapCount(
        keep_configuration ? m_active_config.m_player_count
                           : m_default_config.m_player_count);
    m_current_lap = 1;
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "reset";
    configureCrewRolesLocked();
    m_event_director.configure(m_active_config.m_track_id,
                               m_total_laps,
                               m_active_config.m_player_count);
    m_last_race_time = 0.0f;
    m_rotation_time = 0.0f;
    m_arcade_state_machine.clearError();
    transitionArcadeLocked(keep_configuration
                               ? ARCADE_STATE_READY_FOR_PLAYERS
                               : ARCADE_STATE_ATTRACT,
                           "operator_reset_ready", NULL);
    m_state = keep_configuration ? RELAY_STATE_READY_FOR_PLAYERS
                                 : RELAY_STATE_ATTRACT;
    Log::info("ImmersiveRelayArcade",
              "Operator reset complete keep_configuration=%s.",
              keep_configuration ? "true" : "false");
    return true;
}

bool RelaySessionController::operatorForceFinish(std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_config.m_allow_force_finish)
    {
        if (reject_reason)
            *reject_reason = "force_finish_not_allowed";
        return false;
    }
    if (!isSessionPlayableStateLocked())
    {
        if (reject_reason)
            *reject_reason = "wrong_state";
        return false;
    }
    m_paused = false;
    m_current_lap = m_total_laps;
    m_lap_progress = 1.0f;
    m_race_phase = "finish";
    m_state = m_crew_score >= 12000 ? RELAY_STATE_FINISHED_GOLD :
        (m_crew_score >= 7000 ? RELAY_STATE_FINISHED_SILVER :
         RELAY_STATE_FINISHED_BRONZE);
    m_last_crew_effect = "operator_force_finish";
    m_track_effects["session:finish"] = stateToString(m_state);
    finalizeMissionArtificialLocked("operator_force_finish");
    publishReportLocked(stateToString(m_state));
    transitionArcadeLocked(ARCADE_STATE_FINISH_SEQUENCE,
                           "operator_force_finish", NULL);
    transitionArcadeLocked(ARCADE_STATE_RESULTS,
                           "operator_force_finish_results", NULL);
    return true;
}

bool RelaySessionController::operatorNextState(
    const std::string& requested_state, std::string* reject_reason)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const ArcadeSessionState before = m_arcade_state_machine.getState();
    if (before == ARCADE_STATE_RESULTS &&
        (requested_state.empty() || requested_state == "ready_for_players"))
    {
        // Match Ready/reset: retain configuration, reset rotation and close
        // SC4 admission. The router then resets the native world once.
        lock.unlock();
        return operatorReset(reject_reason);
    }
    if (requested_state.empty())
    {
        const ArcadeSessionState state = m_arcade_state_machine.getState();
        if (state == ARCADE_STATE_FINISH_SEQUENCE)
            return transitionArcadeLocked(ARCADE_STATE_RESULTS,
                                          "operator_next_state",
                                          reject_reason);
        if (reject_reason)
            *reject_reason = "wrong_state";
        return false;
    }
    bool ok = false;
    const ArcadeSessionState state =
        ArcadeSessionStateMachine::stateFromString(requested_state, &ok);
    if (!ok)
    {
        if (reject_reason)
            *reject_reason = "invalid_state";
        return false;
    }
    if ((state == ARCADE_STATE_BRIEFING || state == ARCADE_STATE_COUNTDOWN ||
         state == ARCADE_STATE_RUNNING || state == ARCADE_STATE_FINAL_LAP ||
         state == ARCADE_STATE_LAP_HANDOFF) &&
        (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
         m_crew_handoff_ready_required))
    {
        if (reject_reason) *reject_reason = "scintilla_not_prepared";
        return false;
    }
    return transitionArcadeLocked(state, "operator_next_state",
                                  reject_reason);
}

bool RelaySessionController::operatorRenderPreset(
    const std::string& render_profile,
    const std::string& output,
    const std::string& projection,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string profile =
        render_profile.empty() ? m_active_config.m_render_profile
                               : render_profile;
    if (profile != "debug" &&
        profile != "safe" &&
        profile != "target" &&
        profile != "fulldome_debug" &&
        profile != "fulldome_safe" &&
        profile != "fulldome_target" &&
        profile != "equirectangular_debug" &&
        profile != "equirectangular_safe" &&
        profile != "equirectangular_target")
    {
        if (reject_reason)
            *reject_reason = "invalid_render_preset";
        return false;
    }
    const std::string requested_projection =
        projection.empty() ? m_active_config.m_projection : projection;
    if ((requested_projection != "fisheye_domemaster" && requested_projection != "equirectangular") ||
        (profile.find("equirectangular_") == 0 && requested_projection != "equirectangular"))
    {
        if (reject_reason)
            *reject_reason = "invalid_projection";
        return false;
    }
    const std::string requested_output =
        output.empty() ? m_active_config.m_output : output;
    if (requested_output != "none" && requested_output != "window" &&
        requested_output != "syphon")
    {
        if (reject_reason)
            *reject_reason = "invalid_output";
        return false;
    }
    m_active_config.m_render_profile = profile;
    m_active_config.m_output = requested_output;
    m_active_config.m_projection = requested_projection;
    bumpVersionLocked();
    Log::info("ImmersiveRelayArcade",
              "Operator render preset profile=%s projection=%s output=%s.",
              profile.c_str(), requested_projection.c_str(),
              requested_output.c_str());
    return true;
}

bool RelaySessionController::confirmHandoff(std::string* reject_reason)
{
    {
        std::lock_guard<std::mutex> gate(m_mutex);
        if(!m_scintilla_start_permitted || missionBlocksGameplayLocked()){if(reject_reason)*reject_reason="scintilla_not_prepared";return false;}
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
    {
        if (reject_reason) *reject_reason = "presentation_not_ready";
        return false;
    }
    if (!m_rotation.isHandoffActive())
    {
        if (reject_reason)
            *reject_reason = "handoff_not_active";
        return false;
    }
    if (m_rotation.confirmHandoff(m_rotation_time, reject_reason))
    {
        configureCrewRolesLocked();
        if (!m_rotation.isHandoffActive())
        {
            const ArcadeSessionState next =
                m_current_lap >= m_total_laps
                    ? ARCADE_STATE_FINAL_LAP
                    : ARCADE_STATE_RUNNING;
            transitionArcadeLocked(next, "operator_handoff_confirm", NULL);
            m_state = next == ARCADE_STATE_FINAL_LAP
                ? RELAY_STATE_FINAL_SPRINT : RELAY_STATE_RUNNING;
        }
        bumpVersionLocked();
        return true;
    }
    return false;
}

void RelaySessionController::stopSession(const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_mission_required && m_mission_result.valid) return;
    m_paused = false;
    m_state = RELAY_STATE_FINISHED_OPERATOR_ABORT;
    finalizeMissionArtificialLocked(reason);
    publishReportLocked("finished_operator_abort");
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_FINISH_SEQUENCE, reason, NULL);
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_RESULTS, reason + "_results", NULL);
    bumpVersionLocked();
    Log::info("ImmersiveRelay", "Stopped relay session: %s.",
              reason.c_str());
}

void RelaySessionController::pauseSession()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = true;
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_PAUSED, "legacy_pause", NULL);
    bumpVersionLocked();
}

void RelaySessionController::resumeSession()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked()) return;
    if(m_crew_handoff_ready_required)
    {
        acknowledgeCrewHandoffReadyLocked("",NULL);return;
    }
    m_paused = false;
    if (m_arcade_state_machine.getState() == ARCADE_STATE_PAUSED)
        m_arcade_state_machine.transitionTo(
            m_rotation.isHandoffActive() ? ARCADE_STATE_LAP_HANDOFF :
            (m_current_lap >= m_total_laps ? ARCADE_STATE_FINAL_LAP :
             ARCADE_STATE_RUNNING),
            "legacy_resume", NULL);
    bumpVersionLocked();
}

void RelaySessionController::resetSession()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    finalizeMissionArtificialLocked("session_reset");
    if(m_mission_required&&m_mission_result.valid)publishReportLocked("finished_operator_abort");
    m_paused = false;
    resetCrewAdmissionLocked();
    if (m_mission_required) m_scintilla_start_permitted = false;
    m_state = RELAY_STATE_ATTRACT;
    m_state_before_handoff = RELAY_STATE_RUNNING;
    resetArcadeRuntimeLocked(false);
    m_players.clear();
    m_rotation.configure(m_players, m_default_config.m_handoff_policy,
                         m_default_config.m_handoff_seconds,
                         m_default_config.m_min_handoff_seconds,
                         m_default_config.m_handoff_timeout_seconds,
                         m_default_config.m_handoff_warning_seconds,
                         m_default_config.m_segment_seconds,
                         m_default_config.m_require_handoff_confirm &&
                         !m_default_config.m_auto_confirm_handoff);
    resetCrewStateLocked();
    m_total_laps = getRecommendedLapCount(m_default_config.m_player_count);
    m_current_lap = 1;
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_track_distance = 0.0f;
    m_race_phase = "reset";
    configureCrewRolesLocked();
    m_event_director.configure(m_default_config.m_track_id,
                               m_total_laps,
                               m_default_config.m_player_count);
    m_last_race_time = 0.0f;
    m_rotation_time = 0.0f;
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_RESETTING, "legacy_reset", NULL);
    m_arcade_state_machine.transitionTo(
        ARCADE_STATE_ATTRACT, "legacy_reset_attract", NULL);
    bumpVersionLocked();
    Log::info("ImmersiveRelay", "Relay session reset.");
}

void RelaySessionController::attachRace(const std::string& track_id,
                                        unsigned int player_count)
{
    (void)player_count;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_race_attached = true;
    m_loaded_track_id = track_id;
    m_world_elapsed_ms = 0;
    if (!track_id.empty())
    {
        m_default_config.m_track_id = track_id;
        if (m_active_config.m_track_id.empty())
            m_active_config.m_track_id = track_id;
    }

    if (m_runtime_enabled && !m_operator_controlled &&
        (m_state == RELAY_STATE_ATTRACT ||
         m_state == RELAY_STATE_READY_FOR_PLAYERS))
    {
        RelaySessionConfig config = m_default_config;
        config.m_track_id = track_id;
        std::vector<RelayPlayer> players;
        startSessionLocked(config, players);
        return;
    }

    bumpVersionLocked();
}

void RelaySessionController::detachRace()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    finalizeMissionArtificialLocked("world_detached");
    if(m_mission_required && m_mission_result.valid && !m_last_report.m_available)
        publishReportLocked("finished_operator_abort");
    m_race_attached = false;
    m_loaded_track_id.clear();
    bumpVersionLocked();
}

void RelaySessionController::setTestAcceleratedProgressEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_config.m_test_accelerated_progress = enabled;
    m_active_config.m_test_accelerated_progress = enabled;
    bumpVersionLocked();
}

void RelaySessionController::updateProgressLocked(
    int current_lap, float lap_progress, int checkpoint_id, float distance,
    bool force_rotation)
{
    if (m_players.empty())
        return;

    if (current_lap < 1)
        current_lap = 1;
    if (current_lap > m_total_laps)
        current_lap = m_total_laps;
    lap_progress = std::max(0.0f, std::min(1.0f, lap_progress));

    const bool lap_changed = current_lap != m_current_lap;
    const bool checkpoint_changed = checkpoint_id >= 0 && checkpoint_id != m_current_checkpoint;
    const bool progress_changed =
        std::fabs(lap_progress - m_lap_progress) > 0.001f;
    m_current_lap = current_lap;
    m_lap_progress = lap_progress;
    if (checkpoint_id >= 0)
        m_current_checkpoint = checkpoint_id;
    m_track_distance = std::max(0.0f, distance);
    if (lap_changed)
        recordPilotTurnForLapLocked(current_lap);

    if (force_rotation)
    {
        const float test_time =
            (float)(current_lap - 1) * 30.0f + lap_progress * 20.0f;
        if (test_time > m_rotation_time)
            m_rotation_time = test_time;
        if (lap_changed || progress_changed || checkpoint_id >= 0)
            m_crew_roles.resetCooldowns();
        m_rotation.setActiveIndexForTest(
            (current_lap - 1) % (int)m_players.size(), m_rotation_time);
        configureCrewRolesLocked();
    }

    updateTimedStateLocked(m_last_race_time);
    if (m_current_lap >= m_total_laps && m_lap_progress >= 1.0f &&
        (!m_mission_required || m_active_config.m_test_accelerated_progress))
    {
        m_race_phase = "finish";
        if (m_crew_score >= 12000)
            m_state = RELAY_STATE_FINISHED_GOLD;
        else if (m_crew_score >= 7000)
            m_state = RELAY_STATE_FINISHED_SILVER;
        else
            m_state = RELAY_STATE_FINISHED_BRONZE;
        m_track_effects["session:finish"] = stateToString(m_state);
        finalizeMissionArtificialLocked("test_accelerated_progress");
        publishReportLocked(stateToString(m_state));
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_FINISH_SEQUENCE, "lap_finish", NULL);
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_RESULTS, "lap_finish_results", NULL);
    }
    if (m_event_director.update(buildTriggerContextLocked(),
                                (int)(m_rotation_time * 1000.0f)))
    {
        consumeDirectorTrackEffectsLocked();
        bumpVersionLocked();
    }
    else if (lap_changed || checkpoint_changed)
    {
        bumpVersionLocked();
    }
}

bool RelaySessionController::applyTestProgress(
    int current_lap, float lap_progress, int checkpoint_id,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_config.m_test_accelerated_progress)
    {
        if (reject_reason)
            *reject_reason = "relay_test_progress_not_enabled";
        return false;
    }
    updateProgressLocked(current_lap, lap_progress, checkpoint_id, 0.0f, true);
    return true;
}

void RelaySessionController::updateTrackProgress(
    int current_lap, float lap_progress, int checkpoint_id, float distance)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime_enabled || m_paused ||
        m_active_config.m_test_accelerated_progress || isFinishedStateLocked() ||
        (m_operator_controlled && !isSessionPlayableStateLocked()))
        return;
    updateProgressLocked(current_lap, lap_progress, checkpoint_id, distance,
                         false);
}

void RelaySessionController::updateTimedStateLocked(float race_time)
{
    (void)race_time;
    if (m_operator_controlled && !isSessionPlayableStateLocked())
        return;
    if (m_state == RELAY_STATE_FINISHED_GOLD ||
        m_state == RELAY_STATE_FINISHED_SILVER ||
        m_state == RELAY_STATE_FINISHED_BRONZE ||
        m_state == RELAY_STATE_FINISHED_TIMEOUT ||
        m_state == RELAY_STATE_FINISHED_OPERATOR_ABORT ||
        m_state == RELAY_STATE_RESULTS ||
        m_state == RELAY_STATE_ATTRACT ||
        m_arcade_state_machine.getState() == ARCADE_STATE_PAUSED ||
        m_arcade_state_machine.getState() == ARCADE_STATE_RESULTS ||
        m_arcade_state_machine.getState() == ARCADE_STATE_RESETTING)
        return;

    RelaySessionState new_state = RELAY_STATE_RUNNING;
    if (m_current_lap >= m_total_laps && m_lap_progress >= 0.68f)
    {
        new_state = RELAY_STATE_FINAL_SPRINT;
        m_race_phase = "final_stretch";
    }
    else if (m_current_lap >= m_total_laps)
    {
        new_state = RELAY_STATE_FINAL_SPRINT;
        m_race_phase = "final_lap";
    }
    else if (m_lap_progress <= 0.04f)
    {
        m_race_phase = "lap_start";
    }
    else if (m_lap_progress >= 0.92f)
    {
        m_race_phase = "lap_end";
    }
    else
    {
        m_race_phase = "running_lap";
    }

    if (m_state != new_state)
    {
        m_state = new_state;
        bumpVersionLocked();
    }

    if (new_state == RELAY_STATE_FINAL_SPRINT &&
        m_arcade_state_machine.getState() != ARCADE_STATE_FINAL_LAP &&
        m_arcade_state_machine.getState() != ARCADE_STATE_LAP_HANDOFF)
    {
        m_track_effects["session:final_lap"] = "active";
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_FINAL_LAP, "final_lap", NULL);
    }
    else if (new_state == RELAY_STATE_RUNNING &&
             m_arcade_state_machine.getState() != ARCADE_STATE_RUNNING &&
             m_arcade_state_machine.getState() != ARCADE_STATE_LAP_HANDOFF)
    {
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_RUNNING, "running_lap", NULL);
    }
}

void RelaySessionController::updateWorldClock(float race_time)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_world_elapsed_ms = (int)(std::max(0.0f, race_time) * 1000.0f);
}

void RelaySessionController::updateRace(float dt, float race_time,
                                        bool safe_for_handoff)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_world_elapsed_ms = (int)(std::max(0.0f, race_time) * 1000.0f);
    if (!m_runtime_enabled || !m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
        m_crew_handoff_ready_required)
        return;

    if (m_arcade_state_machine.getState() == ARCADE_STATE_COUNTDOWN &&
        !m_paused)
    {
        // Physics and race time stay frozen during operator countdown.
        m_countdown_remaining_ms = std::max(0,
            m_countdown_remaining_ms - (int)std::round(std::max(0.0f, dt) * 1000.0f));
        if (m_countdown_remaining_ms <= 0)
        {
            std::string ignored;
            if (m_resume_countdown)
            {
                m_resume_countdown = false;
                const ArcadeSessionState next = m_rotation.isHandoffActive()
                    ? ARCADE_STATE_LAP_HANDOFF
                    : (m_current_lap >= m_total_laps ? ARCADE_STATE_FINAL_LAP : ARCADE_STATE_RUNNING);
                transitionArcadeLocked(next, "pilot_resume_countdown_complete", &ignored);
                m_state = m_rotation.isHandoffActive() ? RELAY_STATE_HANDOFF
                    : (next == ARCADE_STATE_FINAL_LAP ? RELAY_STATE_FINAL_SPRINT : RELAY_STATE_RUNNING);
                m_race_phase = m_rotation.isHandoffActive() ? "handoff" : "racing";
                if (m_rotation.isHandoffActive())
                {
                    m_rotation.confirmHandoff(m_rotation_time, &ignored);
                    configureCrewRolesLocked();
                }
            }
            else startRunningFromConfiguredLocked(&ignored);
        }
        else
        {
            bumpVersionLocked();
            return;
        }
    }
    if (m_paused)
        return;
    if (m_active_config.m_test_accelerated_progress ||
        (m_operator_controlled && !isSessionPlayableStateLocked()))
        return;

    m_last_race_time = race_time;
    m_rotation_time += std::max(0.0f, dt);
    if (m_rotation_time < race_time)
        m_rotation_time = race_time;
    m_last_safe_for_handoff = safe_for_handoff;

    updateTimedStateLocked(race_time);
    if (m_event_director.update(buildTriggerContextLocked(),
                                (int)(m_rotation_time * 1000.0f)))
    {
        consumeDirectorTrackEffectsLocked();
        bumpVersionLocked();
    }

    const bool was_handoff = m_rotation.isHandoffActive();
    if (m_rotation.update(m_rotation_time, safe_for_handoff,
                          allowsNewHandoffLocked(race_time)))
    {
        configureCrewRolesLocked();
        bumpVersionLocked();
    }
    const bool is_handoff = m_rotation.isHandoffActive();

    if (is_handoff)
    {
        if (m_state != RELAY_STATE_HANDOFF)
        {
            m_state_before_handoff = m_state;
            m_state = RELAY_STATE_HANDOFF;
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_LAP_HANDOFF, "lap_handoff", NULL);
            bumpVersionLocked();
        }
    }
    else
    {
        if (was_handoff || m_state == RELAY_STATE_HANDOFF)
        {
            m_state = m_state_before_handoff;
            const ArcadeSessionState next =
                m_current_lap >= m_total_laps
                    ? ARCADE_STATE_FINAL_LAP
                    : ARCADE_STATE_RUNNING;
            m_arcade_state_machine.transitionTo(
                next, "lap_handoff_complete", NULL);
        }
        updateTimedStateLocked(race_time);
    }
}

void RelaySessionController::updateAdaptiveDifficulty(
    const RelayDifficultySnapshot& difficulty)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime_enabled)
        return;

    const bool discrete_change =
        m_adaptive_difficulty.m_enabled != difficulty.m_enabled ||
        m_adaptive_difficulty.m_profile != difficulty.m_profile ||
        m_adaptive_difficulty.m_pressure != difficulty.m_pressure ||
        m_adaptive_difficulty.m_assist_level != difficulty.m_assist_level ||
        m_adaptive_difficulty.m_ai_pace_level != difficulty.m_ai_pace_level ||
        m_adaptive_difficulty.m_leaderboard_bucket !=
            difficulty.m_leaderboard_bucket ||
        m_adaptive_difficulty.m_final_lap_lock !=
            difficulty.m_final_lap_lock ||
        m_adaptive_difficulty.m_handoff_grace !=
            difficulty.m_handoff_grace;

    m_adaptive_difficulty = difficulty;
    if (discrete_change)
        bumpVersionLocked();
}

void RelaySessionController::notifyCheckpoint(int checkpoint_id, float distance,
                                              float race_time,
                                              bool safe_for_handoff)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime_enabled || m_paused ||
        m_active_config.m_test_accelerated_progress ||
        m_state == RELAY_STATE_FINISHED_GOLD ||
        m_state == RELAY_STATE_FINISHED_SILVER ||
        m_state == RELAY_STATE_FINISHED_BRONZE ||
        m_state == RELAY_STATE_FINISHED_TIMEOUT ||
        m_state == RELAY_STATE_FINISHED_OPERATOR_ABORT)
        return;

    m_current_checkpoint = checkpoint_id;
    m_track_distance = std::max(0.0f, distance);
    updateTimedStateLocked(race_time);
    if (m_event_director.update(buildTriggerContextLocked(),
                                (int)(m_rotation_time * 1000.0f)))
    {
        consumeDirectorTrackEffectsLocked();
        bumpVersionLocked();
    }

    if (m_rotation.notifyCheckpoint(checkpoint_id, m_rotation_time,
                                    safe_for_handoff,
                                    allowsNewHandoffLocked(race_time)))
    {
        configureCrewRolesLocked();
        m_state_before_handoff = m_state;
        if (m_rotation.isHandoffActive())
        {
            m_state = RELAY_STATE_HANDOFF;
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_LAP_HANDOFF, "checkpoint_handoff", NULL);
        }
        bumpVersionLocked();
        Log::info("ImmersiveRelay",
                  "Relay checkpoint %d requested handoff.", checkpoint_id);
    }
}

void RelaySessionController::notifyLap(int completed_laps, float race_time,
                                       bool safe_for_handoff,
                                       const ScintillaMission::Result* mission_result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime_enabled || m_paused ||
        m_active_config.m_test_accelerated_progress ||
        m_state == RELAY_STATE_FINISHED_GOLD ||
        m_state == RELAY_STATE_FINISHED_SILVER ||
        m_state == RELAY_STATE_FINISHED_BRONZE ||
        m_state == RELAY_STATE_FINISHED_TIMEOUT ||
        m_state == RELAY_STATE_FINISHED_OPERATOR_ABORT)
        return;

    if (completed_laps >= m_total_laps)
    {
        if(m_mission_required)
        {
            if(m_mission_result.valid || !mission_result || !mission_result->valid ||
               !mission_result->natural || mission_result->finishSequence!=1 ||
               mission_result->raceEpoch!=m_mission_observation.raceEpoch ||
               mission_result->missionRevision!=m_mission_observation.missionRevision)
                return;
            m_mission_result=*mission_result;
            m_crew_result=ScintillaCrew::freezeResult(crewReportObservationLocked(),m_mission_result);
            if(m_crew_result.valid&&m_crew_administrative_handoffs) {
                m_crew_result.comparable=false;
                m_crew_result.rotationAnomalies=std::max(m_crew_result.rotationAnomalies,m_crew_administrative_handoffs);
            }
        }
        m_last_race_time = race_time;
        m_current_lap = m_total_laps;
        m_lap_progress = 1.0f;
        m_race_phase = "finish";
        if (m_crew_score >= 12000)
            m_state = RELAY_STATE_FINISHED_GOLD;
        else if (m_crew_score >= 7000)
            m_state = RELAY_STATE_FINISHED_SILVER;
        else
            m_state = RELAY_STATE_FINISHED_BRONZE;
        m_last_crew_effect = "relay_finish";
        m_track_effects["session:finish"] = stateToString(m_state);
        m_event_director.update(buildTriggerContextLocked(),
                                (int)(m_rotation_time * 1000.0f));
        consumeDirectorTrackEffectsLocked();
        publishReportLocked(stateToString(m_state));
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_FINISH_SEQUENCE, "lap_finish", NULL);
        m_arcade_state_machine.transitionTo(
            ARCADE_STATE_RESULTS, "lap_finish_results", NULL);
        bumpVersionLocked();
        Log::info("ImmersiveRelay",
                  "Relay finished after %d lap(s), score=%d.",
                  completed_laps, m_crew_score);
        return;
    }

    m_current_lap = std::min(m_total_laps, completed_laps + 1);
    m_lap_progress = 0.0f;
    m_current_checkpoint = -1;
    m_race_phase = m_current_lap == m_total_laps
        ? "final_lap" : "lap_start";
    recordPilotTurnForLapLocked(m_current_lap);

    updateTimedStateLocked(race_time);
    if (m_event_director.update(buildTriggerContextLocked(),
                                (int)(m_rotation_time * 1000.0f)))
    {
        consumeDirectorTrackEffectsLocked();
        bumpVersionLocked();
    }

    if (m_rotation.notifyLap(completed_laps, m_rotation_time, safe_for_handoff,
                             allowsNewHandoffLocked(race_time)))
    {
        configureCrewRolesLocked();
        m_state_before_handoff = m_state;
        if (m_rotation.isHandoffActive())
        {
            m_state = RELAY_STATE_HANDOFF;
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_LAP_HANDOFF, "lap_handoff", NULL);
        }
        bumpVersionLocked();
        Log::info("ImmersiveRelay",
                  "Relay lap %d requested handoff.", completed_laps);
    }
}

bool RelaySessionController::forceHandoff(const std::string& player_id,
                                          std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_runtime_enabled)
    {
        if (reject_reason)
            *reject_reason = "relay_runtime_not_enabled";
        return false;
    }

    if(crewBlocksGameplayLocked() || m_crew_handoff_ready_required)
    {
        if(reject_reason)*reject_reason="scintilla_not_prepared";
        return false;
    }

    if (!allowsNewHandoffLocked(m_last_race_time))
    {
        if (reject_reason)
            *reject_reason = m_players.size() < 2
                ? "single_pilot_no_handoff"
                : "handoff_blocked_final_sprint";
        return false;
    }

    if (m_rotation.forceHandoff(player_id, m_rotation_time,
                                m_last_safe_for_handoff, reject_reason))
    {
        m_crew_pending_administrative_handoff=true;
        configureCrewRolesLocked();
        m_state_before_handoff = m_state;
        if (m_rotation.isHandoffActive())
        {
            m_state = RELAY_STATE_HANDOFF;
            m_arcade_state_machine.transitionTo(
                ARCADE_STATE_LAP_HANDOFF, "operator_forced_handoff",
                NULL);
        }
        bumpVersionLocked();
        return true;
    }
    return false;
}

bool RelaySessionController::skipHandoff()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked())
        return false;
    if (m_rotation.skipHandoff(m_rotation_time))
    {
        m_crew_pending_administrative_handoff=true;
        configureCrewRolesLocked();
        m_state = m_state_before_handoff;
        m_arcade_state_machine.transitionTo(
            m_current_lap >= m_total_laps
                ? ARCADE_STATE_FINAL_LAP : ARCADE_STATE_RUNNING,
            "handoff_skipped", NULL);
        bumpVersionLocked();
        return true;
    }
    return false;
}

bool RelaySessionController::updateCrewStateForActionLocked(
    RelayCrewActionRequest* action,
    const RelayCrewActionDefinition& def,
    int* score_delta,
    std::string* effect_summary,
    std::string* reject_reason)
{
    int delta = def.m_score_value;
    std::string effect = action->m_type;
    std::string target = action->m_target_id.empty()
        ? "default" : action->m_target_id;

    if (action->m_type == "nav_select_route")
    {
        m_active_route_id = target;
        m_track_effects["route:selected"] = target;
        effect = "route_selected:" + target;
    }
    else if (action->m_type == "nav_mark_hazard")
    {
        m_track_effects["hazard:" + target] = "marked";
        effect = "hazard_marked:" + target;
    }
    else if (action->m_type == "nav_call_pace_note")
    {
        m_track_effects["pace_note"] = target;
        effect = "pace_note:" + target;
    }
    else if (action->m_type == "nav_reveal_shortcut")
    {
        const std::string key = "shortcut:" + target;
        const bool combo = m_track_effects[key] == "armed";
        m_track_effects[key] = combo ? "revealed_and_armed" : "revealed";
        if (combo)
        {
            delta += 200;
            m_crew_multiplier_x100 = std::min(300,
                m_crew_multiplier_x100 + 25);
        }
        effect = combo ? "shortcut_combo:" + target
                       : "shortcut_revealed:" + target;
    }
    else if (action->m_type == "nav_request_role")
    {
        m_track_effects["role_request"] = target;
        effect = "role_requested:" + target;
    }
    else if (action->m_type == "engineer_charge_boost")
    {
        const int amount = action->m_amount > 0 ? action->m_amount : 25;
        action->m_amount = amount;
        m_engineer_boost_charge =
            std::min(100, m_engineer_boost_charge + amount);
        m_engine_heat = std::min(100, m_engine_heat + amount / 3);
        delta += amount;
        effect = "boost_charge:" + std::to_string(m_engineer_boost_charge);
    }
    else if (action->m_type == "engineer_release_boost")
    {
        if (m_engineer_boost_charge <= 0)
        {
            if (reject_reason)
                *reject_reason = "boost_not_charged";
            return false;
        }
        action->m_amount = m_engineer_boost_charge;
        delta += m_engineer_boost_charge * 2;
        m_engine_heat = std::min(100, m_engine_heat +
            std::max(10, m_engineer_boost_charge / 3));
        m_engineer_boost_charge = 0;
        effect = "boost_released:" + std::to_string(action->m_amount);
    }
    else if (action->m_type == "engineer_dump_heat")
    {
        const int dumped = std::min(45, m_engine_heat);
        m_engine_heat = std::max(0, m_engine_heat - dumped);
        delta += dumped;
        effect = "heat_dumped:" + std::to_string(dumped);
    }
    else if (action->m_type == "engineer_stabilize_grip")
    {
        m_engine_heat = std::max(0, m_engine_heat - 8);
        effect = "grip_stabilized";
    }
    else if (action->m_type == "engineer_rebalance_power")
    {
        if (action->m_mode.empty())
            action->m_mode = "balanced";
        m_track_effects["power_balance"] = action->m_mode;
        effect = "power_rebalanced:" + action->m_mode;
    }
    else if (action->m_type == "defender_raise_shield")
    {
        m_armor = std::min(100, m_armor + 3);
        effect = "shield_raised:" + target;
    }
    else if (action->m_type == "defender_cleanse_status")
    {
        m_track_effects["status:" + target] = "cleansed";
        effect = "status_cleansed:" + target;
    }
    else if (action->m_type == "defender_absorb_hit")
    {
        m_armor = std::min(100, m_armor + 5);
        effect = "hit_absorbed:" + target;
    }
    else if (action->m_type == "defender_repair_armor_small")
    {
        const int before = m_armor;
        m_armor = std::min(100, m_armor + 15);
        delta += m_armor - before;
        effect = "armor:" + std::to_string(m_armor);
    }
    else if (action->m_type == "defender_emergency_bubble")
    {
        m_armor = std::max(0, m_armor - 5);
        effect = "emergency_bubble";
    }
    else if (action->m_type == "hacker_open_gate")
    {
        m_track_effects["gate:" + target] = "open";
        effect = "sync_impulse:" + target;
    }
    else if (action->m_type == "hacker_close_hazard_barrier")
    {
        m_track_effects["barrier:" + target] = "closed";
        effect = "protection_impulse:" + target;
    }
    else if (action->m_type == "hacker_rotate_ramp")
    {
        const std::string mode = action->m_mode.empty()
            ? "attack" : action->m_mode;
        m_track_effects["ramp:" + target] = mode;
        effect = "power_impulse:" + target + ":" + mode;
    }
    else if (action->m_type == "hacker_arm_shortcut")
    {
        const std::string key = "shortcut:" + target;
        const bool combo = m_track_effects[key] == "revealed";
        m_track_effects[key] = combo ? "revealed_and_armed" : "armed";
        if (combo)
        {
            delta += 200;
            m_crew_multiplier_x100 = std::min(300,
                m_crew_multiplier_x100 + 25);
        }
        effect = combo ? "team_bonus_combo:" + target
                       : "team_bonus_ready:" + target;
    }
    else if (action->m_type == "hacker_disable_trap")
    {
        m_track_effects["trap:" + target] = "disabled";
        effect = "trap_disabled:" + target;
    }
    else if (action->m_type == "gunner_lock_target")
    {
        m_track_effects["target:" + target] = "locked";
        effect = "target_locked:" + target;
    }
    else if (action->m_type == "gunner_fire_utility")
    {
        const std::string key = "target:" + target;
        if (m_track_effects[key] == "locked")
            delta += 80;
        m_track_effects[key] = "cleared";
        effect = "target_cleared:" + target;
    }
    else if (action->m_type == "gunner_emp_zone")
    {
        m_track_effects["zone:" + target] = "emp";
        effect = "emp_zone:" + target;
    }
    else if (action->m_type == "gunner_drop_decoy")
    {
        m_track_effects["decoy"] = "active";
        effect = "decoy_active";
    }
    else if (action->m_type == "gunner_tag_rival")
    {
        m_track_effects["rival:" + target] = "tagged";
        effect = "rival_tagged:" + target;
    }

    if ((m_crew_actions_accepted + 1) % 5 == 0)
        m_crew_multiplier_x100 = std::min(300, m_crew_multiplier_x100 + 10);

    const int scored = delta * m_crew_multiplier_x100 / 100;
    m_crew_score += scored;
    m_crew_actions_accepted++;
    m_last_crew_event = action->m_type;
    m_last_crew_effect = effect;

    if (score_delta)
        *score_delta = scored;
    if (effect_summary)
        *effect_summary = effect;
    return true;
}

int RelaySessionController::nowMsForApiLocked() const
{
    static const std::chrono::steady_clock::time_point start =
        std::chrono::steady_clock::now();
    const std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    const long long elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start).count();
    return elapsed > 2147483000LL ? 2147483000 : (int)elapsed;
}

std::string RelaySessionController::idempotencyKeyForAction(
    const RelayCrewActionRequest& request) const
{
    return request.m_idempotency_key;
}

std::string RelaySessionController::idempotencySignatureForAction(
    const RelayCrewActionRequest& request,
    const RelayCrewActionDefinition& definition) const
{
    std::ostringstream out;
    out << request.m_session_id << "|";
    out << request.m_player_id << "|";
    out << CrewRoleManager::roleToString(definition.m_role) << "|";
    out << request.m_type << "|";
    out << request.m_target_id << "|";
    out << request.m_mode << "|";
    out << request.m_amount << "|";
    out << request.m_duration_ms;
    return out.str();
}

bool RelaySessionController::isRateLimitedLocked(
    const RelayCrewActionRequest& request,
    const RelayCrewActionDefinition& definition,
    int now_ms, std::string* reject_reason, int* retry_ms)
{
    const std::string role =
        CrewRoleManager::roleToString(definition.m_role);
    const std::string keys[] = {
        "player|" + request.m_player_id,
        "role|" + request.m_player_id + "|" + role,
        "action|" + request.m_player_id + "|" + role + "|" + request.m_type
    };

    int retry = 0;
    for (unsigned int i = 0; i < 3; i++)
    {
        std::map<std::string, int>::const_iterator ready_at =
            m_rate_ready_at_ms.find(keys[i]);
        if (ready_at != m_rate_ready_at_ms.end() &&
            ready_at->second > now_ms)
        {
            retry = std::max(retry, ready_at->second - now_ms);
        }
    }

    if (retry <= 0)
        return false;

    if (reject_reason)
        *reject_reason = "crew_action_rate_limited";
    if (retry_ms)
        *retry_ms = retry;
    return true;
}

void RelaySessionController::markRateLimitLocked(
    const RelayCrewActionRequest& request,
    const RelayCrewActionDefinition& definition,
    int now_ms)
{
    const std::string role =
        CrewRoleManager::roleToString(definition.m_role);
    m_rate_ready_at_ms["player|" + request.m_player_id] =
        now_ms + 180;
    m_rate_ready_at_ms["role|" + request.m_player_id + "|" + role] =
        now_ms + 260;
    m_rate_ready_at_ms["action|" + request.m_player_id + "|" + role + "|" +
                       request.m_type] = now_ms + 120;
}

void RelaySessionController::pushHistoryLocked(
    std::vector<RelayCrewActionHistory>* history,
    const RelayCrewActionHistory& entry) const
{
    history->push_back(entry);
    while (history->size() > 20)
        history->erase(history->begin());
}

void RelaySessionController::recordAcceptedActionLocked(
    const RelayCrewActionRequest& action, const RelayCrewActionAck& ack)
{
    RelayCrewActionHistory entry;
    entry.m_player_id = action.m_player_id;
    entry.m_role = action.m_role;
    entry.m_type = action.m_type;
    entry.m_action_id = action.m_action_id;
    entry.m_idempotency_key = idempotencyKeyForAction(action);
    entry.m_result = ack.m_event_name;
    entry.m_effect_summary = ack.m_effect_summary;
    entry.m_score_delta = ack.m_score_delta;
    entry.m_cooldown_ms = ack.m_cooldown_ms;
    entry.m_state_version = ack.m_state_version;
    pushHistoryLocked(&m_last_accepted_actions, entry);
}

void RelaySessionController::recordRejectedActionLocked(
    const RelayCrewActionRequest& request, const std::string& reason,
    int cooldown_ms)
{
    RelayCrewActionHistory entry;
    entry.m_player_id = request.m_player_id;
    entry.m_role = request.m_role;
    entry.m_type = request.m_type;
    entry.m_action_id = request.m_action_id;
    entry.m_idempotency_key = idempotencyKeyForAction(request);
    entry.m_result = "crew_action_rejected";
    entry.m_reason = reason;
    entry.m_cooldown_ms = cooldown_ms;
    entry.m_state_version = m_state_version;
    pushHistoryLocked(&m_last_rejected_actions, entry);
    bumpVersionLocked();
}

void RelaySessionController::annotatePhoneStatesLocked(
    std::vector<RelayCrewPhoneState>* states, int now_ms) const
{
    const bool session_running =
        m_state == RELAY_STATE_RUNNING ||
        m_state == RELAY_STATE_LAST_MINUTE ||
        m_state == RELAY_STATE_FINAL_SPRINT;

    for (unsigned int i = 0; i < states->size(); i++)
    {
        RelayCrewPhoneState& state = (*states)[i];
        for (unsigned int a = 0; a < state.m_actions.size(); a++)
        {
            RelayCrewPhoneAction& action = state.m_actions[a];
            const RelayCrewActionDefinition* definition =
                CrewRoleManager::getActionDefinition(action.m_type);
            if (definition)
                action.m_role = definition->m_role;

            action.m_cooldown_ms = m_crew_roles.getCooldownRemainingMs(
                state.m_player_id, action.m_type, now_ms);
            if (isContinuousCrewAction(action.m_type))
                action.m_unavailable_reason.clear();

            if (m_paused)
            {
                action.m_available = false;
                action.m_unavailable_reason = "session_paused";
            }
            else if (m_state == RELAY_STATE_HANDOFF)
            {
                action.m_available = false;
                action.m_unavailable_reason =
                    "crew_actions_locked_during_handoff";
            }
            else if (!session_running)
            {
                action.m_available = false;
                action.m_unavailable_reason = "session_not_running";
            }
            else if (action.m_cooldown_ms > 0)
            {
                action.m_available = false;
                action.m_unavailable_reason = "cooldown";
            }
            else if (m_active_config.m_ai_karts == 0 &&
                     (action.m_type == "gunner_fire_utility" || action.m_type == "gunner_emp_zone"))
            {
                action.m_available = false;
                action.m_unavailable_reason = "no_opponents";
            }
            else if (action.m_type == "engineer_release_boost" &&
                     m_engineer_boost_charge <= 0)
            {
                action.m_available = false;
                action.m_unavailable_reason = "boost_not_charged";
            }
            else if ((action.m_type == "hacker_open_gate" && (now_ms % 8000) >= 4000) ||
                     (action.m_type == "hacker_disable_trap" && (now_ms % 8000) < 4000))
            {
                action.m_available = false;
                action.m_unavailable_reason = "sync_window";
            }
            else if (action.m_unavailable_reason.empty())
            {
                action.m_available = true;
            }
        }
    }
}

std::vector<RelayCrewPhoneState>
RelaySessionController::buildPhoneStatesLocked(
    const std::vector<RelayCrewRoleAssignment>& assignments,
    int now_ms) const
{
    if (m_active_config.m_crew_mode != "enabled")
        return {};
    std::vector<RelayCrewPhoneState> states =
        m_event_director.getPhoneStates(assignments, m_engineer_boost_charge,
                                        m_engine_heat, m_armor);

    for (unsigned int i = 0; i < assignments.size(); i++)
    {
        const RelayCrewRoleAssignment& assignment = assignments[i];
        if (assignment.m_is_pilot && assignments.size() != 1)
            continue;

        for (unsigned int r = 0; r < assignment.m_roles.size(); r++)
        {
            const RelayCrewRole role = assignment.m_roles[r];
            RelayCrewPhoneState* target_state = NULL;
            for (unsigned int s = 0; s < states.size(); s++)
            {
                if (phoneStateMatches(states[s], assignment.m_player_id, role))
                {
                    target_state = &states[s];
                    break;
                }
            }

            if (!target_state)
            {
                RelayCrewPhoneState state;
                state.m_player_id = assignment.m_player_id;
                state.m_role = role;
                state.m_title = roleTitle(role);
                state.m_lap = m_current_lap;
                state.m_total_laps = m_total_laps;
                state.m_active_driver_index = m_rotation.getActiveIndex();
                const RelayPilotRotationSnapshot rotation =
                    m_rotation.getSnapshot(m_rotation_time);
                state.m_active_driver_id = rotation.m_active_player_id;
                state.m_message = "Stand by";
                state.m_resource_text =
                    "Turbo " + std::to_string(m_engineer_boost_charge) + "% · Calore " +
                    std::to_string(m_engine_heat) + "% · Protezione " + std::to_string(m_armor) + "%";
                state.m_remaining_ms = 0;
                state.m_primary = false;
                states.push_back(state);
                target_state = &states.back();
            }
        }
    }

    // Ordinary actions remain accessible in every role; encounter actions are
    // presented first but never hide a role's ordinary activity.
    for (RelayCrewPhoneState& state : states)
    {
        if (state.m_message == "Stand by" || state.m_message == "Hold for your window")
            state.m_message = "Aiuta la squadra durante il turno di guida";
        for (const RelayCrewActionDefinition& definition :
             CrewRoleManager::getActionDefinitionsSnapshot())
        {
            if (definition.m_role != state.m_role ||
                !isContinuousCrewAction(definition.m_type) ||
                phoneStateHasAction(state, definition.m_type))
                continue;
            RelayCrewPhoneAction action = makeDefaultPhoneAction(definition);
            action.m_label = crewActionLabel(definition.m_type);
            state.m_actions.push_back(action);
        }
    }

    annotatePhoneStatesLocked(&states, now_ms);
    // All roles must remain usable on a phone holding several roles.
    return states;
}

bool RelaySessionController::submitCrewAction(
    const RelayCrewActionRequest& request,
    RelayCrewActionAck* ack,
    std::string* reject_reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (ack)
    {
        *ack = RelayCrewActionAck();
        ack->m_state_version = m_state_version;
        ack->m_total_score = m_crew_score;
    }

    if (!m_runtime_enabled)
    {
        if (reject_reason)
            *reject_reason = "relay_runtime_not_enabled";
        recordRejectedActionLocked(request, "relay_runtime_not_enabled", 0);
        return false;
    }
    if (!m_race_attached)
    {
        if (reject_reason)
            *reject_reason = "race_not_attached";
        recordRejectedActionLocked(request, "race_not_attached", 0);
        return false;
    }
    if (m_paused)
    {
        if (reject_reason)
            *reject_reason = "session_paused";
        recordRejectedActionLocked(request, "session_paused", 0);
        return false;
    }
    if (m_state == RELAY_STATE_HANDOFF)
    {
        if (reject_reason)
            *reject_reason = "crew_actions_locked_during_handoff";
        recordRejectedActionLocked(request,
            "crew_actions_locked_during_handoff", 0);
        return false;
    }
    if (m_state != RELAY_STATE_RUNNING &&
        m_state != RELAY_STATE_LAST_MINUTE &&
        m_state != RELAY_STATE_FINAL_SPRINT)
    {
        if (reject_reason)
            *reject_reason = "session_not_running";
        recordRejectedActionLocked(request, "session_not_running", 0);
        return false;
    }
    if (m_active_config.m_crew_mode != "enabled")
    {
        if (reject_reason)
            *reject_reason = "crew_mode_disabled";
        recordRejectedActionLocked(request, "crew_mode_disabled", 0);
        return false;
    }
    if (request.m_session_id.empty())
    {
        if (reject_reason)
            *reject_reason = "missing_session_id";
        recordRejectedActionLocked(request, "missing_session_id", 0);
        return false;
    }
    if (request.m_session_id != m_active_config.m_session_id)
    {
        if (reject_reason)
            *reject_reason = "wrong_session_id";
        recordRejectedActionLocked(request, "wrong_session_id", 0);
        return false;
    }
    if (request.m_player_id.empty())
    {
        if (reject_reason)
            *reject_reason = "missing_player_id";
        recordRejectedActionLocked(request, "missing_player_id", 0);
        return false;
    }
    if (request.m_type.empty())
    {
        if (reject_reason)
            *reject_reason = "missing_action_type";
        recordRejectedActionLocked(request, "missing_action_type", 0);
        return false;
    }
    if (idempotencyKeyForAction(request).empty())
    {
        if (reject_reason)
            *reject_reason = "missing_idempotency_key";
        recordRejectedActionLocked(request, "missing_idempotency_key", 0);
        return false;
    }
    if (request.m_state_version_seen == 0)
    {
        if (reject_reason)
            *reject_reason = "missing_state_version";
        recordRejectedActionLocked(request, "missing_state_version", 0);
        return false;
    }
    if (request.m_state_version_seen > m_state_version + 1)
    {
        if (reject_reason)
            *reject_reason = "future_state_version";
        recordRejectedActionLocked(request, "future_state_version", 0);
        return false;
    }
    if (request.m_state_version_seen + 3 < m_state_version)
    {
        if (reject_reason)
            *reject_reason = "stale_state";
        recordRejectedActionLocked(request, "stale_state", 0);
        return false;
    }

    const RelayCrewActionDefinition* found_definition =
        CrewRoleManager::getActionDefinition(request.m_type);
    if (!found_definition)
    {
        if (reject_reason)
            *reject_reason = "unknown_crew_action";
        recordRejectedActionLocked(request, "unknown_crew_action", 0);
        return false;
    }

    RelayCrewActionDefinition definition = *found_definition;
    RelayCrewActionRequest action = request;
    if (!action.m_role_name.empty() &&
        action.m_role == RELAY_CREW_ROLE_UNKNOWN)
    {
        if (reject_reason)
            *reject_reason = "unknown_crew_role";
        recordRejectedActionLocked(action, "unknown_crew_role", 0);
        return false;
    }
    if (action.m_role == RELAY_CREW_ROLE_UNKNOWN)
        action.m_role = definition.m_role;
    if (action.m_role_name.empty())
        action.m_role_name = CrewRoleManager::roleToString(action.m_role);

    if (action.m_role != definition.m_role)
    {
        if (reject_reason)
            *reject_reason = "wrong_role";
        recordRejectedActionLocked(action, "wrong_role", 0);
        return false;
    }

    const std::string idempotency_key = idempotencyKeyForAction(action);
    const std::string idempotency_lookup =
        m_active_config.m_session_id + "|" + action.m_player_id + "|" +
        idempotency_key;
    const std::string idempotency_signature =
        idempotencySignatureForAction(action, definition);
    std::map<std::string, std::string>::const_iterator seen_signature =
        m_idempotency_signatures.find(idempotency_lookup);
    if (seen_signature != m_idempotency_signatures.end())
    {
        if (seen_signature->second != idempotency_signature)
        {
            if (reject_reason)
                *reject_reason = "idempotency_key_reused";
            recordRejectedActionLocked(action, "idempotency_key_reused", 0);
            return false;
        }

        if (ack)
        {
            std::map<std::string, RelayCrewActionAck>::const_iterator
                seen_ack = m_idempotency_acks.find(idempotency_lookup);
            if (seen_ack != m_idempotency_acks.end())
                *ack = seen_ack->second;
            ack->m_accepted = false;
            ack->m_duplicate = true;
            ack->m_event_name = "duplicate_ignored";
            ack->m_score_delta = 0;
            ack->m_total_score = m_crew_score;
            ack->m_state_version = m_state_version;
            ack->m_idempotency_key = idempotency_key;
        }
        return true;
    }

    const int cooldown_now_ms = (int)(m_rotation_time * 1000.0f);
    const int api_now_ms = nowMsForApiLocked();
    int cooldown_remaining_ms = 0;
    std::string validation_error;
    if (!m_crew_roles.validateAction(action, m_state_version,
                                     cooldown_now_ms,
                                     &validation_error,
                                     &cooldown_remaining_ms,
                                     &definition))
    {
        if (reject_reason)
            *reject_reason = validation_error;
        if (ack)
            ack->m_cooldown_ms = cooldown_remaining_ms;
        recordRejectedActionLocked(action, validation_error,
                                   cooldown_remaining_ms);
        return false;
    }

    int retry_ms = 0;
    std::string rate_error;
    if (isRateLimitedLocked(action, definition, api_now_ms, &rate_error,
                            &retry_ms))
    {
        if (reject_reason)
            *reject_reason = rate_error;
        if (ack)
            ack->m_cooldown_ms = retry_ms;
        recordRejectedActionLocked(action, rate_error, retry_ms);
        return false;
    }

    if (action.m_action_id.empty())
        action.m_action_id = action.m_player_id + ":" + action.m_type + ":" +
            std::to_string(m_crew_actions_accepted + 1);

    const bool continuous_resource = isContinuousCrewAction(action.m_type);
    std::string director_error;
    const bool director_action = m_event_director.validateAction(action, &director_error);
    if (!director_action && !continuous_resource)
    {
        if (reject_reason)
            *reject_reason = director_error;
        recordRejectedActionLocked(action, director_error, 0);
        return false;
    }

    if (!director_action &&
        ((action.m_type == "hacker_open_gate" && (cooldown_now_ms % 8000) >= 4000) ||
         (action.m_type == "hacker_disable_trap" && (cooldown_now_ms % 8000) < 4000)))
    {
        if (reject_reason) *reject_reason = "sync_window";
        recordRejectedActionLocked(action, "sync_window", 0);
        return false;
    }
    if (action.m_amount < 0 || action.m_amount > 100)
    {
        if (reject_reason) *reject_reason = "invalid_amount";
        recordRejectedActionLocked(action, "invalid_amount", 0);
        return false;
    }
    if (m_active_config.m_ai_karts == 0 &&
        (action.m_type == "gunner_fire_utility" || action.m_type == "gunner_emp_zone"))
    {
        if (reject_reason) *reject_reason = "no_opponents";
        recordRejectedActionLocked(action, "no_opponents", 0);
        return false;
    }
    markRateLimitLocked(action, definition, api_now_ms);

    int scored = 0;
    std::string effect_summary;
    std::string state_error;
    if (!updateCrewStateForActionLocked(&action, definition, &scored,
                                        &effect_summary, &state_error))
    {
        if (reject_reason)
            *reject_reason = state_error;
        recordRejectedActionLocked(action, state_error, 0);
        return false;
    }

    std::string encounter_effect;
    const int encounter_bonus =
        director_action ? m_event_director.markActionAccepted(action, &encounter_effect) : 0;
    if (encounter_bonus > 0)
    {
        scored += encounter_bonus;
        m_crew_score += encounter_bonus;
        if (!encounter_effect.empty())
            effect_summary += "|" + encounter_effect;
        m_last_crew_effect = effect_summary;
        consumeDirectorTrackEffectsLocked();
    }

    m_pending_crew_actions.push_back(action);
    m_crew_roles.markActionAccepted(action, cooldown_now_ms);
    m_actions_accepted_by_role[action.m_role]++;
    m_score_by_role[action.m_role] += scored;
    bumpVersionLocked();

    if (ack)
    {
        ack->m_accepted = true;
        ack->m_event_name = "crew_action_accepted";
        ack->m_effect_summary = effect_summary;
        ack->m_idempotency_key = idempotency_key;
        ack->m_score_delta = scored;
        ack->m_total_score = m_crew_score;
        ack->m_cooldown_ms = definition.m_cooldown_ms;
        ack->m_state_version = m_state_version;
        m_idempotency_signatures[idempotency_lookup] =
            idempotency_signature;
        m_idempotency_acks[idempotency_lookup] = *ack;
        recordAcceptedActionLocked(action, *ack);
    }
    else
    {
        RelayCrewActionAck stored_ack;
        stored_ack.m_accepted = true;
        stored_ack.m_event_name = "crew_action_accepted";
        stored_ack.m_effect_summary = effect_summary;
        stored_ack.m_idempotency_key = idempotency_key;
        stored_ack.m_score_delta = scored;
        stored_ack.m_total_score = m_crew_score;
        stored_ack.m_cooldown_ms = definition.m_cooldown_ms;
        stored_ack.m_state_version = m_state_version;
        m_idempotency_signatures[idempotency_lookup] =
            idempotency_signature;
        m_idempotency_acks[idempotency_lookup] = stored_ack;
        recordAcceptedActionLocked(action, stored_ack);
    }

    Log::info("ImmersiveRelay",
              "Crew action accepted player=%s type=%s score=%d total=%d.",
              action.m_player_id.c_str(), action.m_type.c_str(),
              scored, m_crew_score);
    return true;
}

std::vector<RelayCrewActionRequest>
RelaySessionController::drainPendingCrewActions()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<RelayCrewActionRequest> actions = m_pending_crew_actions;
    m_pending_crew_actions.clear();
    return actions;
}

std::string RelaySessionController::consumeCrewDomeCallout()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_event_director.consumeDomeCallout();
}

bool RelaySessionController::isHumanInputLocked() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return isWorldHeldLocked();
}

std::vector<std::string> RelaySessionController::getSpatialCrewIDs() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::string driver = m_rotation.getSnapshot(m_rotation_time).m_active_player_id;
    std::vector<std::string> ids;
    for (const RelayPlayer& player : m_players)
        if (player.m_player_id != driver) ids.push_back(player.m_player_id);
    return ids;
}

std::vector<RelayPlayer> RelaySessionController::getSpatialParticipants() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_players;
}

RelaySessionSnapshot RelaySessionController::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    RelaySessionSnapshot s;
    s.m_runtime_enabled = m_runtime_enabled;
    s.m_race_attached = m_race_attached;
    s.m_loaded_track_id = m_loaded_track_id;
    s.m_world_elapsed_ms = m_world_elapsed_ms;
    s.m_input_locked = !m_presentation_ready || !m_scintilla_start_permitted || missionBlocksGameplayLocked() ||
        m_crew_handoff_ready_required || m_paused || m_rotation.isHandoffActive() ||
        (m_operator_controlled && !isSessionPlayableStateLocked());
    s.m_paused = m_paused;
    s.m_presentation_ready = m_presentation_ready;
    s.m_mission_required = m_mission_required;
    s.m_mission_presentation_held = m_mission_presentation_held;
    s.m_state = m_state;
    s.m_state_version = m_state_version;
    s.m_session_id = m_active_config.m_session_id;
    s.m_preset = m_active_config.m_preset;
    s.m_track_id = m_active_config.m_track_id;
    s.m_gameplay_seconds = m_active_config.m_gameplay_seconds;
    s.m_slot_seconds = m_active_config.m_slot_seconds;
    s.m_player_count = (int)m_players.size();
    s.m_arcade_ui = m_active_config.m_arcade_ui;
    s.m_race_laps = m_total_laps;
    s.m_current_lap = m_current_lap;
    s.m_total_laps = m_total_laps;
    s.m_lap_progress = m_lap_progress;
    s.m_current_checkpoint = m_current_checkpoint;
    s.m_track_distance = m_track_distance;
    s.m_race_phase = m_race_phase;
    s.m_arcade_configured = m_arcade_configured;
    s.m_arcade_state = m_arcade_state_machine.getState();
    s.m_arcade_transition_count =
        m_arcade_state_machine.getTransitionCount();
    s.m_arcade_last_error = m_arcade_state_machine.getLastError();
    s.m_arcade_last_error_recoverable =
        m_arcade_state_machine.isLastErrorRecoverable();
    s.m_kart_id = m_active_config.m_kart_id;
    s.m_difficulty = m_active_config.m_difficulty;
    s.m_ai_karts = m_active_config.m_ai_karts;
    s.m_lap_policy = m_active_config.m_lap_policy;
    s.m_crew_mode = m_active_config.m_crew_mode;
    s.m_encounter_script = m_active_config.m_encounter_script;
    s.m_render_profile = m_active_config.m_render_profile;
    s.m_output = m_active_config.m_output;
    s.m_projection = m_active_config.m_projection;
    s.m_operator_mode = m_active_config.m_operator_mode;
    s.m_allow_pause = m_active_config.m_allow_pause;
    s.m_allow_force_finish = m_active_config.m_allow_force_finish;
    s.m_allow_abort = m_active_config.m_allow_abort;
    s.m_require_handoff_confirm =
        m_active_config.m_require_handoff_confirm;
    s.m_auto_confirm_handoff = m_active_config.m_auto_confirm_handoff;
    s.m_handoff_seconds = m_active_config.m_handoff_seconds;
    s.m_min_handoff_seconds = m_active_config.m_min_handoff_seconds;
    s.m_music_enabled = m_active_config.m_music_enabled;
    s.m_sfx_enabled = m_active_config.m_sfx_enabled;
    s.m_music_volume = m_active_config.m_music_volume;
    s.m_sfx_volume = m_active_config.m_sfx_volume;
    s.m_connected_player_ids = m_connected_player_ids;
    s.m_briefing_seconds = m_active_config.m_briefing_seconds;
    s.m_countdown_seconds = m_active_config.m_countdown_seconds;
    s.m_countdown_remaining_ms = m_countdown_remaining_ms;
    s.m_can_start = m_arcade_configured && !missionBlocksGameplayLocked() &&
        (m_race_attached || m_active_config.m_test_accelerated_progress) &&
        (s.m_arcade_state == ARCADE_STATE_READY_FOR_PLAYERS ||
         s.m_arcade_state == ARCADE_STATE_SESSION_CONFIGURED ||
         s.m_arcade_state == ARCADE_STATE_RESULTS ||
         (m_crew_handoff_ready_required && m_crew_start_permitted && m_presentation_ready &&
          m_scintilla_start_permitted && !m_rotation.isHandoffActive() &&
          (s.m_arcade_state == ARCADE_STATE_RUNNING || s.m_arcade_state == ARCADE_STATE_FINAL_LAP ||
           s.m_arcade_state == ARCADE_STATE_PAUSED)));
    s.m_can_pause = m_active_config.m_allow_pause &&
        (s.m_arcade_state == ARCADE_STATE_RUNNING ||
         s.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
         s.m_arcade_state == ARCADE_STATE_FINAL_LAP ||
         s.m_arcade_state == ARCADE_STATE_COUNTDOWN);
    s.m_can_resume = s.m_arcade_state == ARCADE_STATE_PAUSED && !missionBlocksGameplayLocked();
    s.m_can_reset = s.m_arcade_state != ARCADE_STATE_RESETTING &&
        s.m_arcade_state != ARCADE_STATE_ERROR_FATAL &&
        s.m_arcade_state != ARCADE_STATE_SHUTDOWN;
    s.m_can_abort = m_active_config.m_allow_abort &&
        (s.m_arcade_state == ARCADE_STATE_BRIEFING ||
         s.m_arcade_state == ARCADE_STATE_COUNTDOWN ||
         s.m_arcade_state == ARCADE_STATE_RUNNING ||
         s.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
         s.m_arcade_state == ARCADE_STATE_PAUSED ||
         s.m_arcade_state == ARCADE_STATE_FINAL_LAP);
    s.m_can_force_finish = m_active_config.m_allow_force_finish &&
        (s.m_arcade_state == ARCADE_STATE_RUNNING ||
         s.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
         s.m_arcade_state == ARCADE_STATE_PAUSED ||
         s.m_arcade_state == ARCADE_STATE_FINAL_LAP);
    s.m_report = m_last_report;
    s.m_test_accelerated_progress =
        m_active_config.m_test_accelerated_progress;
    s.m_elapsed_ms = (int)(m_last_race_time * 1000.0f);
    s.m_remaining_ms = 0;
    s.m_handoff_policy = m_active_config.m_handoff_policy;
    s.m_rotation = m_rotation.getSnapshot(m_rotation_time);
    s.m_crew_score = m_crew_score;
    s.m_crew_multiplier_x100 = m_crew_multiplier_x100;
    s.m_engineer_boost_charge = m_engineer_boost_charge;
    s.m_engine_heat = m_engine_heat;
    s.m_armor = m_armor;
    s.m_crew_actions_accepted = m_crew_actions_accepted;
    s.m_encounters_started = m_event_director.getEncountersStarted();
    s.m_encounters_succeeded = m_event_director.getEncountersSucceeded();
    s.m_encounters_failed = m_event_director.getEncountersFailed();
    s.m_combo_count = m_event_director.getComboCount();
    s.m_active_route_id = m_active_route_id;
    s.m_last_crew_event = m_last_crew_event;
    s.m_last_crew_effect = m_last_crew_effect;
    if (!m_last_accepted_actions.empty())
    {
        const RelayCrewActionHistory& feedback = m_last_accepted_actions.back();
        s.m_crew_feedback_id = m_active_config.m_session_id + ":" + feedback.m_idempotency_key;
        s.m_crew_feedback_message = crewActionLabel(feedback.m_type) + ": riuscito";
        if (feedback.m_type == "engineer_charge_boost")
            s.m_crew_feedback_message = "Turbo caricato: " + std::to_string(m_engineer_boost_charge) + "%";
        else if (feedback.m_type == "engineer_release_boost")
            s.m_crew_feedback_message = "Turbo attivato!";
        if (feedback.m_effect_summary.find("|combo:") != std::string::npos)
            s.m_crew_feedback_message = "Combo della squadra!";
        else if (feedback.m_effect_summary.find("encounter_success:") != std::string::npos)
            s.m_crew_feedback_message = "Sfida completata! " + crewActionLabel(feedback.m_type);
    }
    s.m_crew_assignments = m_crew_roles.getAssignmentsSnapshot();
    for (std::map<std::string, std::string>::const_iterator i =
             m_track_effects.begin(); i != m_track_effects.end(); i++)
    {
        s.m_track_effects.push_back(RelayCrewTrackEffect(i->first,
                                                        i->second));
    }
    const int now_ms = (int)(m_rotation_time * 1000.0f);
    s.m_cooldowns = m_crew_roles.getCooldownsSnapshot(now_ms);
    s.m_last_accepted_actions = m_last_accepted_actions;
    s.m_last_rejected_actions = m_last_rejected_actions;
    s.m_pilot_turns_by_player = m_pilot_turns_by_player;
    s.m_actions_accepted_by_role = m_actions_accepted_by_role;
    s.m_score_by_role = m_score_by_role;
    s.m_adaptive_difficulty_config = m_active_config.m_adaptive_difficulty;
    s.m_adaptive_difficulty = m_adaptive_difficulty;
    s.m_encounter = m_event_director.getSnapshot();
    s.m_phone_states = buildPhoneStatesLocked(s.m_crew_assignments, now_ms);
    return s;
}

#endif // ENABLE_IMMERSIVE_RELAY
