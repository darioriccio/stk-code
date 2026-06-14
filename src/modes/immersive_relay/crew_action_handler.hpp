//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_CREW_ACTION_HANDLER_HPP
#define HEADER_CREW_ACTION_HANDLER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_role_manager.hpp"

#include <string>
#include <vector>

class AbstractKart;
class RaceGUIBase;

class CrewActionHandler
{
private:
    int timeToTicks(float seconds) const;
    void addRelayMessage(RaceGUIBase* gui, const AbstractKart* kart,
                         const std::string& text, int red, int green,
                         int blue) const;
    void giveBubbleShield(AbstractKart* kart, float seconds) const;
    void applyPilotBoost(AbstractKart* kart, float add_max_speed,
                         float speed_boost, float duration_seconds) const;
    AbstractKart* findRivalKart(const RelayCrewActionRequest& action,
                                const AbstractKart* pilot_kart) const;
    void slowRival(const RelayCrewActionRequest& action,
                   const AbstractKart* pilot_kart,
                   float seconds, float speed_factor) const;
    void slowAllRivals(const AbstractKart* pilot_kart,
                       float seconds, float speed_factor) const;
    std::string displayTarget(const RelayCrewActionRequest& action,
                              const std::string& fallback) const;
    void applyAction(const RelayCrewActionRequest& action,
                     AbstractKart* pilot_kart,
                     RaceGUIBase* gui) const;

public:
    CrewActionHandler();

    void applyActions(const std::vector<RelayCrewActionRequest>& actions,
                      AbstractKart* pilot_kart,
                      RaceGUIBase* gui) const;
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_CREW_ACTION_HANDLER_HPP
