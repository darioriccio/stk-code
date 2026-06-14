//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/camera/immersive_camera_rig.hpp"

#ifndef SERVER_ONLY
#include "karts/abstract_kart.hpp"
#include "modes/world.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"

#include <ICameraSceneNode.h>
#include <algorithm>
#include <cmath>
#endif

namespace ImmersiveRelay
{

ImmersiveCameraPose::ImmersiveCameraPose()
{
    m_position = Vec3(0.0f, 0.0f, 0.0f);
    m_forward = Vec3(0.0f, 0.0f, 1.0f);
    m_right = Vec3(1.0f, 0.0f, 0.0f);
    m_up = Vec3(0.0f, 1.0f, 0.0f);
    m_active_driver_index = -1;
    m_target_kart_id = 0;
}

ImmersiveCameraRig::ImmersiveCameraRig()
{
    m_have_smoothed_pose = false;
    m_last_active_driver_index = -999;
    m_face_fov_deg = 100.0f;
}

bool ImmersiveCameraRig::update(const ImmersiveRenderConfig& config,
                                const RelaySessionSnapshot& snapshot,
                                float dt)
{
#ifdef SERVER_ONLY
    return false;
#else
    AbstractKart* kart = selectTargetKart(snapshot);
    if (!kart)
        return false;
    m_face_fov_deg = config.m_face_fov_deg;

    ImmersiveCameraPose desired;
    buildDesiredPose(config, snapshot, kart, &desired);

    const float smoothing = std::max(0.0f, std::min(1.0f,
        config.m_smoothing_factor * std::max(1.0f, dt * 60.0f)));
    if (!m_have_smoothed_pose ||
        desired.m_active_driver_index != m_last_active_driver_index)
    {
        m_pose = desired;
        m_have_smoothed_pose = true;
    }
    else
    {
        m_pose.m_position += (desired.m_position - m_pose.m_position) *
            smoothing;
        m_pose.m_forward = normalizedOr(
            m_pose.m_forward + (desired.m_forward - m_pose.m_forward) *
                smoothing,
            desired.m_forward);
        m_pose.m_up = normalizedOr(
            m_pose.m_up + (desired.m_up - m_pose.m_up) * smoothing,
            Vec3(0.0f, 1.0f, 0.0f));
        m_pose.m_right = normalizedOr(
            (Vec3)m_pose.m_up.cross(m_pose.m_forward),
            desired.m_right);
        m_pose.m_up = normalizedOr(
            (Vec3)m_pose.m_forward.cross(m_pose.m_right),
            Vec3(0.0f, 1.0f, 0.0f));
        m_pose.m_active_driver_index = desired.m_active_driver_index;
        m_pose.m_active_driver_id = desired.m_active_driver_id;
        m_pose.m_target_kart_id = desired.m_target_kart_id;
    }

    if (desired.m_active_driver_index != m_last_active_driver_index ||
        desired.m_active_driver_id != m_last_active_driver_id)
    {
        Log::info("ImmersiveCameraRig",
            "Camera target active_driver index=%d id=%s kart=%u.",
            desired.m_active_driver_index,
            desired.m_active_driver_id.c_str(),
            desired.m_target_kart_id);
        m_last_active_driver_index = desired.m_active_driver_index;
        m_last_active_driver_id = desired.m_active_driver_id;
    }
    return true;
#endif
}

void ImmersiveCameraRig::applyFaceToCameraNode(
    ImmersiveCubemapFace face,
    irr::scene::ICameraSceneNode* camera) const
{
#ifndef SERVER_ONLY
    if (!camera)
        return;

    Vec3 dir = m_pose.m_forward;
    Vec3 up = m_pose.m_up;
    switch (face)
    {
    case CUBEMAP_FACE_POS_X:
        dir = m_pose.m_right;
        up = m_pose.m_up;
        break;
    case CUBEMAP_FACE_NEG_X:
        dir = -m_pose.m_right;
        up = m_pose.m_up;
        break;
    case CUBEMAP_FACE_POS_Y:
        dir = m_pose.m_up;
        up = -m_pose.m_forward;
        break;
    case CUBEMAP_FACE_NEG_Y:
        dir = -m_pose.m_up;
        up = m_pose.m_forward;
        break;
    case CUBEMAP_FACE_NEG_Z:
        dir = -m_pose.m_forward;
        up = m_pose.m_up;
        break;
    case CUBEMAP_FACE_POS_Z:
    default:
        dir = m_pose.m_forward;
        up = m_pose.m_up;
        break;
    }

    const Vec3 target = m_pose.m_position + dir;
    camera->setPosition(m_pose.m_position.toIrrVector());
    camera->setTarget(target.toIrrVector());
    camera->setUpVector(up.toIrrVector());
    camera->setFOV(m_face_fov_deg * DEGREE_TO_RAD);
    camera->setAspectRatio(1.0f);
    camera->setNearValue(0.2f);
    if (Track::getCurrentTrack())
        camera->setFarValue(Track::getCurrentTrack()->getCameraFar());
#endif
}

#ifndef SERVER_ONLY
Vec3 ImmersiveCameraRig::normalizedOr(const Vec3& value,
                                      const Vec3& fallback)
{
    if (value.length2() < 0.000001f)
        return fallback;
    Vec3 out = value;
    out.normalize();
    return out;
}

Vec3 ImmersiveCameraRig::rotateAroundAxis(const Vec3& value,
                                          const Vec3& axis,
                                          float radians)
{
    Vec3 safe_axis = normalizedOr(axis, Vec3(0.0f, 1.0f, 0.0f));
    btQuaternion q(safe_axis, radians);
    return Vec3(quatRotate(q, value));
}

AbstractKart* ImmersiveCameraRig::selectTargetKart(
    const RelaySessionSnapshot& snapshot) const
{
    World* world = World::getWorld();
    if (!world || world->getNumKarts() == 0)
        return NULL;

    AbstractKart* kart = NULL;
    if (snapshot.m_rotation.m_active_index >= 0)
    {
        kart = world->getPlayerKart(
            (unsigned int)snapshot.m_rotation.m_active_index);
    }
    if (!kart)
        kart = world->getPlayerKart(0);
    if (!kart)
        kart = world->getKart(0);
    return kart;
}

void ImmersiveCameraRig::buildDesiredPose(
    const ImmersiveRenderConfig& config,
    const RelaySessionSnapshot& snapshot,
    AbstractKart* kart,
    ImmersiveCameraPose* pose) const
{
    const btTransform trans = kart->getSmoothedTrans();
    Vec3 forward = normalizedOr(
        Vec3(trans.getBasis() * Vec3(0.0f, 0.0f, 1.0f)),
        Vec3(0.0f, 0.0f, 1.0f));
    Vec3 right = normalizedOr(
        Vec3(trans.getBasis() * Vec3(1.0f, 0.0f, 0.0f)),
        Vec3(1.0f, 0.0f, 0.0f));
    Vec3 up = normalizedOr(
        Vec3(trans.getBasis() * Vec3(0.0f, 1.0f, 0.0f)),
        Vec3(0.0f, 1.0f, 0.0f));

    if (config.m_horizon_stabilization ||
        config.m_camera_mode == IMMERSIVE_CAMERA_STABILIZED_CHASE)
    {
        up = Vec3(0.0f, 1.0f, 0.0f);
        right = normalizedOr(Vec3(up.cross(forward)), right);
        up = normalizedOr(Vec3(forward.cross(right)), Vec3(0.0f, 1.0f, 0.0f));
    }
    else
    {
        const float roll = kart->getRoll();
        const float limit = config.m_roll_limit_deg * DEGREE_TO_RAD;
        const float clamped_roll = std::max(-limit, std::min(limit, roll));
        up = rotateAroundAxis(Vec3(0.0f, 1.0f, 0.0f), forward, clamped_roll);
        right = normalizedOr(Vec3(up.cross(forward)), right);
        up = normalizedOr(Vec3(forward.cross(right)), Vec3(0.0f, 1.0f, 0.0f));
    }

    Vec3 position;
    switch (config.m_camera_mode)
    {
    case IMMERSIVE_CAMERA_POV:
        position = Vec3(trans(Vec3(0.0f, 1.25f, 1.05f)));
        break;
    case IMMERSIVE_CAMERA_COCKPIT:
        position = Vec3(trans(Vec3(0.0f, 1.05f, 0.45f)));
        break;
    case IMMERSIVE_CAMERA_CHASE:
        position = Vec3(trans(Vec3(0.0f, config.m_vertical_offset,
                                   -config.m_forward_offset)));
        break;
    case IMMERSIVE_CAMERA_STABILIZED_CHASE:
    default:
        position = Vec3(trans(Vec3(0.0f, config.m_vertical_offset,
                                   -config.m_forward_offset)));
        break;
    }

    const float yaw = config.m_yaw_deg * DEGREE_TO_RAD;
    const float pitch = -config.m_pitch_deg * DEGREE_TO_RAD;
    const float roll = config.m_roll_deg * DEGREE_TO_RAD;

    forward = rotateAroundAxis(forward, up, yaw);
    right = normalizedOr(Vec3(up.cross(forward)), right);
    forward = rotateAroundAxis(forward, right, pitch);
    up = normalizedOr(Vec3(forward.cross(right)), up);
    up = rotateAroundAxis(up, forward, roll);
    right = normalizedOr(Vec3(up.cross(forward)), right);
    up = normalizedOr(Vec3(forward.cross(right)), up);

    pose->m_position = position;
    pose->m_forward = normalizedOr(forward, Vec3(0.0f, 0.0f, 1.0f));
    pose->m_right = normalizedOr(right, Vec3(1.0f, 0.0f, 0.0f));
    pose->m_up = normalizedOr(up, Vec3(0.0f, 1.0f, 0.0f));
    pose->m_active_driver_index = snapshot.m_rotation.m_active_index;
    pose->m_active_driver_id = snapshot.m_rotation.m_active_player_id;
    pose->m_target_kart_id = kart->getWorldKartId();
}
#endif

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
