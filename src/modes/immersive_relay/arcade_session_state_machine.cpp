//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/arcade_session_state_machine.hpp"

#include "utils/log.hpp"

#include <algorithm>

ArcadeSessionStateMachine::ArcadeSessionStateMachine()
{
    m_state = ARCADE_STATE_BOOT;
    m_previous_state = ARCADE_STATE_BOOT;
    m_transition_count = 0;
    m_last_error_recoverable = true;
}

const char* ArcadeSessionStateMachine::stateToString(
    ArcadeSessionState state)
{
    switch (state)
    {
    case ARCADE_STATE_BOOT: return "boot";
    case ARCADE_STATE_IDLE: return "idle";
    case ARCADE_STATE_ATTRACT: return "attract";
    case ARCADE_STATE_READY_FOR_PLAYERS: return "ready_for_players";
    case ARCADE_STATE_SESSION_CONFIGURED: return "session_configured";
    case ARCADE_STATE_BRIEFING: return "briefing";
    case ARCADE_STATE_COUNTDOWN: return "countdown";
    case ARCADE_STATE_RUNNING: return "running";
    case ARCADE_STATE_LAP_HANDOFF: return "lap_handoff";
    case ARCADE_STATE_PAUSED: return "paused";
    case ARCADE_STATE_FINAL_LAP: return "final_lap";
    case ARCADE_STATE_FINISH_SEQUENCE: return "finish_sequence";
    case ARCADE_STATE_RESULTS: return "results";
    case ARCADE_STATE_RESETTING: return "resetting";
    case ARCADE_STATE_ERROR_RECOVERABLE: return "error_recoverable";
    case ARCADE_STATE_ERROR_FATAL: return "error_fatal";
    case ARCADE_STATE_SHUTDOWN: return "shutdown";
    }
    return "error_recoverable";
}

ArcadeSessionState ArcadeSessionStateMachine::stateFromString(
    const std::string& value, bool* ok)
{
    struct Entry
    {
        const char* m_name;
        ArcadeSessionState m_state;
    };
    static const Entry entries[] = {
        {"boot", ARCADE_STATE_BOOT},
        {"idle", ARCADE_STATE_IDLE},
        {"attract", ARCADE_STATE_ATTRACT},
        {"ready_for_players", ARCADE_STATE_READY_FOR_PLAYERS},
        {"session_configured", ARCADE_STATE_SESSION_CONFIGURED},
        {"briefing", ARCADE_STATE_BRIEFING},
        {"countdown", ARCADE_STATE_COUNTDOWN},
        {"running", ARCADE_STATE_RUNNING},
        {"lap_handoff", ARCADE_STATE_LAP_HANDOFF},
        {"paused", ARCADE_STATE_PAUSED},
        {"final_lap", ARCADE_STATE_FINAL_LAP},
        {"finish_sequence", ARCADE_STATE_FINISH_SEQUENCE},
        {"results", ARCADE_STATE_RESULTS},
        {"resetting", ARCADE_STATE_RESETTING},
        {"error_recoverable", ARCADE_STATE_ERROR_RECOVERABLE},
        {"error_fatal", ARCADE_STATE_ERROR_FATAL},
        {"shutdown", ARCADE_STATE_SHUTDOWN}
    };

    for (unsigned int i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        if (value == entries[i].m_name)
        {
            if (ok)
                *ok = true;
            return entries[i].m_state;
        }
    }

    if (ok)
        *ok = false;
    return ARCADE_STATE_ERROR_RECOVERABLE;
}

std::vector<ArcadeSessionState>
ArcadeSessionStateMachine::allowedTransitions(ArcadeSessionState state)
{
    std::vector<ArcadeSessionState> out;
    switch (state)
    {
    case ARCADE_STATE_BOOT:
        out.push_back(ARCADE_STATE_IDLE);
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        out.push_back(ARCADE_STATE_ERROR_FATAL);
        out.push_back(ARCADE_STATE_SHUTDOWN);
        break;
    case ARCADE_STATE_IDLE:
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_SESSION_CONFIGURED);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        out.push_back(ARCADE_STATE_SHUTDOWN);
        break;
    case ARCADE_STATE_ATTRACT:
        out.push_back(ARCADE_STATE_IDLE);
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_SESSION_CONFIGURED);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        out.push_back(ARCADE_STATE_SHUTDOWN);
        break;
    case ARCADE_STATE_READY_FOR_PLAYERS:
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_SESSION_CONFIGURED);
        out.push_back(ARCADE_STATE_BRIEFING);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_SESSION_CONFIGURED:
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_BRIEFING);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_BRIEFING:
        out.push_back(ARCADE_STATE_COUNTDOWN);
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_COUNTDOWN:
        out.push_back(ARCADE_STATE_RUNNING);
        out.push_back(ARCADE_STATE_PAUSED);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_RUNNING:
        out.push_back(ARCADE_STATE_LAP_HANDOFF);
        out.push_back(ARCADE_STATE_PAUSED);
        out.push_back(ARCADE_STATE_FINAL_LAP);
        out.push_back(ARCADE_STATE_FINISH_SEQUENCE);
        out.push_back(ARCADE_STATE_RESULTS);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_LAP_HANDOFF:
        out.push_back(ARCADE_STATE_RUNNING);
        out.push_back(ARCADE_STATE_FINAL_LAP);
        out.push_back(ARCADE_STATE_PAUSED);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_PAUSED:
        out.push_back(ARCADE_STATE_RUNNING);
        out.push_back(ARCADE_STATE_LAP_HANDOFF);
        out.push_back(ARCADE_STATE_FINAL_LAP);
        out.push_back(ARCADE_STATE_FINISH_SEQUENCE);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_FINAL_LAP:
        out.push_back(ARCADE_STATE_LAP_HANDOFF);
        out.push_back(ARCADE_STATE_PAUSED);
        out.push_back(ARCADE_STATE_FINISH_SEQUENCE);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_FINISH_SEQUENCE:
        out.push_back(ARCADE_STATE_RESULTS);
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_RESULTS:
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_RESETTING:
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_ERROR_RECOVERABLE);
        break;
    case ARCADE_STATE_ERROR_RECOVERABLE:
        out.push_back(ARCADE_STATE_RESETTING);
        out.push_back(ARCADE_STATE_READY_FOR_PLAYERS);
        out.push_back(ARCADE_STATE_ATTRACT);
        out.push_back(ARCADE_STATE_ERROR_FATAL);
        out.push_back(ARCADE_STATE_SHUTDOWN);
        break;
    case ARCADE_STATE_ERROR_FATAL:
        out.push_back(ARCADE_STATE_SHUTDOWN);
        break;
    case ARCADE_STATE_SHUTDOWN:
        break;
    }
    return out;
}

