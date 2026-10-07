//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_SESSION_CONTROLLER_HPP
#define HEADER_RELAY_SESSION_CONTROLLER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_role_manager.hpp"
#include "modes/immersive_relay/crew_event_director.hpp"
#include "modes/immersive_relay/arcade_session_state_machine.hpp"
#include "modes/immersive_relay/relay_difficulty_director.hpp"
#include "modes/immersive_relay/relay_pilot_rotation.hpp"

#include <map>
#include <mutex>
#include <string>
#include <vector>

enum RelaySessionState
{
    RELAY_STATE_ATTRACT,
    RELAY_STATE_LOADING,
    RELAY_STATE_READY_FOR_PLAYERS,
    RELAY_STATE_BRIEFING,
    RELAY_STATE_COUNTDOWN,
    RELAY_STATE_RUNNING,
    RELAY_STATE_HANDOFF,
    RELAY_STATE_LAST_MINUTE,
    RELAY_STATE_FINAL_SPRINT,
    RELAY_STATE_FINISHED_GOLD,
    RELAY_STATE_FINISHED_SILVER,
    RELAY_STATE_FINISHED_BRONZE,
    RELAY_STATE_FINISHED_TIMEOUT,
    RELAY_STATE_FINISHED_OPERATOR_ABORT,
    RELAY_STATE_RESULTS,
    RELAY_STATE_RESETTING,
    RELAY_STATE_ERROR_RECOVERABLE,
    RELAY_STATE_ERROR_FATAL
};

struct RelaySessionConfig
{
    std::string m_session_id;
    std::string m_preset;
    std::string m_track_id;
    std::string m_kart_id;
    std::string m_difficulty;
    int m_ai_karts;
    std::string m_lap_policy;
    std::string m_crew_mode;
    std::string m_encounter_script;
    std::string m_render_profile;
    std::string m_output;
    std::string m_projection;
    std::string m_operator_mode;
    bool m_allow_pause;
    bool m_allow_force_finish;
    bool m_allow_abort;
    bool m_require_handoff_confirm;
    bool m_auto_confirm_handoff;
    bool m_music_enabled;
    bool m_sfx_enabled;
    float m_music_volume;
    float m_sfx_volume;
    int m_briefing_seconds;
    int m_countdown_seconds;
    int m_player_count;
    bool m_arcade_ui;
    bool m_test_accelerated_progress;
    int m_gameplay_seconds;
    int m_slot_seconds;
    RelayHandoffPolicy m_handoff_policy;
    float m_handoff_seconds;
    float m_min_handoff_seconds;
    float m_handoff_timeout_seconds;
    float m_handoff_warning_seconds;
    float m_segment_seconds;
    float m_final_sprint_no_handoff_seconds;
    RelayDifficultyConfig m_adaptive_difficulty;

    RelaySessionConfig();
};

struct RelaySessionReport
{
    bool m_available;
    std::string m_session_id;
    int m_player_count;
    std::string m_track_id;
    int m_total_laps;
    int m_completed_laps;
    std::string m_final_result;
    std::string m_rank;
    int m_score;
    int m_combo_count;
    int m_encounters_started;
    int m_encounters_succeeded;
    int m_encounters_failed;
    std::map<RelayCrewRole, int> m_actions_by_role;
    std::map<std::string, int> m_driver_turns_by_player;
    int m_duration_seconds;
    std::string m_render_profile;
    std::string m_projection;
    std::string m_output;
    bool m_crew_mode_enabled;
    bool m_crew_actions_enabled;
    int m_crew_actions_accepted;

    RelaySessionReport();
};

struct RelayLocalApiOptions
{
    bool m_enabled;
    std::string m_bind_host;
    int m_port;
    std::string m_token;

    RelayLocalApiOptions();
};

