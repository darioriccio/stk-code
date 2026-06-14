//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_main_hooks.hpp"

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"
#include "race/race_manager.hpp"
#include "utils/command_line.hpp"
#include "utils/log.hpp"

#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/local_control_server.hpp"
#endif

#include <cstdlib>
#include <string>

namespace RelayMainHooks
{

namespace
{
bool readFloatOption(const char* option, float* value)
{
    std::string raw;
    if (!CommandLine::has(option, &raw))
        return false;
    *value = (float)atof(raw.c_str());
    return true;
}
}

void handleCommandLineOptions()
{
    std::string s;
    int n = 0;
    ImmersiveRelay::ImmersiveRenderConfig render_config =
        ImmersiveRelay::ImmersiveRenderPipeline::get()->getConfig();
    bool render_config_seen = false;
    bool projection_seen = false;

    if (CommandLine::has("--immersive-relay"))
    {
        RelaySessionController::get()->setRuntimeEnabled(true);
        RaceManager::get()->setMinorMode(RaceManager::MINOR_MODE_NORMAL_RACE);
    }
    if (CommandLine::has("--relay-arcade-ui"))
    {
        RelaySessionController::get()->setRuntimeEnabled(true);
        RelaySessionController::get()->setArcadeUIEnabled(true);
        RaceManager::get()->setMinorMode(RaceManager::MINOR_MODE_NORMAL_RACE);
    }
    if (CommandLine::has("--relay-session-preset", &s))
    {
        RelaySessionController::get()->setDefaultPreset(s);
    }
    if (CommandLine::has("--relay-player-count", &n))
    {
        RelaySessionController::get()->setDefaultPlayerCount(n);
    }
    if (CommandLine::has("--relay-handoff-policy", &s))
    {
        if (!RelaySessionController::get()->setDefaultHandoffPolicy(s))
            Log::warn("main", "Unknown relay handoff policy '%s'.", s.c_str());
    }

    if (CommandLine::has("--immersive-output", &s))
    {
        render_config_seen = true;
        if (!ImmersiveRelay::parseOutputMode(s, &render_config.m_output_mode))
            Log::warn("main", "Unknown immersive output mode '%s'.", s.c_str());
    }
    if (CommandLine::has("--immersive-projection", &s))
    {
        render_config_seen = true;
        projection_seen = true;
        if (!ImmersiveRelay::parseProjectionMode(
                s, &render_config.m_projection_mode))
        {
            Log::warn("main", "Unknown immersive projection '%s'.", s.c_str());
        }
    }
    if (CommandLine::has("--immersive-domemaster-size", &n))
    {
        render_config_seen = true;
        const unsigned int sanitized =
            ImmersiveRelay::sanitizeDomemasterSize(n);
        if ((int)sanitized != n)
            Log::warn("main",
                "Invalid immersive domemaster size %d, using %u.",
                n, sanitized);
        render_config.m_domemaster_size = sanitized;
        render_config.m_projection_width = sanitized;
        render_config.m_projection_height = sanitized;
    }
    if (CommandLine::has("--immersive-cubemap-size", &n))
    {
        render_config_seen = true;
        const unsigned int sanitized = ImmersiveRelay::sanitizeCubemapSize(n);
        if ((int)sanitized != n)
            Log::warn("main",
                "Invalid immersive cubemap size %d, using %u.", n, sanitized);
        render_config.m_cubemap_size = sanitized;
    }
    if (CommandLine::has("--immersive-camera", &s))
    {
        render_config_seen = true;
        if (!ImmersiveRelay::parseCameraMode(s, &render_config.m_camera_mode))
            Log::warn("main", "Unknown immersive camera '%s'.", s.c_str());
    }
    if (readFloatOption("--immersive-yaw-deg", &render_config.m_yaw_deg))
        render_config_seen = true;
    if (readFloatOption("--immersive-pitch-deg", &render_config.m_pitch_deg))
        render_config_seen = true;
    if (readFloatOption("--immersive-roll-deg", &render_config.m_roll_deg))
        render_config_seen = true;
    if (readFloatOption("--immersive-dome-tilt-deg",
                        &render_config.m_dome_tilt_deg))
        render_config_seen = true;
    if (CommandLine::has("--immersive-horizon-stabilization", &s))
    {
        render_config_seen = true;
        if (!ImmersiveRelay::parseOnOff(
                s, &render_config.m_horizon_stabilization))
        {
            Log::warn("main",
                "Unknown immersive horizon stabilization value '%s'.",
                s.c_str());
        }
    }
    if (readFloatOption("--immersive-roll-limit-deg",
                        &render_config.m_roll_limit_deg))
        render_config_seen = true;
    if (readFloatOption("--immersive-forward-offset",
                        &render_config.m_forward_offset))
        render_config_seen = true;
    if (readFloatOption("--immersive-vertical-offset",
                        &render_config.m_vertical_offset))
        render_config_seen = true;
    if (readFloatOption("--immersive-smoothing-factor",
                        &render_config.m_smoothing_factor))
        render_config_seen = true;
    if (CommandLine::has("--immersive-debug-grid", &s))
    {
        render_config_seen = true;
        if (!ImmersiveRelay::parseOnOff(s, &render_config.m_debug_grid))
            Log::warn("main", "Unknown immersive debug grid value '%s'.",
                      s.c_str());
    }
    if (CommandLine::has("--immersive-stats", &s))
    {
        render_config_seen = true;
        if (!ImmersiveRelay::parseOnOff(s, &render_config.m_stats_enabled))
            Log::warn("main", "Unknown immersive stats value '%s'.",
                      s.c_str());
    }
    if (CommandLine::has("--syphon-server-name", &s))
    {
        render_config_seen = true;
        render_config.m_syphon_server_name = s;
    }
    if (render_config_seen)
    {
        if (!projection_seen &&
            render_config.m_output_mode != ImmersiveRelay::IMMERSIVE_OUTPUT_NONE)
        {
            render_config.m_projection_mode =
                ImmersiveRelay::IMMERSIVE_PROJECTION_FISHEYE;
        }
        ImmersiveRelay::syncLegacyDomemasterSize(&render_config);
        if (render_config.m_output_mode !=
                ImmersiveRelay::IMMERSIVE_OUTPUT_NONE &&
            !ImmersiveRelay::projectionIsAvailable(
                render_config.m_projection_mode))
        {
            Log::warn("main",
                "Immersive projection '%s' is %s in this build; only fisheye_domemaster is production in Phase 4.6.",
                ImmersiveRelay::projectionKindToString(
                    render_config.m_projection_mode),
                ImmersiveRelay::projectionStatusToString(
                    render_config.m_projection_mode));
        }
#ifndef ENABLE_SYPHON_OUTPUT
        if (render_config.m_output_mode ==
            ImmersiveRelay::IMMERSIVE_OUTPUT_SYPHON)
        {
            Log::warn("main",
                "Syphon output requested, but this binary was built without ENABLE_SYPHON_OUTPUT.");
        }
#endif
        ImmersiveRelay::ImmersiveRenderPipeline::get()->setConfig(
            render_config);
    }
#ifdef ENABLE_RELAY_TEST_TOOLS
    if (CommandLine::has("--relay-test-accelerated-progress"))
    {
        RelaySessionController::get()->setRuntimeEnabled(true);
        RelaySessionController::get()->setTestAcceleratedProgressEnabled(true);
    }
#endif

#ifdef ENABLE_RELAY_LOCAL_API
    bool relay_api_requested = CommandLine::has("--relay-local-api");
    std::string relay_api_bind;
    std::string relay_api_token;
    int relay_api_port = 0;
    if (CommandLine::has("--relay-api-bind", &relay_api_bind))
        relay_api_requested = true;
    if (CommandLine::has("--relay-api-port", &relay_api_port))
        relay_api_requested = true;
    if (CommandLine::has("--relay-api-token", &relay_api_token))
        relay_api_requested = true;
    if (relay_api_requested)
    {
        RelaySessionController::get()->setRuntimeEnabled(true);
        RelaySessionController::get()->setLocalApiOptions(
            true, relay_api_bind, relay_api_port, relay_api_token);
    }
#endif
}

void applyRaceDefaultsAfterLapOptions()
{
    if (!RelaySessionController::get()->isRuntimeEnabled())
        return;

    std::string s;
    const RelaySessionConfig relay_config =
        RelaySessionController::get()->getDefaultConfig();
    const bool relay_laps_explicit = CommandLine::has("--laps", &s);
    const int requested_laps = relay_laps_explicit
        ? RaceManager::get()->getNumLaps() : 0;
    const int relay_laps =
        RelaySessionController::getRecommendedLapCount(
            relay_config.m_player_count, requested_laps);

    if (RaceManager::get()->getNumLaps() != relay_laps)
    {
        RaceManager::get()->setNumLaps(relay_laps);
        Log::info("main",
            "Immersive Relay adjusted race length to %d laps for %d player(s).",
            relay_laps, relay_config.m_player_count);
    }
}

void startLocalApiIfEnabled()
{
#ifdef ENABLE_RELAY_LOCAL_API
    RelayLocalApiOptions relay_api_options =
        RelaySessionController::get()->getLocalApiOptions();
    if (relay_api_options.m_enabled)
    {
        LocalControlServer::get()->start(relay_api_options.m_bind_host,
                                         relay_api_options.m_port,
                                         relay_api_options.m_token);
    }
#endif
}

void shutdown()
{
    ImmersiveRelay::ImmersiveRenderPipeline::get()->shutdown();
    ImmersiveRelay::ImmersiveRenderPipeline::destroy();
#ifdef ENABLE_RELAY_LOCAL_API
    LocalControlServer::get()->stop();
    LocalControlServer::destroy();
#endif
    RelaySessionController::destroy();
}

} // namespace RelayMainHooks

#endif // ENABLE_IMMERSIVE_RELAY
