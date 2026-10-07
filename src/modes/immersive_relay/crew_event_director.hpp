//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_CREW_EVENT_DIRECTOR_HPP
#define HEADER_CREW_EVENT_DIRECTOR_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_role_manager.hpp"

#include <map>
#include <string>
#include <vector>

struct CrewEncounterTriggerContext
{
    std::string m_track_id;
    int m_current_lap;
    int m_total_laps;
    float m_lap_progress;
    int m_checkpoint_id;
    float m_distance;
    int m_active_driver_index;
    std::string m_active_driver_id;
    int m_player_count;
    std::string m_race_phase;

    CrewEncounterTriggerContext();
};

struct RelayCrewPhoneAction
{
    std::string m_type;
    std::string m_label;
    RelayCrewRole m_role;
    std::string m_target_key;
    std::string m_target_id;
    std::string m_mode_key;
    std::string m_mode;
    int m_amount;
    bool m_available;
    std::string m_unavailable_reason;
    int m_cooldown_ms;

    RelayCrewPhoneAction();
};

struct RelayCrewPhoneState
{
    std::string m_player_id;
    RelayCrewRole m_role;
    int m_lap;
    int m_total_laps;
    int m_active_driver_index;
    std::string m_active_driver_id;
    std::string m_encounter_id;
    std::string m_next_encounter_id;
    std::string m_title;
    std::string m_message;
    std::string m_resource_text;
    int m_remaining_ms;
    bool m_primary;
    std::vector<RelayCrewPhoneAction> m_actions;

    RelayCrewPhoneState();
};

struct RelayCrewEncounterSnapshot
{
    bool m_active;
    std::string m_id;
    std::string m_title;
    std::string m_stage;
    std::string m_state;
    std::string m_trigger_type;
    std::string m_combo_tag;
    std::string m_dome_callout;
    int m_remaining_ms;
    int m_required_actions;
    int m_completed_actions;
    std::vector<RelayCrewRole> m_roles;

    RelayCrewEncounterSnapshot();
};

class CrewEventDirector
{
private:
    enum EncounterState
    {
        ENCOUNTER_PENDING,
        ENCOUNTER_PREVIEW,
        ENCOUNTER_ACTIVE,
        ENCOUNTER_RESOLVED_SUCCESS,
        ENCOUNTER_RESOLVED_FAIL,
        ENCOUNTER_EXPIRED
    };

    struct Trigger
    {
        std::string m_type;
        int m_lap;
        std::vector<int> m_laps;
        std::string m_lap_pattern;
        int m_lap_every;
        int m_lap_offset;
        int m_checkpoint;
        float m_progress;
        float m_preview_progress;
        float m_duration_progress;

        Trigger();
    };

    struct Step
    {
        std::string m_id;
        int m_start_ms;
        int m_end_ms;
        RelayCrewRole m_role;
        std::string m_title;
        std::string m_message;
        std::string m_dome_callout;
        std::vector<RelayCrewPhoneAction> m_actions;
        bool m_required;
        bool m_completed;

        Step();
    };

    struct Encounter
    {
        bool m_enabled;
        int m_player_count_min;
        int m_player_count_max;
        int m_duration_ms;
        int m_fail_score;
        int m_success_score;
        int m_combo_score;
        EncounterState m_state;
        int m_runtime_lap;
        int m_active_started_ms;
        bool m_started_logged;
        bool m_preview_logged;
        bool m_payoff_logged;
        bool m_combo_awarded;
        std::string m_id;
        std::string m_title;
        Trigger m_trigger;
        std::vector<RelayCrewRole> m_roles;
        std::vector<Step> m_steps;
        std::vector<std::string> m_required_actions;
        std::vector<std::string> m_accepted_actions;
        std::string m_combo_tag;
        std::string m_fail_soft_penalty;
        std::map<std::string, std::string> m_success_effects;
        std::map<std::string, std::string> m_fail_effects;

