//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_IMMERSIVE_CAMERA_RIG_HPP
#define HEADER_IMMERSIVE_CAMERA_RIG_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "modes/immersive_relay/render/immersive_render_config.hpp"
#include "utils/vec3.hpp"

namespace irr
{
    namespace scene { class ICameraSceneNode; }
}

class AbstractKart;

namespace ImmersiveRelay
{

enum ImmersiveCubemapFace
{
    CUBEMAP_FACE_POS_X = 0,
    CUBEMAP_FACE_NEG_X = 1,
    CUBEMAP_FACE_POS_Y = 2,
    CUBEMAP_FACE_NEG_Y = 3,
    CUBEMAP_FACE_POS_Z = 4,
    CUBEMAP_FACE_NEG_Z = 5,
    CUBEMAP_FACE_COUNT = 6
};

struct ImmersiveCameraPose
{
    Vec3 m_position;
    Vec3 m_forward;
    Vec3 m_right;
    Vec3 m_up;
    int m_active_driver_index;
    std::string m_active_driver_id;
    unsigned int m_target_kart_id;

    ImmersiveCameraPose();
};

class ImmersiveCameraRig
{
private:
    bool m_have_smoothed_pose;
    int m_last_active_driver_index;
    std::string m_last_active_driver_id;
    ImmersiveCameraPose m_pose;
    float m_face_fov_deg;

    static Vec3 normalizedOr(const Vec3& value, const Vec3& fallback);
    static Vec3 rotateAroundAxis(const Vec3& value, const Vec3& axis,
                                 float radians);
    AbstractKart* selectTargetKart(
        const RelaySessionSnapshot& snapshot) const;
    void buildDesiredPose(const ImmersiveRenderConfig& config,
                          const RelaySessionSnapshot& snapshot,
                          AbstractKart* kart,
                          ImmersiveCameraPose* pose) const;

public:
    ImmersiveCameraRig();
    bool update(const ImmersiveRenderConfig& config,
                const RelaySessionSnapshot& snapshot,
                float dt);
    const ImmersiveCameraPose& getPose() const { return m_pose; }
    void applyFaceToCameraNode(ImmersiveCubemapFace face,
                               irr::scene::ICameraSceneNode* camera) const;
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_IMMERSIVE_CAMERA_RIG_HPP
