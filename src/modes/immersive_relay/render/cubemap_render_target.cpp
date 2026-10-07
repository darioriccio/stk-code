//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/cubemap_render_target.hpp"

#ifndef SERVER_ONLY
#include "graphics/gl_headers.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/render_target.hpp"
#include "utils/log.hpp"

#include <dimension2d.h>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>
#endif

namespace ImmersiveRelay
{

#ifndef SERVER_ONLY
namespace
{
void dumpCubemapFaceOnce(unsigned int face, unsigned int size)
{
    static bool dumped[CUBEMAP_FACE_COUNT] = { false, false, false,
                                               false, false, false };
    if (face >= CUBEMAP_FACE_COUNT || dumped[face])
        return;

    const char* dir = std::getenv("STK_IMMERSIVE_CUBEMAP_FACE_DUMP_DIR");
    if (!dir || dir[0] == '\0')
        return;

    char path[1024];
    std::snprintf(path, sizeof(path), "%s/immersive_cubemap_face_%u.ppm",
                  dir, face);

    std::vector<unsigned char> pixels((size_t)size * (size_t)size * 4, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, (GLsizei)size, (GLsizei)size, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());

    FILE* file = std::fopen(path, "wb");
    if (!file)
    {
        Log::warn("CubemapRenderTarget",
                  "Cannot write cubemap face debug dump '%s'.", path);
        dumped[face] = true;
        return;
    }

    std::fprintf(file, "P6\n%u %u\n255\n", size, size);
    for (int y = (int)size - 1; y >= 0; y--)
    {
        for (unsigned int x = 0; x < size; x++)
        {
            const size_t index = ((size_t)y * (size_t)size + x) * 4;
            std::fwrite(&pixels[index], 1, 3, file);
        }
    }
    std::fclose(file);
    Log::info("CubemapRenderTarget",
              "Wrote cubemap face debug dump '%s'.", path);
    dumped[face] = true;
}

void dumpCubemapDepthFaceOnce(unsigned int face, unsigned int size)
{
    static bool dumped[CUBEMAP_FACE_COUNT] = { false, false, false,
                                               false, false, false };
    if (face >= CUBEMAP_FACE_COUNT || dumped[face])
        return;

    const char* dir = std::getenv("STK_IMMERSIVE_CUBEMAP_DEPTH_DUMP_DIR");
    if (!dir || dir[0] == '\0')
        return;

    char path[1024];
    std::snprintf(path, sizeof(path),
                  "%s/immersive_cubemap_depth_face_%u.pgm", dir, face);

    std::vector<float> depth((size_t)size * (size_t)size, 1.0f);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, (GLsizei)size, (GLsizei)size, GL_DEPTH_COMPONENT,
                 GL_FLOAT, depth.data());

    FILE* file = std::fopen(path, "wb");
    if (!file)
    {
        Log::warn("CubemapRenderTarget",
                  "Cannot write cubemap depth debug dump '%s'.", path);
        dumped[face] = true;
        return;
    }

    std::fprintf(file, "P5\n%u %u\n255\n", size, size);
    for (int y = (int)size - 1; y >= 0; y--)
    {
        for (unsigned int x = 0; x < size; x++)
        {
            const size_t index = (size_t)y * (size_t)size + x;
            unsigned char v =
                (unsigned char)(std::max(0.0f, std::min(1.0f, depth[index])) *
                                255.0f);
            std::fwrite(&v, 1, 1, file);
        }
    }
    std::fclose(file);
    Log::info("CubemapRenderTarget",
              "Wrote cubemap depth debug dump '%s'.", path);
    dumped[face] = true;
}
}
#endif

CubemapRenderTarget::CubemapRenderTarget()
{
    m_size = 0;
    m_camera_node = NULL;
    m_cube_texture = 0;
    m_depth_cube_texture = 0;
    m_copy_frame_buffer = 0;
    m_depth_copy_frame_buffer = 0;
    m_logged_validation = false;
    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        m_face_textures[i] = 0;
        m_face_depth_textures[i] = 0;
    }
}