struct RelaySessionSnapshot
{
    bool m_runtime_enabled;
    bool m_race_attached;
    bool m_input_locked;
    std::string m_loaded_track_id;
    int m_world_elapsed_ms;
    bool m_paused;
    bool m_presentation_ready;
    RelaySessionState m_state;
    unsigned int m_state_version;
    std::string m_session_id;
    std::string m_preset;
    std::string m_track_id;
    int m_gameplay_seconds;
    int m_slot_seconds;
    int m_player_count;
    bool m_arcade_ui;
    int m_race_laps;
    int m_current_lap;
    int m_total_laps;
    float m_lap_progress;
    int m_current_checkpoint;
    float m_track_distance;
    std::string m_race_phase;
    bool m_arcade_configured;
    ArcadeSessionState m_arcade_state;
    unsigned int m_arcade_transition_count;
    std::string m_arcade_last_error;
    bool m_arcade_last_error_recoverable;
    std::string m_kart_id;
    std::string m_difficulty;
    int m_ai_karts;
    std::string m_lap_policy;
    std::string m_crew_mode;
    std::string m_encounter_script;
    std::string m_render_profile;
    std::string m_output;
    std::string m_projection;
    std::string m_operator_mode;
    bool m_allow_pause;
    bool m_allow_force_finish;
    bool m_allow_abort;
    bool m_require_handoff_confirm;
    bool m_auto_confirm_handoff;
    bool m_music_enabled;
    bool m_sfx_enabled;
    float m_music_volume;
    float m_sfx_volume;
    int m_briefing_seconds;
    int m_countdown_seconds;
    int m_countdown_remaining_ms;
    bool m_can_start;
    bool m_can_pause;
    bool m_can_resume;
    bool m_can_reset;
    bool m_can_abort;
    bool m_can_force_finish;
    RelaySessionReport m_report;
    bool m_test_accelerated_progress;
    int m_elapsed_ms;
    int m_remaining_ms;
    RelayHandoffPolicy m_handoff_policy;
    RelayPilotRotationSnapshot m_rotation;
    std::vector<std::string> m_connected_player_ids;
    float m_handoff_seconds;
    float m_min_handoff_seconds;
    int m_crew_score;
    int m_crew_multiplier_x100;
    int m_engineer_boost_charge;
    int m_engine_heat;
    int m_armor;
    int m_crew_actions_accepted;
    int m_encounters_started;
    int m_encounters_succeeded;
    int m_encounters_failed;
    int m_combo_count;
    std::string m_active_route_id;
    std::string m_last_crew_event;
    std::string m_last_crew_effect;
    std::string m_crew_feedback_id;
    std::string m_crew_feedback_message;
    std::vector<RelayCrewRoleAssignment> m_crew_assignments;
    std::vector<RelayCrewTrackEffect> m_track_effects;
    std::vector<RelayCrewCooldownSnapshot> m_cooldowns;
    std::vector<RelayCrewActionHistory> m_last_accepted_actions;
    std::vector<RelayCrewActionHistory> m_last_rejected_actions;
    std::map<std::string, int> m_pilot_turns_by_player;
    std::vector<int> m_recorded_pilot_laps;
    std::map<RelayCrewRole, int> m_actions_accepted_by_role;
    std::map<RelayCrewRole, int> m_score_by_role;
    RelayDifficultyConfig m_adaptive_difficulty_config;
    RelayDifficultySnapshot m_adaptive_difficulty;
    RelayCrewEncounterSnapshot m_encounter;
    std::vector<RelayCrewPhoneState> m_phone_states;
};

class RelaySessionController
{
private:
    static RelaySessionController* m_instance;

