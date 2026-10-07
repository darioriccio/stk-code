//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_race_mode.hpp"

#include "config/stk_config.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/abstract_kart_animation.hpp"
#include "karts/controller/kart_control.hpp"
#include "karts/controller/player_controller.hpp"
#include "modes/immersive_relay/relay_session_controller.hpp"
#include "physics/btKart.hpp"
#include "race/race_manager.hpp"
#include "states_screens/race_gui_base.hpp"
#include "tracks/check_manager.hpp"
#include "tracks/track.hpp"
#include "utils/string_utils.hpp"

#include <SColor.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
static const std::string IDENT_IMMERSIVE_RELAY("IMMERSIVE_RELAY");
}

RelayRaceMode::RelayRaceMode()
    : StandardRace()
{
    m_has_previous_front = false;
    m_handoff_freeze_active = false;
    m_last_handoff_active = false;
    m_last_handoff_warning_active = false;
    m_last_countdown_value = -1;
    m_last_warning_countdown_value = -1;
    m_last_lap_handoff_warning_lap = -1;
    m_last_state_version = 0;
}

RelayRaceMode::~RelayRaceMode()
{
    RelaySessionController::get()->detachRace();
}

void RelayRaceMode::init()
{
    StandardRace::init();
    m_difficulty_director.reset();
    m_difficulty_director.setConfig(
        RelaySessionController::get()->getSnapshot()
            .m_adaptive_difficulty_config);
    RelaySessionController::get()->attachRace(
        RaceManager::get()->getTrackName(),
        RaceManager::get()->getNumPlayers());
}

void RelayRaceMode::reset(bool restart)
{
    StandardRace::reset(restart);
    m_has_previous_front = false;
    releaseHandoffFreeze();
    m_last_handoff_active = false;
    m_last_handoff_warning_active = false;
    m_last_countdown_value = -1;
    m_last_warning_countdown_value = -1;
    m_last_lap_handoff_warning_lap = -1;
    m_last_state_version = 0;
    m_difficulty_director.reset();
    m_difficulty_director.setConfig(
        RelaySessionController::get()->getSnapshot()
            .m_adaptive_difficulty_config);
    RelaySessionController::get()->attachRace(
        RaceManager::get()->getTrackName(),
        RaceManager::get()->getNumPlayers());
}

bool RelayRaceMode::isSafeForHandoff(const AbstractKart* kart) const
{
    if (!kart || getPhase() != RACE_PHASE)
        return false;
    if (kart->getKartAnimation() || !kart->isOnGround())
        return false;
    if (std::fabs(kart->getSpeed()) > 35.0f)
        return false;
    if (std::fabs(kart->getControls().getSteer()) > 0.78f)
        return false;
    return true;
}

bool RelayRaceMode::isRescueActive(const AbstractKart* kart) const
{
    if (!kart || !kart->getKartAnimation())
        return false;
    return kart->getKartAnimation()->getName() == "RescueAnimation";
}

RelayDifficultySample RelayRaceMode::buildDifficultySample(
    const AbstractKart* kart,
    const RelaySessionSnapshot& snapshot,
    bool safe_for_handoff,
    float dt) const
{
    (void)safe_for_handoff;
    RelayDifficultySample sample;
    sample.m_dt = dt;
    sample.m_race_time = getTime();
    sample.m_current_lap = snapshot.m_current_lap;
    sample.m_total_laps = snapshot.m_total_laps;
    sample.m_lap_progress = snapshot.m_lap_progress;
    sample.m_completed_handoffs = snapshot.m_rotation.m_completed_handoffs;
    sample.m_handoff_active = snapshot.m_rotation.m_handoff_active ||
        snapshot.m_state == RELAY_STATE_HANDOFF;

    if (!kart)
        return sample;

    sample.m_human_position = kart->getPosition();
    sample.m_kart_count = (int)getNumKarts();
    sample.m_human_speed = std::fabs(kart->getSpeed());
    sample.m_human_max_speed = kart->getCurrentMaxSpeed();
    sample.m_rescue_active = isRescueActive(kart);

    const unsigned int human_id = kart->getWorldKartId();
    if (human_id < getNumKarts())
        sample.m_on_road = isOnRoad(human_id);

    const float human_distance = human_id < getNumKarts()
        ? getOverallDistance(human_id) : 0.0f;
    float leader_distance = human_distance;
    float next_distance = -std::numeric_limits<float>::max();

    for (unsigned int i = 0; i < getNumKarts(); i++)
    {
        const AbstractKart* other = getKart((int)i);
        if (!other || other->isEliminated())
            continue;

        const float distance = getOverallDistance(i);
        if (distance > leader_distance)
            leader_distance = distance;
        if (i != human_id && distance <= human_distance &&
            distance > next_distance)
        {
            next_distance = distance;
        }
    }

    sample.m_gap_to_leader_m =
        std::max(0.0f, leader_distance - human_distance);
    if (next_distance > -std::numeric_limits<float>::max() * 0.5f)
        sample.m_gap_to_next_m =
            std::max(0.0f, human_distance - next_distance);
    return sample;
}

