//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"

#ifndef SERVER_ONLY
#include "modes/immersive_relay/output/debug_window/debug_window_output_adapter.hpp"
#include "modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.hpp"
#include "modes/immersive_relay/render/camera/immersive_camera_rig.hpp"
#include "modes/immersive_relay/render/full_sphere_capture.hpp"
#include "modes/immersive_relay/render/projection/fisheye_domemaster_projection.hpp"
#include "modes/immersive_relay/render/projection/projection_adapter.hpp"

#include <memory>
#endif

namespace ImmersiveRelay
{

ImmersiveRenderPipeline* ImmersiveRenderPipeline::m_instance = NULL;

#ifndef SERVER_ONLY
namespace
{
std::unique_ptr<ImmersiveCameraRig> g_camera_rig;
std::unique_ptr<FullSphereCapture> g_capture;
std::unique_ptr<ProjectionAdapter> g_projection;
std::unique_ptr<DebugWindowOutputAdapter> g_window_output;
std::unique_ptr<SyphonOutputAdapterMac> g_syphon_output;

float elapsedMs(uint64_t start)
{
    return (float)(StkTime::getMonoTimeMs() - start);
}

ProjectionAdapter* ensureProjectionAdapter(ImmersiveProjectionMode mode)
{
    if (!projectionIsAvailable(mode))
        return NULL;

    if (!g_projection.get() || g_projection->kind() != mode)
    {
        if (g_projection.get())
            g_projection->destroy();
        if (mode == IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER)
            g_projection.reset(new FisheyeDomemasterProjection());
    }
    return g_projection.get();
}
}
#endif

ImmersiveRenderPipeline::ImmersiveRenderPipeline()
{
    m_configured = false;
}

ImmersiveRenderPipeline* ImmersiveRenderPipeline::get()
{
    if (!m_instance)
        m_instance = new ImmersiveRenderPipeline();
    return m_instance;
}

void ImmersiveRenderPipeline::destroy()
{
    delete m_instance;
    m_instance = NULL;
}

void ImmersiveRenderPipeline::setConfig(const ImmersiveRenderConfig& config)
{
    m_config = config;
    syncLegacyDomemasterSize(&m_config);
    m_configured = true;
    m_stats.beginFrame(m_config);
    if (m_config.m_projection_mode != IMMERSIVE_PROJECTION_NONE &&
        !projectionIsAvailable(m_config.m_projection_mode))
    {
        m_stats.m_fallback_reason = std::string("projection_unavailable:") +
            projectionKindToString(m_config.m_projection_mode);
    }
    Log::info("ImmersiveRenderPipeline",
        "Configured capture=full_sphere_cubemap output=%s projection=%s status=%s size=%ux%u cubemap=%u camera=%s.",
        outputModeToString(config.m_output_mode),
        projectionKindToString(m_config.m_projection_mode),
        projectionStatusToString(m_config.m_projection_mode),
        m_config.m_projection_width,
        m_config.m_projection_height,
        m_config.m_cubemap_size,
        cameraModeToString(config.m_camera_mode));
}

void ImmersiveRenderPipeline::renderFrame(float dt)
{
    if (!m_configured || !m_config.isEnabled() ||
        !RelaySessionController::get()->isRuntimeEnabled())
        return;

    RelaySessionSnapshot snapshot = RelaySessionController::get()->getSnapshot();
    m_stats.beginFrame(m_config);
    m_stats.m_active_driver_index = snapshot.m_rotation.m_active_index;
    m_stats.m_active_driver_id = snapshot.m_rotation.m_active_player_id;
    m_stats.m_lap = snapshot.m_current_lap;
    m_stats.m_total_laps = snapshot.m_total_laps;
    m_stats.m_current_encounter = snapshot.m_encounter.m_id;

#ifdef SERVER_ONLY
    m_stats.m_fallback_reason = "SERVER_ONLY build has no graphics renderer";
    m_stats.m_true_fulldome = false;
    m_stats.m_publish_skipped++;
    return;
#else
    if (!g_camera_rig.get())
        g_camera_rig.reset(new ImmersiveCameraRig());
    if (!g_capture.get())
        g_capture.reset(new FullSphereCapture());
    ProjectionAdapter* projection =
        ensureProjectionAdapter(m_config.m_projection_mode);
    if (!projection)
    {
        m_stats.m_fallback_reason = std::string("projection_unavailable:") +
            projectionKindToString(m_config.m_projection_mode);
        m_stats.m_true_fulldome = false;
        m_stats.m_publish_skipped++;
        return;
    }

    const uint64_t frame_start = StkTime::getMonoTimeMs();

    if (!g_camera_rig->update(m_config, snapshot, dt))
    {
        m_stats.m_fallback_reason = "no active relay camera target";
        m_stats.m_true_fulldome = false;
        m_stats.m_publish_skipped++;
        return;
    }

    const uint64_t capture_start = StkTime::getMonoTimeMs();
    if (!g_capture->ensure(m_config.m_cubemap_size) ||
        !g_capture->render(*g_camera_rig, dt))
    {
        m_stats.m_fallback_reason = "full-sphere cubemap capture failed";
        m_stats.m_true_fulldome = false;
        m_stats.m_publish_skipped++;
        return;
    }
    m_stats.m_capture_time_ms = elapsedMs(capture_start);
    m_stats.m_cubemap_render_time_ms = m_stats.m_capture_time_ms;

    const uint64_t projection_start = StkTime::getMonoTimeMs();
    if (!projection->configure(m_config) ||
        !projection->renderProjection(*g_capture, m_config, m_stats))
    {
        m_stats.m_fallback_reason = "full-sphere to projection pass failed";
        m_stats.m_true_fulldome = false;
        m_stats.m_publish_skipped++;
        return;
    }
    m_stats.m_projection_time_ms = elapsedMs(projection_start);
    m_stats.m_true_fulldome = true;
    m_stats.m_projection_width = projection->getWidth();
    m_stats.m_projection_height = projection->getHeight();

    ImmersiveOutputFrame frame;
    frame.m_texture = projection->getTexture();
    frame.m_texture_target = projection->getTextureTarget();
    frame.m_width = projection->getWidth();
    frame.m_height = projection->getHeight();
    frame.m_projection_mode = projection->kind();
    frame.m_projection_id = projection->id();
    frame.m_server_name = m_config.m_syphon_server_name;

    bool published = false;
    float publish_ms = 0.0f;
    if (m_config.m_output_mode == IMMERSIVE_OUTPUT_WINDOW)
    {
        if (!g_window_output.get())
            g_window_output.reset(new DebugWindowOutputAdapter());
        published = g_window_output->publishFrame(frame, &publish_ms);
        m_stats.m_output_active = g_window_output->isActive();
        m_stats.m_output_publish_time_ms = publish_ms;
    }
    else if (m_config.m_output_mode == IMMERSIVE_OUTPUT_SYPHON)
    {
        if (!g_syphon_output.get())
            g_syphon_output.reset(new SyphonOutputAdapterMac());
        if (g_syphon_output->initialize(m_config.m_syphon_server_name,
                                        frame.m_width,
                                        frame.m_height))
        {
            published = g_syphon_output->publishFrame(frame, &publish_ms);
            m_stats.m_output_active = g_syphon_output->isActive();
            m_stats.m_output_publish_time_ms = publish_ms;
            m_stats.m_syphon_active = g_syphon_output->isActive();
            m_stats.m_syphon_publish_time_ms = publish_ms;
        }
        else
        {
            m_stats.m_fallback_reason =
                "Syphon output requested but adapter did not initialize";
            m_stats.m_true_fulldome = false;
        }
    }

    if (!published)
        m_stats.m_publish_skipped++;
    m_stats.recordFrame(elapsedMs(frame_start));

    if (m_config.m_stats_enabled)
    {
        Log::info("ImmersiveRenderStats",
            "mode=%s projection=%s size=%u cubemap=%u fps=%.1f frame=%.1fms last=%.1fms p95=%.1fms p99=%.1fms cubemap=%.1fms projection=%.1fms syphon=%.1fms skipped=%u capture=%.1fms output=%.1fms active_driver=%d lap=%d/%d server='%s'",
            outputModeToString(m_stats.m_output_mode),
            projectionModeToString(m_stats.m_projection_mode),
            m_stats.m_projection_width,
            m_stats.m_cubemap_size,
            m_stats.m_avg_fps,
            m_stats.m_avg_frame_time_ms,
            m_stats.m_last_frame_time_ms,
            m_stats.m_p95_frame_time_ms,
            m_stats.m_p99_frame_time_ms,
            m_stats.m_cubemap_render_time_ms,
            m_stats.m_projection_time_ms,
            m_stats.m_syphon_publish_time_ms,
            m_stats.m_publish_skipped,
            m_stats.m_capture_time_ms,
            m_stats.m_output_publish_time_ms,
            m_stats.m_active_driver_index,
            m_stats.m_lap,
            m_stats.m_total_laps,
            m_stats.m_syphon_server_name.c_str());
    }
#endif
}

void ImmersiveRenderPipeline::shutdown()
{
#ifndef SERVER_ONLY
    if (g_syphon_output.get())
        g_syphon_output->shutdown();
    if (g_window_output.get())
        g_window_output->shutdown();
    if (g_projection.get())
        g_projection->destroy();
    if (g_capture.get())
        g_capture->destroy();
    g_syphon_output.reset();
    g_window_output.reset();
    g_projection.reset();
    g_capture.reset();
    g_camera_rig.reset();
#endif
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
