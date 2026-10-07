//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_FULL_SPHERE_CAPTURE_HPP
#define HEADER_FULL_SPHERE_CAPTURE_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/camera/immersive_camera_rig.hpp"
#include "modes/immersive_relay/render/cubemap_render_target.hpp"

namespace ImmersiveRelay
{

class FullSphereCapture
{
private:
    std::unique_ptr<CubemapRenderTarget> m_cubemap_target;

public:
    FullSphereCapture() : m_cubemap_target(new CubemapRenderTarget()) {}
    ~FullSphereCapture() { destroy(); }

    bool ensure(unsigned int cubemap_size)
    {
        if (m_cubemap_target->getSize() == cubemap_size)
            return m_cubemap_target->ensure(cubemap_size);
        std::unique_ptr<CubemapRenderTarget> next(new CubemapRenderTarget());
        if (!next->ensure(cubemap_size)) return false;
        m_cubemap_target.swap(next);
        return true;
    }
    bool render(const ImmersiveCameraRig& rig, float dt)
    {
        return m_cubemap_target->renderFaces(rig, dt);
    }
    const char* type() const { return "full_sphere_cubemap"; }
    unsigned int getFaceCount() const { return CUBEMAP_FACE_COUNT; }
    unsigned int getSize() const { return m_cubemap_target->getSize(); }
    unsigned int getCubeTexture() const
    {
        return m_cubemap_target->getCubeTexture();
    }
    unsigned int getDepthCubeTexture() const
    {
        return m_cubemap_target->getDepthCubeTexture();
    }
    const unsigned int* getFaceTextures() const
    {
        return m_cubemap_target->getFaceTextures();
    }
    const unsigned int* getFaceDepthTextures() const
    {
        return m_cubemap_target->getFaceDepthTextures();
    }
    void destroy() { m_cubemap_target->destroy(); }
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_FULL_SPHERE_CAPTURE_HPP