    mutable std::mutex m_mutex;
    bool m_runtime_enabled;
    bool m_race_attached;
    bool m_operator_controlled;
    bool m_resume_countdown;
    ArcadeSessionState m_state_before_pause;
    std::string m_loaded_track_id;
    int m_world_elapsed_ms;
    bool m_paused;
    bool m_presentation_ready;
    RelaySessionState m_state;
    RelaySessionState m_state_before_handoff;
    unsigned int m_state_version;
    RelaySessionConfig m_default_config;
    RelaySessionConfig m_active_config;
    RelayLocalApiOptions m_api_options;
    ArcadeSessionStateMachine m_arcade_state_machine;
    bool m_arcade_configured;
    int m_countdown_remaining_ms;
    float m_countdown_started_at;
    float m_session_started_at;
    float m_session_finished_at;
    RelaySessionReport m_last_report;
    std::vector<RelayPlayer> m_players;
    RelayPilotRotation m_rotation;
    CrewRoleManager m_crew_roles;
    std::vector<std::string> m_connected_player_ids;
    CrewEventDirector m_event_director;
    std::vector<RelayCrewActionRequest> m_pending_crew_actions;
    std::map<std::string, std::string> m_track_effects;
    std::map<std::string, std::string> m_idempotency_signatures;
    std::map<std::string, RelayCrewActionAck> m_idempotency_acks;
    std::map<std::string, int> m_rate_ready_at_ms;
    std::vector<RelayCrewActionHistory> m_last_accepted_actions;
    std::vector<RelayCrewActionHistory> m_last_rejected_actions;
    int m_crew_score;
    int m_crew_multiplier_x100;
    int m_engineer_boost_charge;
    int m_engine_heat;
    int m_armor;
    int m_crew_actions_accepted;
    int m_current_lap;
    int m_total_laps;
    float m_lap_progress;
    int m_current_checkpoint;
    float m_track_distance;
    std::string m_race_phase;
    std::map<std::string, int> m_pilot_turns_by_player;
    std::vector<int> m_recorded_pilot_laps;
    std::map<RelayCrewRole, int> m_actions_accepted_by_role;
    std::map<RelayCrewRole, int> m_score_by_role;
    RelayDifficultySnapshot m_adaptive_difficulty;
    std::string m_active_route_id;
    std::string m_last_crew_event;
    std::string m_last_crew_effect;
    float m_last_race_time;
    float m_rotation_time;
    bool m_last_safe_for_handoff;

    void bumpVersionLocked();
    bool transitionArcadeLocked(ArcadeSessionState state,
                                const std::string& reason,
                                std::string* reject_reason=NULL);
    bool isFinishedStateLocked() const;
    bool isSessionPlayableStateLocked() const;
    void resetCrewStateLocked();
    void resetArcadeRuntimeLocked(bool keep_configuration);
    RelaySessionReport buildReportLocked(const std::string& final_result) const;
    void publishReportLocked(const std::string& final_result);
    void configureCrewRolesLocked();
    void recordPilotTurnForLapLocked(int lap);
    CrewEncounterTriggerContext buildTriggerContextLocked() const;
    void consumeDirectorTrackEffectsLocked();
    void applyPresetLocked(RelaySessionConfig* config) const;
    int sanitizePlayerCount(int player_count) const;
    void ensureDefaultPlayersLocked(const RelaySessionConfig& config,
                                    std::vector<RelayPlayer>* players) const;
    bool allowsNewHandoffLocked(float race_time) const;
    void startSessionLocked(const RelaySessionConfig& config,
                            const std::vector<RelayPlayer>& players);
    bool configureArcadeLocked(const RelaySessionConfig& config,
                               const std::vector<RelayPlayer>& players,
                               std::string* reject_reason);
    bool startRunningFromConfiguredLocked(std::string* reject_reason);
    void updateTimedStateLocked(float race_time);
    bool updateCrewStateForActionLocked(RelayCrewActionRequest* action,
                                        const RelayCrewActionDefinition& def,
                                        int* score_delta,
                                        std::string* effect_summary,
                                        std::string* reject_reason);
    int nowMsForApiLocked() const;
    std::string idempotencyKeyForAction(
        const RelayCrewActionRequest& request) const;
    std::string idempotencySignatureForAction(
        const RelayCrewActionRequest& request,
        const RelayCrewActionDefinition& definition) const;
    bool isRateLimitedLocked(const RelayCrewActionRequest& request,
                             const RelayCrewActionDefinition& definition,
                             int now_ms, std::string* reject_reason,
                             int* retry_ms);
    void markRateLimitLocked(const RelayCrewActionRequest& request,
                             const RelayCrewActionDefinition& definition,
                             int now_ms);
    void pushHistoryLocked(std::vector<RelayCrewActionHistory>* history,
                           const RelayCrewActionHistory& entry) const;
    void recordAcceptedActionLocked(const RelayCrewActionRequest& action,
                                    const RelayCrewActionAck& ack);
    void recordRejectedActionLocked(const RelayCrewActionRequest& request,
                                    const std::string& reason,
                                    int cooldown_ms);
    std::vector<RelayCrewPhoneState> buildPhoneStatesLocked(
        const std::vector<RelayCrewRoleAssignment>& assignments,
        int now_ms) const;
    void annotatePhoneStatesLocked(
        std::vector<RelayCrewPhoneState>* states,
        int now_ms) const;

