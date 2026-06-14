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
#include "modes/immersive_relay/relay_session_controller.hpp"

#include <cmath>

RelayPlayerController::RelayPlayerController(AbstractKart* kart,
                                             const int local_player_id,
                                             HandicapLevel h)
    : LocalPlayerController(kart, local_player_id, h)
{
    setControllerName("RelayPlayerController");
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
    if (RelaySessionController::get()->isHumanInputLocked() &&
        isDrivingAction(action))
    {
        if (!dry_run)
            resetInputState();
        return true;
    }

    return LocalPlayerController::action(action, value, dry_run);
}

void RelayPlayerController::update(int ticks)
{
    LocalPlayerController::update(ticks);

    if (!RelaySessionController::get()->isHumanInputLocked())
        return;

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
