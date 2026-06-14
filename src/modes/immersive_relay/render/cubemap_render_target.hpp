//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_CUBEMAP_RENDER_TARGET_HPP
#define HEADER_CUBEMAP_RENDER_TARGET_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/camera/immersive_camera_rig.hpp"

#include <memory>
#include <string>

class RenderTarget;

namespace irr
{
    namespace scene { class ICameraSceneNode; }
}

namespace ImmersiveRelay
{

class CubemapRenderTarget
{
private:
    unsigned int m_size;
    std::unique_ptr<RenderTarget> m_faces[CUBEMAP_FACE_COUNT];
    irr::scene::ICameraSceneNode* m_camera_node;
    unsigned int m_face_textures[CUBEMAP_FACE_COUNT];
    unsigned int m_face_depth_textures[CUBEMAP_FACE_COUNT];
    unsigned int m_cube_texture;
    unsigned int m_depth_cube_texture;
    unsigned int m_copy_frame_buffer;
    unsigned int m_depth_copy_frame_buffer;
    bool m_logged_validation;

    bool copyFaceToCubeMap(unsigned int face, unsigned int source_texture);
    bool copyDepthFaceToCubeMap(unsigned int face,
                                unsigned int source_depth_texture);

public:
    CubemapRenderTarget();
    ~CubemapRenderTarget();

    bool ensure(unsigned int size);
    bool renderFaces(const ImmersiveCameraRig& rig, float dt);
    const unsigned int* getFaceTextures() const { return m_face_textures; }
    const unsigned int* getFaceDepthTextures() const
    {
        return m_face_depth_textures;
    }
    unsigned int getCubeTexture() const { return m_cube_texture; }
    unsigned int getDepthCubeTexture() const { return m_depth_cube_texture; }
    unsigned int getSize() const { return m_size; }
    void destroy();
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_CUBEMAP_RENDER_TARGET_HPP
