//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_FISHEYE_DOMEMASTER_PROJECTION_HPP
#define HEADER_FISHEYE_DOMEMASTER_PROJECTION_HPP

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/projection/projection_adapter.hpp"

#include <memory>

class FrameBuffer;

namespace ImmersiveRelay
{

class FisheyeDomemasterProjection : public ProjectionAdapter
{
private:
    unsigned int m_size;
    unsigned int m_texture;
    unsigned int m_publish_texture;
    unsigned int m_publish_texture_target;
    std::unique_ptr<FrameBuffer> m_frame_buffer;
    bool m_logged_validation;

public:
    FisheyeDomemasterProjection();
    ~FisheyeDomemasterProjection();

    std::string id() const { return "fisheye_domemaster"; }
    ImmersiveProjectionMode kind() const
    {
        return IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
    }
    bool configure(const ImmersiveRenderConfig& config);
    bool renderProjection(const FullSphereCapture& capture,
                          const ImmersiveRenderConfig& config,
                          ImmersiveRenderStats& stats);
    ProjectionOutputDescription outputDescription() const;
    bool ensure(unsigned int size);
    bool project(unsigned int cube_texture,
                 unsigned int depth_cube_texture,
                 unsigned int skybox_cube_texture,
                 const unsigned int* face_textures,
                 const unsigned int* face_depth_textures,
                 const ImmersiveRenderConfig& config);
    unsigned int getTexture() const { return m_publish_texture; }
    unsigned int getTextureTarget() const { return m_publish_texture_target; }
    unsigned int getSize() const { return m_size; }
    unsigned int getWidth() const { return m_size; }
    unsigned int getHeight() const { return m_size; }
    void destroy();
    bool blitToDefault() const;
};

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
#endif // HEADER_FISHEYE_DOMEMASTER_PROJECTION_HPP
