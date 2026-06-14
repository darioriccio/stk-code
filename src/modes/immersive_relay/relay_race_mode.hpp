//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_RACE_MODE_HPP
#define HEADER_RELAY_RACE_MODE_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_action_handler.hpp"
#include "modes/immersive_relay/relay_difficulty_director.hpp"
#include "modes/standard_race.hpp"
#include "utils/vec3.hpp"

#include "btBulletDynamicsCommon.h"

#include <vector>

class AbstractKart;
struct RelaySessionSnapshot;

class RelayRaceMode : public StandardRace
{
private:
    Vec3 m_previous_front;
    std::vector<btTransform> m_handoff_hold_transforms;
    bool m_has_previous_front;
    bool m_handoff_freeze_active;
    bool m_last_handoff_active;
    bool m_last_handoff_warning_active;
    int m_last_countdown_value;
    int m_last_warning_countdown_value;
    int m_last_lap_handoff_warning_lap;
    unsigned int m_last_state_version;
    CrewActionHandler m_crew_action_handler;
    RelayDifficultyDirector m_difficulty_director;

    bool isSafeForHandoff(const AbstractKart* kart) const;
    bool isRescueActive(const AbstractKart* kart) const;
    RelayDifficultySample buildDifficultySample(
        const AbstractKart* kart,
        const RelaySessionSnapshot& snapshot,
        bool safe_for_handoff,
        float dt) const;
    void updateAdaptiveDifficulty(AbstractKart* kart,
                                  const RelaySessionSnapshot& snapshot,
                                  bool safe_for_handoff,
                                  int ticks);
    void holdKartForMandatoryHandoff(AbstractKart* kart) const;
    void captureHandoffFreezeTransforms();
    void releaseHandoffFreeze();
    void enforceHandoffFreeze();
    void updateRelayMessages(const AbstractKart* kart);

public:
    RelayRaceMode();
    virtual ~RelayRaceMode();

    virtual void init() OVERRIDE;
    virtual void reset(bool restart=false) OVERRIDE;
    virtual void update(int ticks) OVERRIDE;
    virtual void updateTime(int ticks) OVERRIDE;
    virtual void newLap(unsigned int kart_index) OVERRIDE;
    virtual const std::string& getIdent() const OVERRIDE;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_RACE_MODE_HPP