CubemapRenderTarget::~CubemapRenderTarget()
{
    destroy();
}

bool CubemapRenderTarget::ensure(unsigned int size)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (m_size == size && m_camera_node && m_faces[0].get())
        return true;

    GLint maximum = 0;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maximum);
    if (!size || size > (unsigned int)maximum) return false;
    destroy();
    m_size = size;
    m_camera_node = irr_driver->addCameraSceneNode();
    if (!m_camera_node)
    {
        Log::error("CubemapRenderTarget",
                   "Cannot create immersive camera node.");
        m_size = 0;
        return false;
    }

    irr::core::dimension2du dimension(size, size);
    glGenTextures(1, (GLuint*)&m_cube_texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)m_cube_texture);
    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8,
                     (GLsizei)size, (GLsizei)size, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_WRAP_R
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glGenTextures(1, (GLuint*)&m_depth_cube_texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)m_depth_cube_texture);
    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                     GL_DEPTH_COMPONENT24, (GLsizei)size, (GLsizei)size,
                     0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_WRAP_R
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
#ifdef GL_TEXTURE_CUBE_MAP_SEAMLESS
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
    glGenFramebuffers(1, (GLuint*)&m_copy_frame_buffer);
    glGenFramebuffers(1, (GLuint*)&m_depth_copy_frame_buffer);

    bool allocation_valid = m_cube_texture && m_depth_cube_texture &&
                            m_copy_frame_buffer && m_depth_copy_frame_buffer;
    const GLuint cubes[2] = { (GLuint)m_cube_texture, (GLuint)m_depth_cube_texture };
    for (unsigned int c=0; c<2 && allocation_valid; c++)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubes[c]);
        for (unsigned int i=0; i<CUBEMAP_FACE_COUNT && allocation_valid; i++)
        {
            GLint width=0, height=0;
            glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_TEXTURE_WIDTH, &width);
            glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_TEXTURE_HEIGHT, &height);
            allocation_valid = width == (GLint)size && height == (GLint)size;
        }
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    if (!allocation_valid)
    {
        Log::error("CubemapRenderTarget", "Cannot allocate %ux%u sphere capture.", size, size);
        destroy(); return false;
    }
    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        std::ostringstream name;
        name << "immersive_relay_cubemap_face_" << i;
        m_faces[i] = irr_driver->createRenderTarget(dimension, name.str());
        if (!m_faces[i].get())
        {
            Log::error("CubemapRenderTarget",
                       "Cannot create cubemap face %u.", i);
            destroy();
            return false;
        }
    }
    Log::info("CubemapRenderTarget",
        "Created six %ux%u immersive cubemap-equivalent render targets.",
        size, size);
    Log::info("CubemapRenderTarget",
        "Face order validation: 0=+X/right, 1=-X/left, 2=+Y/up, 3=-Y/down, 4=+Z/forward, 5=-Z/back; faces are copied into a GL_TEXTURE_CUBE_MAP for seamless fisheye sampling.");
    return true;
#endif
}

bool CubemapRenderTarget::copyFaceToCubeMap(unsigned int face,
                                            unsigned int source_texture)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (face >= CUBEMAP_FACE_COUNT || source_texture == 0 ||
        m_cube_texture == 0 || m_copy_frame_buffer == 0)
        return false;

    GLint previous_read_fbo = 0;
    GLint previous_draw_fbo = 0;
    GLint previous_cube_texture = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previous_cube_texture);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_copy_frame_buffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, (GLuint)source_texture, 0);
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE)
    {
        Log::warn("CubemapRenderTarget",
                  "Cannot copy face %u into cube map: read FBO incomplete.",
                  face);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_cube_texture);
        return false;
    }

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    dumpCubemapFaceOnce(face, m_size);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)m_cube_texture);
    glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                        0, 0, 0, 0, (GLsizei)m_size, (GLsizei)m_size);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_cube_texture);
    return true;
