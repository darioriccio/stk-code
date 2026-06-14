//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_MAIN_HOOKS_HPP
#define HEADER_RELAY_MAIN_HOOKS_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

namespace RelayMainHooks
{
    void handleCommandLineOptions();
    void applyRaceDefaultsAfterLapOptions();
    void startLocalApiIfEnabled();
    void shutdown();
}

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_RELAY_MAIN_HOOKS_HPP