        Encounter();
    };

    std::string m_track_id;
    int m_total_laps;
    int m_player_count;
    int m_now_ms;
    int m_last_active_index;
    CrewEncounterTriggerContext m_context;
    std::vector<Encounter> m_encounters;
    std::vector<RelayCrewRole> m_available_roles;
    bool actionRoleAvailable(const std::string& identity) const;
    std::string m_pending_dome_callout;
    std::vector<RelayCrewTrackEffect> m_pending_track_effects;
    std::map<std::string, int> m_role_ready_at_ms;
    std::map<std::string, int> m_encounter_ready_at_ms;
    std::map<std::string, int> m_combo_ready_at_ms;
    int m_encounters_started;
    int m_encounters_succeeded;
    int m_encounters_failed;
    int m_combo_count;

    RelayCrewPhoneAction makeAction(
        const std::string& type, const std::string& label,
        const std::string& target_key, const std::string& target_id,
        const std::string& mode_key, const std::string& mode, int amount);
    Step makeStep(const std::string& id, int start_ms, int end_ms,
                  RelayCrewRole role, const std::string& title,
                  const std::string& message,
                  const std::string& dome_callout,
                  const std::vector<RelayCrewPhoneAction>& actions,
                  bool required) const;
    bool loadEncounterScripts();
    bool loadEncounterScriptFile(const std::string& path);
    std::string makeRuntimeEncounterId(const Encounter& encounter) const;
    std::string stateToString(EncounterState state) const;
    bool lapMatches(const Encounter& encounter) const;
    void resetEncounterRuntime(Encounter* encounter, int lap);
    EncounterState evaluateState(const Encounter& encounter) const;
    void queueEffects(const std::map<std::string, std::string>& effects);
    void updateEncounter(Encounter* encounter);
    int getBestEncounterIndex() const;
    std::string getStageForEncounter(const Encounter& encounter) const;
    const Step* getCurrentStepForRole(const Encounter& encounter,
                                      RelayCrewRole role) const;
    Step* getCurrentStepForRole(Encounter* encounter, RelayCrewRole role);
    std::string actionIdentity(const RelayCrewActionRequest& action) const;
    std::string actionIdentity(const RelayCrewPhoneAction& action) const;
    bool actionMatchesStep(const Step& step,
                           const RelayCrewActionRequest& action) const;
    bool encounterIncludesAction(const Encounter& encounter,
                                 const RelayCrewActionRequest& action) const;
    bool hasAcceptedAction(const Encounter& encounter,
                           const std::string& identity) const;
    bool requiredActionsSatisfied(const Encounter& encounter) const;
    int countRequiredActions(const Encounter& encounter) const;
    int countCompletedActions(const Encounter& encounter) const;

public:
    CrewEventDirector();

    void configure(const std::string& track_id, int total_laps,
                   int player_count);
    void reset();
    void setAvailableRoles(const std::vector<RelayCrewRoleAssignment>& assignments);
    bool update(const CrewEncounterTriggerContext& context, int now_ms);
    bool validateAction(const RelayCrewActionRequest& action,
                        std::string* reject_reason) const;
    int markActionAccepted(const RelayCrewActionRequest& action,
                           std::string* effect_suffix);
    std::string consumeDomeCallout();
    std::vector<RelayCrewTrackEffect> consumeTrackEffects();
    RelayCrewEncounterSnapshot getSnapshot() const;
    std::vector<RelayCrewPhoneState> getPhoneStates(
        const std::vector<RelayCrewRoleAssignment>& assignments,
        int boost_charge, int heat, int armor) const;
    int getEncountersStarted() const { return m_encounters_started; }
    int getEncountersSucceeded() const { return m_encounters_succeeded; }
    int getEncountersFailed() const { return m_encounters_failed; }
    int getComboCount() const { return m_combo_count; }
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_CREW_EVENT_DIRECTOR_HPP
