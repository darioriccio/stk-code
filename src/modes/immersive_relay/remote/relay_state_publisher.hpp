//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_STATE_PUBLISHER_HPP
#define HEADER_RELAY_STATE_PUBLISHER_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_session_controller.hpp"

#include <string>

class RelayStatePublisher
{
public:
    static std::string jsonEscape(const std::string& input);
    static std::string stateJson(const RelaySessionSnapshot& snapshot);
    static std::string stateJson(const RelaySessionSnapshot& snapshot,
                                 const std::string& player_filter,
                                 const std::string& role_filter);
    static std::string healthJson(const RelaySessionSnapshot& snapshot,
                                  bool api_active, int api_port);
    static std::string capabilitiesJson(const RelaySessionSnapshot& snapshot,
                                        bool api_active, int api_port);
    static std::string okJson(const RelaySessionSnapshot& snapshot,
                              const std::string& event_name);
    static std::string crewActionJson(const RelaySessionSnapshot& snapshot,
                                      const RelayCrewActionAck& ack);
    static std::string errorJson(const std::string& error,
                                 unsigned int state_version);
    static std::string errorJson(const std::string& error,
                                 unsigned int state_version,
                                 int retry_after_ms);
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_STATE_PUBLISHER_HPP
