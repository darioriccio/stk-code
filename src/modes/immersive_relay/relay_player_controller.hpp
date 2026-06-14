//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_PLAYER_CONTROLLER_HPP
#define HEADER_RELAY_PLAYER_CONTROLLER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "karts/controller/local_player_controller.hpp"

class RelayPlayerController : public LocalPlayerController
{
private:
    bool isDrivingAction(PlayerAction action) const;

public:
    RelayPlayerController(AbstractKart* kart, const int local_player_id,
                          HandicapLevel h);

    virtual void update(int ticks) OVERRIDE;
    virtual bool action(PlayerAction action, int value,
                        bool dry_run=false) OVERRIDE;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_PLAYER_CONTROLLER_HPP