std::vector<std::string>
ArcadeSessionStateMachine::allowedOperatorCommands(ArcadeSessionState state)
{
    std::vector<std::string> out;
    switch (state)
    {
    case ARCADE_STATE_BOOT:
    case ARCADE_STATE_IDLE:
    case ARCADE_STATE_ATTRACT:
        out.push_back("configure");
        out.push_back("reset");
        break;
    case ARCADE_STATE_READY_FOR_PLAYERS:
    case ARCADE_STATE_SESSION_CONFIGURED:
        out.push_back("configure");
        out.push_back("start");
        out.push_back("reset");
        out.push_back("render-preset");
        break;
    case ARCADE_STATE_BRIEFING:
        out.push_back("countdown");
        out.push_back("reset");
        out.push_back("abort");
        break;
    case ARCADE_STATE_COUNTDOWN:
        out.push_back("pause");
        out.push_back("reset");
        out.push_back("abort");
        break;
    case ARCADE_STATE_RUNNING:
    case ARCADE_STATE_FINAL_LAP:
        out.push_back("pause");
        out.push_back("abort");
        out.push_back("reset");
        out.push_back("force-finish");
        out.push_back("handoff/confirm");
        out.push_back("render-preset");
        break;
    case ARCADE_STATE_LAP_HANDOFF:
        out.push_back("handoff/confirm");
        out.push_back("pause");
        out.push_back("abort");
        out.push_back("reset");
        out.push_back("force-finish");
        break;
    case ARCADE_STATE_PAUSED:
        out.push_back("resume");
        out.push_back("abort");
        out.push_back("reset");
        out.push_back("force-finish");
        break;
    case ARCADE_STATE_FINISH_SEQUENCE:
        out.push_back("next-state");
        out.push_back("reset");
        break;
    case ARCADE_STATE_RESULTS:
        out.push_back("configure");
        out.push_back("reset");
        out.push_back("start");
        break;
    case ARCADE_STATE_RESETTING:
        break;
    case ARCADE_STATE_ERROR_RECOVERABLE:
        out.push_back("reset");
        out.push_back("configure");
        break;
    case ARCADE_STATE_ERROR_FATAL:
    case ARCADE_STATE_SHUTDOWN:
        break;
    }
    return out;
}

bool ArcadeSessionStateMachine::canTransition(ArcadeSessionState from,
                                              ArcadeSessionState to)
{
    if (from == to)
        return true;
    const std::vector<ArcadeSessionState> allowed = allowedTransitions(from);
    return std::find(allowed.begin(), allowed.end(), to) != allowed.end();
}

bool ArcadeSessionStateMachine::transitionTo(ArcadeSessionState state,
                                             const std::string& reason,
                                             std::string* reject_reason)
{
    if (m_state == state)
        return true;

    if (!canTransition(m_state, state))
    {
        if (reject_reason)
            *reject_reason = "wrong_state";
        Log::warn("ImmersiveRelayArcade",
                  "Rejected transition %s -> %s reason=%s.",
                  stateToString(m_state), stateToString(state),
                  reason.c_str());
        return false;
    }

    Log::info("ImmersiveRelayArcade",
              "Exit state %s -> %s reason=%s.",
              stateToString(m_state), stateToString(state), reason.c_str());
    m_previous_state = m_state;
    m_state = state;
    m_transition_count++;
    Log::info("ImmersiveRelayArcade",
              "Enter state %s reason=%s.",
              stateToString(m_state), reason.c_str());
    return true;
}

bool ArcadeSessionStateMachine::fail(const std::string& error,
                                     bool recoverable,
                                     std::string* reject_reason)
{
    m_last_error = error;
    m_last_error_recoverable = recoverable;
    return transitionTo(recoverable ? ARCADE_STATE_ERROR_RECOVERABLE
                                    : ARCADE_STATE_ERROR_FATAL,
                        error, reject_reason);
}

void ArcadeSessionStateMachine::clearError()
{
    m_last_error.clear();
    m_last_error_recoverable = true;
}

#endif // ENABLE_IMMERSIVE_RELAY