#endif
}

bool CubemapRenderTarget::copyDepthFaceToCubeMap(
    unsigned int face,
    unsigned int source_depth_texture)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (face >= CUBEMAP_FACE_COUNT || source_depth_texture == 0 ||
        m_depth_cube_texture == 0 || m_copy_frame_buffer == 0 ||
        m_depth_copy_frame_buffer == 0)
        return false;

    GLint previous_read_fbo = 0;
    GLint previous_draw_fbo = 0;
    GLint previous_cube_texture = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previous_cube_texture);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_copy_frame_buffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, (GLuint)source_depth_texture, 0);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                      (GLuint)m_depth_copy_frame_buffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                           (GLuint)m_depth_cube_texture, 0);
    glDrawBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE ||
        glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) !=
            GL_FRAMEBUFFER_COMPLETE)
    {
        Log::warn("CubemapRenderTarget",
                  "Cannot copy depth face %u into cube map: FBO incomplete.",
                  face);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_cube_texture);
        return false;
    }

    dumpCubemapDepthFaceOnce(face, m_size);

    glBlitFramebuffer(0, 0, (GLint)m_size, (GLint)m_size,
                      0, 0, (GLint)m_size, (GLint)m_size,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)previous_cube_texture);
    return true;
#endif
}

bool CubemapRenderTarget::renderFaces(const ImmersiveCameraRig& rig,
                                      float dt)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (!m_camera_node || !m_faces[0].get())
        return false;

    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        rig.applyFaceToCameraNode((ImmersiveCubemapFace)i, m_camera_node);
        m_faces[i]->renderToTexture(m_camera_node, dt);
        m_face_textures[i] = m_faces[i]->getColorTexture();
        m_face_depth_textures[i] = m_faces[i]->getDepthTexture();
        if (m_face_textures[i] == 0 || m_face_depth_textures[i] == 0)
            return false;
        if (!copyFaceToCubeMap(i, m_face_textures[i]))
            return false;
        if (!copyDepthFaceToCubeMap(i, m_face_depth_textures[i]))
            return false;
    }
    if (!m_logged_validation)
    {
        Log::info("CubemapRenderTarget",
            "Rendered six distinct cubemap-equivalent faces: +X tex=%u, -X tex=%u, +Y tex=%u, -Y tex=%u, +Z tex=%u, -Z tex=%u.",
            m_face_textures[0], m_face_textures[1],
            m_face_textures[2], m_face_textures[3],
            m_face_textures[4], m_face_textures[5]);
        m_logged_validation = true;
    }
    return true;
#endif
}

void CubemapRenderTarget::destroy()
{
#ifndef SERVER_ONLY
    for (unsigned int i = 0; i < CUBEMAP_FACE_COUNT; i++)
    {
        m_faces[i].reset();
        m_face_textures[i] = 0;
        m_face_depth_textures[i] = 0;
    }
    if (m_camera_node)
    {
        irr_driver->removeCameraSceneNode(m_camera_node);
        m_camera_node = NULL;
    }
    if (m_copy_frame_buffer != 0)
    {
        GLuint fbo = (GLuint)m_copy_frame_buffer;
        glDeleteFramebuffers(1, &fbo);
    }
    if (m_depth_copy_frame_buffer != 0)
    {
        GLuint fbo = (GLuint)m_depth_copy_frame_buffer;
        glDeleteFramebuffers(1, &fbo);
    }
    if (m_cube_texture != 0)
    {
        GLuint texture = (GLuint)m_cube_texture;
        glDeleteTextures(1, &texture);
    }
    if (m_depth_cube_texture != 0)
    {
        GLuint texture = (GLuint)m_depth_cube_texture;
        glDeleteTextures(1, &texture);
    }
#endif
    m_cube_texture = 0;
    m_depth_cube_texture = 0;
    m_copy_frame_buffer = 0;
    m_depth_copy_frame_buffer = 0;
    m_size = 0;
    m_logged_validation = false;
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