void RelayRaceMode::updateAdaptiveDifficulty(
    AbstractKart* kart,
    const RelaySessionSnapshot& snapshot,
    bool safe_for_handoff,
    int ticks)
{
    m_difficulty_director.setConfig(snapshot.m_adaptive_difficulty_config);
    const float dt = stk_config ? stk_config->ticks2Time(ticks) : 0.0f;
    const RelayDifficultySample sample =
        buildDifficultySample(kart, snapshot, safe_for_handoff, dt);
    m_difficulty_director.update(sample);
    m_difficulty_director.apply(kart, this, RaceManager::get()->getNumPlayers());
    RelaySessionController::get()->updateAdaptiveDifficulty(
        m_difficulty_director.getSnapshot());
}

void RelayRaceMode::holdKartForMandatoryHandoff(AbstractKart* kart) const
{
    if (!kart || kart->isEliminated())
        return;

    KartControl& controls = kart->getControls();
    controls.reset();
    controls.setSteer(0.0f);
    controls.setAccel(0.0f);
    controls.setBrake(true);
    controls.setNitro(false);
    controls.setSkidControl(KartControl::SC_NONE);
    controls.setFire(false);
    controls.setRescue(false);
    controls.setLookBack(false);

    if (kart->getVehicle())
        kart->getVehicle()->setAllBrakes(100000.0f);

    kart->getBody()->clearForces();
    kart->getBody()->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
    kart->getBody()->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
    kart->setSpeed(0.0f);
}

void RelayRaceMode::captureHandoffFreezeTransforms()
{
    m_handoff_hold_transforms.clear();
    m_handoff_hold_transforms.reserve(getNumKarts());
    for (unsigned int i = 0; i < getNumKarts(); i++)
    {
        AbstractKart* kart = getKart(i);
        if (kart)
        {
            // Clear remembered keyboard/gamepad actions, not only KartControl.
            // A key held by the previous pilot must not revive after handoff.
            if (kart->getController() && kart->getController()->isPlayerController())
                static_cast<PlayerController*>(kart->getController())->resetInputState();
            m_handoff_hold_transforms.push_back(
                kart->getBody()->getWorldTransform());
        }
        else
            m_handoff_hold_transforms.push_back(btTransform::getIdentity());
    }
    m_handoff_freeze_active = true;
}

void RelayRaceMode::releaseHandoffFreeze()
{
    if (!m_handoff_freeze_active)
        return;

    for (unsigned int i = 0; i < getNumKarts(); i++)
    {
        AbstractKart* kart = getKart(i);
        if (kart && kart->getVehicle())
        {
            if (kart->getController() && kart->getController()->isPlayerController())
                static_cast<PlayerController*>(kart->getController())->resetInputState();
            kart->getVehicle()->setAllBrakes(0.0f);
        }
    }

    m_handoff_hold_transforms.clear();
    m_handoff_freeze_active = false;
}

void RelayRaceMode::enforceHandoffFreeze()
{
    if (!m_handoff_freeze_active ||
        m_handoff_hold_transforms.size() != getNumKarts())
    {
        captureHandoffFreezeTransforms();
    }

    for (unsigned int i = 0; i < getNumKarts(); i++)
    {
        AbstractKart* kart = getKart(i);
        if (!kart || kart->isEliminated())
            continue;

        const btTransform& transform = m_handoff_hold_transforms[i];
        holdKartForMandatoryHandoff(kart);
        kart->getBody()->setCenterOfMassTransform(transform);
        kart->getBody()->proceedToTransform(transform);
        kart->setTrans(transform);
        if (kart->getVehicle())
            kart->getVehicle()->updateAllWheelPositions();
    }
}

