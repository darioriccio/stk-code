//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_ARCADE_SESSION_STATE_MACHINE_HPP
#define HEADER_ARCADE_SESSION_STATE_MACHINE_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include <string>
#include <vector>

enum ArcadeSessionState
{
    ARCADE_STATE_BOOT,
    ARCADE_STATE_IDLE,
    ARCADE_STATE_ATTRACT,
    ARCADE_STATE_READY_FOR_PLAYERS,
    ARCADE_STATE_SESSION_CONFIGURED,
    ARCADE_STATE_BRIEFING,
    ARCADE_STATE_COUNTDOWN,
    ARCADE_STATE_RUNNING,
    ARCADE_STATE_LAP_HANDOFF,
    ARCADE_STATE_PAUSED,
    ARCADE_STATE_FINAL_LAP,
    ARCADE_STATE_FINISH_SEQUENCE,
    ARCADE_STATE_RESULTS,
    ARCADE_STATE_RESETTING,
    ARCADE_STATE_ERROR_RECOVERABLE,
    ARCADE_STATE_ERROR_FATAL,
    ARCADE_STATE_SHUTDOWN
};

class ArcadeSessionStateMachine
{
private:
    ArcadeSessionState m_state;
    ArcadeSessionState m_previous_state;
    unsigned int m_transition_count;
    std::string m_last_error;
    bool m_last_error_recoverable;

public:
    ArcadeSessionStateMachine();

    static const char* stateToString(ArcadeSessionState state);
    static ArcadeSessionState stateFromString(const std::string& value,
                                              bool* ok);
    static std::vector<ArcadeSessionState> allowedTransitions(
        ArcadeSessionState state);
    static std::vector<std::string> allowedOperatorCommands(
        ArcadeSessionState state);
    static bool canTransition(ArcadeSessionState from,
                              ArcadeSessionState to);

    ArcadeSessionState getState() const { return m_state; }
    ArcadeSessionState getPreviousState() const { return m_previous_state; }
    unsigned int getTransitionCount() const { return m_transition_count; }
    const std::string& getLastError() const { return m_last_error; }
    bool isLastErrorRecoverable() const { return m_last_error_recoverable; }

    bool transitionTo(ArcadeSessionState state, const std::string& reason,
                      std::string* reject_reason);
    bool fail(const std::string& error, bool recoverable,
              std::string* reject_reason);
    void clearError();
};

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_ARCADE_SESSION_STATE_MACHINE_HPP
