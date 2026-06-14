//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/projection/fisheye_domemaster_projection.hpp"

#ifndef SERVER_ONLY
#include "graphics/frame_buffer.hpp"
#include "graphics/gl_headers.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/shader_based_renderer.hpp"
#include "graphics/shared_gpu_objects.hpp"
#include "graphics/shader.hpp"
#include "graphics/skybox.hpp"
#include "graphics/sp/sp_base.hpp"
#include "utils/constants.hpp"
#include "utils/log.hpp"
#include "modes/immersive_relay/render/immersive_render_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#endif

namespace ImmersiveRelay
{

#ifndef SERVER_ONLY
class DomemasterShader
    : public Shader<DomemasterShader, irr::core::vector2df, float, int, int,
                    float, int>
{
public:
    DomemasterShader()
    {
        loadProgram(OBJECT,
                    GL_VERTEX_SHADER, "immersive_domemaster.vert",
                    GL_FRAGMENT_SHADER, "immersive_fisheye_domemaster.frag");
        assignTextureUnit(0, "environment_cube",
                          1, "scene_depth_cube",
                          2, "skybox_cube",
                          3, "face_pos_x",
                          4, "face_neg_x",
                          5, "face_pos_y",
                          6, "face_neg_y",
                          7, "face_pos_z",
                          8, "face_neg_z",
                          9, "depth_face_pos_x",
                          10, "depth_face_neg_x",
                          11, "depth_face_pos_y",
                          12, "depth_face_neg_y",
                          13, "depth_face_pos_z",
                          14, "depth_face_neg_z");
        assignUniforms("resolution", "dome_tilt_rad", "debug_grid",
                       "has_skybox", "face_tan_half_fov", "use_multiview");
    }
};

void dumpDomemasterFboOnce(unsigned int size)
{
    static bool dumped = false;
    if (dumped)
        return;

    const char* path = std::getenv("STK_IMMERSIVE_DOMEMASTER_DUMP");
    if (!path || path[0] == '\0')
        return;

    std::vector<unsigned char> pixels((size_t)size * (size_t)size * 4, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, (GLsizei)size, (GLsizei)size, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());

    FILE* file = std::fopen(path, "wb");
    if (!file)
    {
        Log::warn("FisheyeDomemasterProjection",
                  "Cannot write domemaster debug dump '%s'.", path);
        dumped = true;
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
    Log::info("FisheyeDomemasterProjection",
              "Wrote domemaster debug dump '%s' (%ux%u).",
              path, size, size);
    dumped = true;
}
#endif

FisheyeDomemasterProjection::FisheyeDomemasterProjection()
{
    m_size = 0;
    m_texture = 0;
    m_publish_texture = 0;
    m_publish_texture_target = 0;
    m_logged_validation = false;
}

FisheyeDomemasterProjection::~FisheyeDomemasterProjection()
{
    destroy();
}

bool FisheyeDomemasterProjection::ensure(unsigned int size)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (m_size == size && m_texture != 0 && m_frame_buffer.get())
        return true;

    destroy();
    m_size = size;

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)size, (GLsizei)size,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Syphon clients on macOS receive rectangle textures natively. Keep a
    // 2D fallback so relay/window builds stay portable when rectangle
    // textures are not exposed by the GL headers.
#ifdef GL_TEXTURE_RECTANGLE
    m_publish_texture_target = GL_TEXTURE_RECTANGLE;
#else
    m_publish_texture_target = GL_TEXTURE_2D;
#endif
    glGenTextures(1, &m_publish_texture);
    glBindTexture((GLenum)m_publish_texture_target, m_publish_texture);
    glTexImage2D((GLenum)m_publish_texture_target, 0, GL_RGBA8,
                 (GLsizei)size, (GLsizei)size,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri((GLenum)m_publish_texture_target,
                    GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri((GLenum)m_publish_texture_target,
                    GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri((GLenum)m_publish_texture_target,
                    GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri((GLenum)m_publish_texture_target,
                    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture((GLenum)m_publish_texture_target, 0);

    std::vector<GLuint> rtts;
    rtts.push_back(m_texture);
    m_frame_buffer.reset(new FrameBuffer(rtts, size, size));
    Log::info("FisheyeDomemasterProjection",
              "Created %ux%u domemaster projection target.", size, size);
    return true;
#endif
}

bool FisheyeDomemasterProjection::configure(
    const ImmersiveRenderConfig& config)
{
    return ensure(config.m_domemaster_size);
}

bool FisheyeDomemasterProjection::renderProjection(
    const FullSphereCapture& capture,
    const ImmersiveRenderConfig& config,
    ImmersiveRenderStats& stats)
{
    (void)stats;
#ifdef SERVER_ONLY
    return false;
#else
    unsigned int skybox_cube_texture = 0;
    ShaderBasedRenderer* renderer = SP::getRenderer();
    if (renderer && renderer->getSkybox())
        skybox_cube_texture = renderer->getSkybox()->getCubeMap();

    return project(capture.getCubeTexture(),
                   capture.getDepthCubeTexture(),
                   skybox_cube_texture,
                   capture.getFaceTextures(),
                   capture.getFaceDepthTextures(),
                   config);
#endif
}

ProjectionOutputDescription
FisheyeDomemasterProjection::outputDescription() const
{
    ProjectionOutputDescription description;
    description.m_kind = IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
    description.m_id = id();
    description.m_status =
        projectionStatusToString(IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER);
    description.m_width = m_size;
    description.m_height = m_size;
    description.m_aspect =
        projectionAspectToString(IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER);
    description.m_coverage =
        projectionCoverageToString(IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER);
    description.m_orientation =
        projectionOrientationToString(IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER);
    description.m_square = true;
    description.m_two_to_one = false;
    description.m_debug_grid_supported = true;
    description.m_production = true;
    return description;
}

bool FisheyeDomemasterProjection::project(
    unsigned int cube_texture,
    unsigned int depth_cube_texture,
    unsigned int skybox_cube_texture,
    const unsigned int* face_textures,
    const unsigned int* face_depth_textures,
    const ImmersiveRenderConfig& config)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (!ensure(config.m_domemaster_size))
        return false;
    if (cube_texture == 0)
        return false;

    const GLboolean was_depth_test = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean was_cull_face = glIsEnabled(GL_CULL_FACE);
    const GLboolean was_blend = glIsEnabled(GL_BLEND);
    const GLboolean was_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean was_depth_write = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &was_depth_write);

    m_frame_buffer->bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)cube_texture);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_WRAP_R
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
#endif
#ifdef GL_TEXTURE_CUBE_MAP_SEAMLESS
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
    if (depth_cube_texture != 0)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)depth_cube_texture);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_WRAP_R
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
                        GL_CLAMP_TO_EDGE);