void RelayRaceMode::updateRelayMessages(const AbstractKart* kart)
{
    RaceGUIBase* gui = getRaceGUI();
    if (!gui)
        return;

    RelaySessionSnapshot snapshot =
        RelaySessionController::get()->getSnapshot();
    const RelayPilotRotationSnapshot& rotation = snapshot.m_rotation;

    const std::string crew_callout =
        RelaySessionController::get()->consumeCrewDomeCallout();
    if (!crew_callout.empty())
    {
        gui->addMessage(StringUtils::utf8ToWide(crew_callout),
            kart, 2.4f, video::SColor(255, 110, 220, 255),
            true, true, true);
    }

    if (snapshot.m_player_count >= 2 && kart &&
        !rotation.m_handoff_active &&
        !rotation.m_handoff_pending_safe_zone)
    {
        Track* track = Track::getCurrentTrack();
        const int completed_laps = getLapForKart(kart->getWorldKartId());
        const int lap_count = RaceManager::get()->getNumLaps();
        if (track && lap_count > 1 && completed_laps + 1 < lap_count)
        {
            const float track_length = track->getTrackLength();
            const float distance = getDistanceDownTrackForKart(
                kart->getWorldKartId(), true);
            const float lap_fraction = track_length > 0.0f
                ? distance / track_length : 0.0f;
            if (lap_fraction >= 0.75f &&
                m_last_lap_handoff_warning_lap != completed_laps)
            {
                std::string message = "Cambio pilota a fine giro: ";
                message += rotation.m_next_display_name.empty()
                    ? rotation.m_next_player_id : rotation.m_next_display_name;
                message += " preparati";
                gui->addMessage(StringUtils::utf8ToWide(message),
                    kart, 3.0f, video::SColor(255, 255, 214, 64),
                    true, true, true);
                m_last_lap_handoff_warning_lap = completed_laps;
            }
        }
    }

    if (rotation.m_handoff_warning_active)
    {
        const int countdown =
            (rotation.m_handoff_warning_remaining_ms + 999) / 1000;
        if (!m_last_handoff_warning_active ||
            countdown != m_last_warning_countdown_value)
        {
            std::string message = "Cambio pilota tra ";
            message += std::to_string(countdown);
            message += "s: ";
            message += rotation.m_next_display_name.empty()
                ? rotation.m_next_player_id : rotation.m_next_display_name;
            message += " preparati";
            gui->addMessage(StringUtils::utf8ToWide(message),
                kart, 1.2f, video::SColor(255, 255, 214, 64),
                true, true, true);
            m_last_warning_countdown_value = countdown;
        }
    }
    else if (m_last_handoff_warning_active)
    {
        m_last_warning_countdown_value = -1;
    }

    if (snapshot.m_state_version != m_last_state_version &&
        rotation.m_handoff_pending_safe_zone)
    {
        gui->addMessage(StringUtils::utf8ToWide(
            "Cambio pilota in attesa di zona sicura"),
            kart, 2.0f, video::SColor(255, 255, 214, 64),
            true, true, true);
    }

    if (rotation.m_handoff_active && !m_last_handoff_active)
    {
        std::string message = "STOP CAMBIO PILOTA: ";
        message += rotation.m_next_display_name.empty()
            ? rotation.m_next_player_id : rotation.m_next_display_name;
        message += " al volante";
        gui->addMessage(StringUtils::utf8ToWide(message),
            kart, 3.0f, video::SColor(255, 255, 214, 64),
            true, true, true);
        m_last_countdown_value = -1;
    }
    else if (!rotation.m_handoff_active && m_last_handoff_active)
    {
        std::string message = "ORA! PILOTA ATTIVO: ";
        message += rotation.m_active_display_name.empty()
            ? rotation.m_active_player_id : rotation.m_active_display_name;
        gui->addMessage(StringUtils::utf8ToWide(message),
            kart, 2.0f, video::SColor(255, 120, 255, 120),
            true, true, true);
        m_last_countdown_value = -1;
    }

    if (rotation.m_handoff_active)
    {
        const int countdown = (rotation.m_handoff_remaining_ms + 999) / 1000;
        if (!rotation.m_waiting_for_operator_confirm &&
            countdown <= 3 && countdown > 0 && countdown != m_last_countdown_value)
        {
            gui->addMessage(StringUtils::toWString(countdown),
                kart, 0.8f, video::SColor(255, 255, 255, 255),
                true, true, true);
            m_last_countdown_value = countdown;
        }
        else if (rotation.m_waiting_for_operator_confirm && countdown == 0 &&
                 m_last_countdown_value != 0)
        {
            gui->addMessage(StringUtils::utf8ToWide("NUOVO PILOTA: PREMI PRONTO"),
                kart, 10.0f, video::SColor(255, 255, 255, 255), true, true, true);
            m_last_countdown_value = 0;
        }
    }

    m_last_handoff_active = rotation.m_handoff_active;
    m_last_handoff_warning_active = rotation.m_handoff_warning_active;
    m_last_state_version = snapshot.m_state_version;
}

