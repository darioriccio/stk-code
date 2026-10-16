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

#include <memory>

class SkiddingAI;

class RelayPlayerController : public LocalPlayerController
{
private:
    bool isDrivingAction(PlayerAction action) const;
    int m_party_raw_brake=0;
    int m_party_brake_ticks=0;
    void enforcePartyBrake();
    // Diagnostic autopilot ($STK_RELAY_AUTOPILOT=1): the native AI drives
    // while the kart stays the human relay kart for every Relay rule.
    std::unique_ptr<SkiddingAI> m_autopilot;

public:
    RelayPlayerController(AbstractKart* kart, const int local_player_id,
                          HandicapLevel h);
    virtual ~RelayPlayerController();
    static bool autopilotRequested();
    virtual void reset() OVERRIDE;
    virtual void crashed(const AbstractKart* k) OVERRIDE;
    virtual void crashed(const Material* m) OVERRIDE;
    virtual void newLap(int lap) OVERRIDE;

    virtual void update(int ticks) OVERRIDE;
    virtual bool action(PlayerAction action, int value,
                        bool dry_run=false) OVERRIDE;
    // Retains the actual analog action even while the session holds input.
    // m_prev_brake cannot be used: vanilla stores any nonzero value there.
    bool partyDriverVeto() const {return m_party_raw_brake>32768/2;}
    void beginPartyBrakeLease(int ticks);
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_PLAYER_CONTROLLER_HPP
