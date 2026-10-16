//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_player_controller.hpp"

#include "karts/abstract_kart.hpp"
#include "karts/controller/kart_control.hpp"
#include "karts/controller/skidding_ai.hpp"
#include "utils/log.hpp"
#include "modes/immersive_relay/relay_session_controller.hpp"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <string>

RelayPlayerController::RelayPlayerController(AbstractKart* kart,
                                             const int local_player_id,
                                             HandicapLevel h)
    : LocalPlayerController(kart, local_player_id, h)
{
    setControllerName("RelayPlayerController");
    if (autopilotRequested())
    {
        Log::warn("RelayPlayerController",
                  "Diagnostic autopilot drives the relay kart (STK_RELAY_AUTOPILOT).");
        m_autopilot.reset(new SkiddingAI(kart));
        // Network-AI mode: no rubber-band speed cap relative to the "player",
        // which here would be this very kart, and no AI start delay.
        m_autopilot->setNetworkAI(true);
    }
}

RelayPlayerController::~RelayPlayerController()
{
}

bool RelayPlayerController::autopilotRequested()
{
    const char* value = std::getenv("STK_RELAY_AUTOPILOT");
    return value && std::string(value) == "1";
}

void RelayPlayerController::reset()
{
    LocalPlayerController::reset();m_party_raw_brake=0;m_party_brake_ticks=0;
    if (m_autopilot)
    {
        m_autopilot->reset();
        m_autopilot->setNetworkAI(true);
    }
}

void RelayPlayerController::crashed(const AbstractKart* k)
{
    LocalPlayerController::crashed(k);
    if (m_autopilot)
        m_autopilot->crashed(k);
}

void RelayPlayerController::crashed(const Material* m)
{
    LocalPlayerController::crashed(m);
    if (m_autopilot)
        m_autopilot->crashed(m);
}

void RelayPlayerController::newLap(int lap)
{
    LocalPlayerController::newLap(lap);
    // Public through the Controller interface (protected in the AI class).
    if (m_autopilot)
        static_cast<Controller*>(m_autopilot.get())->newLap(lap);
}

bool RelayPlayerController::isDrivingAction(PlayerAction action) const
{
    switch (action)
    {
    case PA_STEER_LEFT:
    case PA_STEER_RIGHT:
    case PA_ACCEL:
    case PA_BRAKE:
    case PA_NITRO:
    case PA_DRIFT:
    case PA_FIRE:
    case PA_RESCUE:
    case PA_LOOK_BACK:
        return true;
    default:
        return false;
    }
}

bool RelayPlayerController::action(PlayerAction action, int value,
                                   bool dry_run)
{
    // LocalPlayerController::action currently ignores its dry_run argument
    // and can also issue a startup boost. Query the pure PlayerController
    // branch instead; pause's native branch likewise ignores dry_run.
    if(dry_run) {
        if(RelaySessionController::get()->isHumanInputLocked()&&isDrivingAction(action))return true;
        if(action==PA_PAUSE_RACE)return value!=0;
        return PlayerController::action(action,value,true);
    }
    if(action==PA_BRAKE)m_party_raw_brake=value;
    if ((m_autopilot || RelaySessionController::get()->isHumanInputLocked()) &&
        isDrivingAction(action))
    {
        if (!dry_run)
            resetInputState();
        return true;
    }

    const bool changed=LocalPlayerController::action(action, value, false);
    enforcePartyBrake();return changed;
}

void RelayPlayerController::beginPartyBrakeLease(int ticks)
{
    // The caller installs this only after a successful Party physical commit.
    if(ticks>0&&ticks<=750)m_party_brake_ticks=std::max(m_party_brake_ticks,ticks);
}

void RelayPlayerController::enforcePartyBrake()
{
    if(m_party_brake_ticks<=0||!partyDriverVeto())return;
    m_controls->setAccel(0.0f);m_controls->setBrake(true);m_controls->setNitro(false);
}

void RelayPlayerController::reset()
{
    LocalPlayerController::reset();m_party_raw_brake=0;m_party_brake_ticks=0;
}

void RelayPlayerController::update(int ticks)
{
    // The player pass recentres an unheld steer before the autopilot runs, and
    // the AI limits its own change per tick to the same rate: the two cancel
    // and the kart barely turns. Hand the AI back the steer it last chose.
    const float steer = m_controls->getSteer();
    LocalPlayerController::update(ticks);
    // The autopilot writes the same kart controls after the player pass.
    if (m_autopilot)
    {
        m_controls->setSteer(steer);
        m_autopilot->update(ticks);
    }

    if (!RelaySessionController::get()->isHumanInputLocked())
    {
        enforcePartyBrake();
        if(ticks>0)m_party_brake_ticks-=std::min(m_party_brake_ticks,ticks);
        return;
    }

    const float speed = std::fabs(m_kart->getSpeed());
    m_controls->setSteer(0.0f);
    m_controls->setAccel(0.0f);
    m_controls->setBrake(speed > 0.5f);
    m_controls->setNitro(false);
    m_controls->setSkidControl(KartControl::SC_NONE);
    m_controls->setFire(false);
    m_controls->setRescue(false);
    m_controls->setLookBack(false);
}

#endif // ENABLE_IMMERSIVE_RELAY