void RelayRaceMode::update(int ticks)
{
    AbstractKart* player_kart = RaceManager::get()->getNumPlayers() > 0
        ? getPlayerKart(0) : NULL;

    RelaySessionSnapshot snapshot =
        RelaySessionController::get()->getSnapshot();
    if (RelaySessionController::get()->isWorldHeld())
    {
        if (!m_handoff_freeze_active)
            captureHandoffFreezeTransforms();

        enforceHandoffFreeze();
        RelaySessionController::get()->updateRace(
            stk_config->ticks2Time(ticks), getTime(), true);

        player_kart = RaceManager::get()->getNumPlayers() > 0
            ? getPlayerKart(0) : NULL;
        snapshot = RelaySessionController::get()->getSnapshot();
        updateAdaptiveDifficulty(player_kart, snapshot, false, ticks);
        if (player_kart)
            updateRelayMessages(player_kart);

        snapshot = RelaySessionController::get()->getSnapshot();
        if (RelaySessionController::get()->isWorldHeld())
            enforceHandoffFreeze();
        else
            releaseHandoffFreeze();
        return;
    }

    releaseHandoffFreeze();

    const Vec3 previous_front = m_has_previous_front && player_kart
        ? m_previous_front : Vec3(0, 0, 0);

    StandardRace::update(ticks);

    player_kart = RaceManager::get()->getNumPlayers() > 0
        ? getPlayerKart(0) : NULL;
    if (!player_kart)
        return;

    const bool safe_for_handoff = isSafeForHandoff(player_kart);
    Track* track = Track::getCurrentTrack();
    if (track)
    {
        const float track_length = track->getTrackLength();
        const float distance = getDistanceDownTrackForKart(
            player_kart->getWorldKartId(), true);
        const float lap_progress = track_length > 0.0f
            ? std::max(0.0f, std::min(1.0f, distance / track_length))
            : 0.0f;
        const int current_lap = std::max(1,
            getLapForKart(player_kart->getWorldKartId()) + 1);
        RelaySessionController::get()->updateTrackProgress(
            current_lap, lap_progress, -1, distance);
    }
    RelaySessionController::get()->updateRace(
        stk_config->ticks2Time(ticks), getTime(), safe_for_handoff);

    std::vector<RelayCrewActionRequest> crew_actions =
        RelaySessionController::get()->drainPendingCrewActions();
    if (!crew_actions.empty())
        m_crew_action_handler.applyActions(crew_actions, player_kart,
                                           getRaceGUI());

    snapshot = RelaySessionController::get()->getSnapshot();
    updateAdaptiveDifficulty(player_kart, snapshot, safe_for_handoff, ticks);

    const Vec3 current_front = player_kart->getFrontXYZ();
    if (m_has_previous_front)
    {
            CheckManager* check_manager = Track::getCurrentTrack()
                ? Track::getCurrentTrack()->getCheckManager() : NULL;
        if (check_manager)
        {
            const int checkpoint_id = check_manager->getChecklineTriggering(
                previous_front, current_front);
            if (checkpoint_id >= 0)
            {
                RelaySessionController::get()->notifyCheckpoint(
                    checkpoint_id,
                    getDistanceDownTrackForKart(
                        player_kart->getWorldKartId(), true),
                    getTime(),
                    safe_for_handoff);
            }
        }
    }

    m_previous_front = current_front;
    m_has_previous_front = true;
    updateRelayMessages(player_kart);

    snapshot = RelaySessionController::get()->getSnapshot();
    if (snapshot.m_player_count >= 2 &&
        snapshot.m_rotation.m_handoff_active)
    {
        captureHandoffFreezeTransforms();
        enforceHandoffFreeze();
    }
}

void RelayRaceMode::updateTime(int ticks)
{
    if (RelaySessionController::get()->isWorldHeld())
    {
        return;
    }
    StandardRace::updateTime(ticks);
}

void RelayRaceMode::newLap(unsigned int kart_index)
{
    StandardRace::newLap(kart_index);
    AbstractKart* player_kart = RaceManager::get()->getNumPlayers() > 0
        ? getPlayerKart(0) : NULL;
    if (player_kart && player_kart->getWorldKartId() == kart_index)
    {
        const int completed_laps = getLapForKart(kart_index);
        if (completed_laps > 0)
        {
            RelaySessionController::get()->notifyLap(
                completed_laps, getTime(), isSafeForHandoff(player_kart));
        }
    }
}

const std::string& RelayRaceMode::getIdent() const
{
    return IDENT_IMMERSIVE_RELAY;
}

#endif // ENABLE_IMMERSIVE_RELAY