#endif
#ifdef GL_TEXTURE_COMPARE_MODE
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE,
                        GL_NONE);
#endif
#ifdef GL_DEPTH_STENCIL_TEXTURE_MODE
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_DEPTH_STENCIL_TEXTURE_MODE,
                        GL_DEPTH_COMPONENT);
#endif
    }
    if (skybox_cube_texture != 0)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_CUBE_MAP, (GLuint)skybox_cube_texture);
    }
    bool use_multiview = true;
    for (unsigned int i = 0; i < 6; i++)
    {
        if (!face_textures || !face_depth_textures ||
            face_textures[i] == 0 || face_depth_textures[i] == 0)
        {
            use_multiview = false;
            break;
        }
    }
    if (use_multiview)
    {
        for (unsigned int i = 0; i < 6; i++)
        {
            glActiveTexture(GL_TEXTURE3 + i);
            glBindTexture(GL_TEXTURE_2D, (GLuint)face_textures[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);

            glActiveTexture(GL_TEXTURE0 + 9 + i);
            glBindTexture(GL_TEXTURE_2D, (GLuint)face_depth_textures[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
#ifdef GL_TEXTURE_COMPARE_MODE
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
#ifdef GL_DEPTH_STENCIL_TEXTURE_MODE
            glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE,
                            GL_DEPTH_COMPONENT);
#endif
        }
    }
    glActiveTexture(GL_TEXTURE0);
    if (!m_logged_validation)
    {
        Log::info("FisheyeDomemasterProjection",
            "Projection validation: forward-facing fisheye from six rendered face textures=%s; cube fallback=%u; output=%ux%u.",
            use_multiview ? "on" : "off",
            cube_texture,
            m_size, m_size);
        m_logged_validation = true;
    }
    const float half_fov = config.m_face_fov_deg * 0.5f * DEGREE_TO_RAD;
    const float face_tan_half_fov = tanf(half_fov);
    DomemasterShader::getInstance()->drawFullScreenEffect(
        irr::core::vector2df((float)m_size, (float)m_size),
        config.m_dome_tilt_deg * DEGREE_TO_RAD,
        config.m_debug_grid ? 1 : 0,
        skybox_cube_texture != 0 ? 1 : 0,
        face_tan_half_fov,
        use_multiview ? 1 : 0);
    dumpDomemasterFboOnce(m_size);
    if (use_multiview)
    {
        for (unsigned int i = 0; i < 6; i++)
        {
            glActiveTexture(GL_TEXTURE3 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0 + 9 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindTexture((GLenum)m_publish_texture_target, m_publish_texture);
    glCopyTexSubImage2D((GLenum)m_publish_texture_target, 0, 0, 0, 0, 0,
                        (GLsizei)m_size, (GLsizei)m_size);
    glBindTexture((GLenum)m_publish_texture_target, 0);

    glDepthMask(was_depth_write);
    if (was_depth_test)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (was_cull_face)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (was_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (was_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, irr_driver->getDefaultFramebuffer());
    glViewport(0, 0,
        irr_driver->getActualScreenSize().Width,
        irr_driver->getActualScreenSize().Height);
    return true;
#endif
}

bool FisheyeDomemasterProjection::blitToDefault() const
{
#ifdef SERVER_ONLY
    return false;
#else
    if (!m_frame_buffer.get())
        return false;
    const irr::core::dimension2du screen = irr_driver->getActualScreenSize();
    const unsigned int side = std::min(screen.Width, screen.Height);
    const unsigned int x0 = (screen.Width - side) / 2;
    const unsigned int y0 = (screen.Height - side) / 2;
    m_frame_buffer->blitToDefault(x0, y0, x0 + side, y0 + side);
    return true;
#endif
}

void FisheyeDomemasterProjection::destroy()
{
#ifndef SERVER_ONLY
    m_frame_buffer.reset();
    if (m_texture != 0)
    {
        GLuint texture = m_texture;
        glDeleteTextures(1, &texture);
    }
    if (m_publish_texture != 0)
    {
        GLuint texture = m_publish_texture;
        glDeleteTextures(1, &texture);
    }
#endif
    m_texture = 0;
    m_publish_texture = 0;
    m_publish_texture_target = 0;
    m_size = 0;
    m_logged_validation = false;
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