    void updateProgressLocked(int current_lap, float lap_progress,
                              int checkpoint_id, float distance,
                              bool force_rotation);
    void resetAdaptiveDifficultyLocked();

public:
    RelaySessionController();
    ~RelaySessionController();

    static RelaySessionController* get();
    static void destroy();

    void setRuntimeEnabled(bool enabled);
    bool isRuntimeEnabled() const;
    void setOperatorControlled(bool enabled);
    bool isWorldHeld() const;
    void worldLoadFailed(const std::string& reason);
    void setDefaultPreset(const std::string& preset);
    void setDefaultPlayerCount(int player_count);
    void setArcadeUIEnabled(bool enabled);
    bool isArcadeUIEnabled() const;
    bool setDefaultHandoffPolicy(const std::string& policy);
    static bool parseHandoffPolicy(const std::string& value,
                                   RelayHandoffPolicy* policy);
    static const char* handoffPolicyToString(RelayHandoffPolicy policy);
    static const char* stateToString(RelaySessionState state);
    static int getRecommendedLapCount(int player_count,
                                      int requested_laps=0);

    void setLocalApiOptions(bool enabled, const std::string& bind_host,
                            int port, const std::string& token);
    RelayLocalApiOptions getLocalApiOptions() const;

    RelaySessionConfig getDefaultConfig() const;
    bool startSession(const RelaySessionConfig& config,
                      const std::vector<RelayPlayer>& players,
                      std::string* reject_reason);
    bool configureArcadeSession(const RelaySessionConfig& config,
                                const std::vector<RelayPlayer>& players,
                                std::string* reject_reason);
    void setPresentationReady(bool ready);
    bool operatorStart(std::string* reject_reason);
    bool pilotReady(const std::string& player_id, std::string* reject_reason);
    bool setCrewPresence(const std::string& session_id,
                         const std::vector<std::string>& connected_player_ids,
                         std::string* reject_reason);
    bool operatorCountdown(bool skip, std::string* reject_reason);
    bool operatorPause(std::string* reject_reason);
    bool operatorResume(std::string* reject_reason);
    bool operatorAbort(std::string* reject_reason);
    bool operatorReset(std::string* reject_reason);
    bool operatorForceFinish(std::string* reject_reason);
    bool operatorNextState(const std::string& requested_state,
                           std::string* reject_reason);
    bool operatorRenderPreset(const std::string& render_profile,
                              const std::string& output,
                              const std::string& projection,
                              std::string* reject_reason);
    bool confirmHandoff(std::string* reject_reason);
    void stopSession(const std::string& reason);
    void pauseSession();
    void resumeSession();
    void resetSession();
    void attachRace(const std::string& track_id, unsigned int player_count);
    void detachRace();
    void setTestAcceleratedProgressEnabled(bool enabled);
    bool applyTestProgress(int current_lap, float lap_progress,
                           int checkpoint_id, std::string* reject_reason);
    void updateTrackProgress(int current_lap, float lap_progress,
                             int checkpoint_id, float distance);
    void updateRace(float dt, float race_time, bool safe_for_handoff);
    void updateAdaptiveDifficulty(
        const RelayDifficultySnapshot& difficulty);
    void notifyCheckpoint(int checkpoint_id, float distance, float race_time,
                          bool safe_for_handoff);
    void notifyLap(int completed_laps, float race_time,
                   bool safe_for_handoff);
    bool forceHandoff(const std::string& player_id, std::string* reject_reason);
    bool skipHandoff();
    bool submitCrewAction(const RelayCrewActionRequest& request,
                          RelayCrewActionAck* ack,
                          std::string* reject_reason);
    std::vector<RelayCrewActionRequest> drainPendingCrewActions();
    std::string consumeCrewDomeCallout();

    bool isHumanInputLocked() const;
    RelaySessionSnapshot getSnapshot() const;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_SESSION_CONTROLLER_HPP
