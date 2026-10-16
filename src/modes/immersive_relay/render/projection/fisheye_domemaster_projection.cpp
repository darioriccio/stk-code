//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/render/projection/fisheye_domemaster_projection.hpp"
#include "modes/immersive_relay/spatial/scintilla_engine.hpp"

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
    void scintillaUniforms() const { ScintillaEngine::bindProjectionUniforms(m_program); }
    unsigned measurementProgram() const { return m_program; }
    bool valid() const { GLint linked=0; glGetProgramiv(m_program, GL_LINK_STATUS, &linked); return linked == GL_TRUE; }
};

class EquirectangularShader
    : public Shader<EquirectangularShader, irr::core::vector2df, int, int, float, int>
{
public:
    EquirectangularShader()
    {
        loadProgram(OBJECT, GL_VERTEX_SHADER, "immersive_domemaster.vert",
                    GL_FRAGMENT_SHADER, "immersive_equirectangular.frag");
        assignTextureUnit(0, "environment_cube", 1, "scene_depth_cube", 2, "skybox_cube",
            3, "face_pos_x", 4, "face_neg_x", 5, "face_pos_y", 6, "face_neg_y",
            7, "face_pos_z", 8, "face_neg_z", 9, "depth_face_pos_x",
            10, "depth_face_neg_x", 11, "depth_face_pos_y", 12, "depth_face_neg_y",
            13, "depth_face_pos_z", 14, "depth_face_neg_z");
        assignUniforms("resolution", "debug_grid", "has_skybox",
                       "face_tan_half_fov", "use_multiview");
    }
    void scintillaUniforms() const { ScintillaEngine::bindProjectionUniforms(m_program); }
    unsigned measurementProgram() const { return m_program; }
    bool valid() const { GLint linked=0; glGetProgramiv(m_program, GL_LINK_STATUS, &linked); return linked == GL_TRUE; }
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
    m_width = 0;
    m_height = 0;
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
    return ensureTarget(size, size);
}

bool FisheyeDomemasterProjection::ensureTarget(unsigned int width, unsigned int height)
{
#ifdef SERVER_ONLY
    return false;
#else
    if (m_width == width && m_height == height && m_texture && m_frame_buffer)
        return true;
    GLint max_size = 0, previous_read_fbo = 0, previous_draw_fbo = 0, previous_active = 0, previous_2d = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_2d);
    if (!width || !height || width > (unsigned)max_size || height > (unsigned)max_size)
    {
        glActiveTexture((GLenum)previous_active);
        return false;
    }
#ifdef GL_TEXTURE_RECTANGLE
    const GLenum target = GL_TEXTURE_RECTANGLE;
    GLint previous_rectangle = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &previous_rectangle);
#else
    const GLenum target = GL_TEXTURE_2D;
#endif
    GLuint texture = 0, publish = 0, probe_fbo = 0;
    glGenTextures(1, &texture);
    glGenTextures(1, &publish);
    const GLuint textures[2] = { texture, publish };
    bool valid = texture != 0 && publish != 0;
    for (unsigned int i=0; i<2 && valid; i++)
    {
        const GLenum current = i == 0 ? GL_TEXTURE_2D : target;
        glBindTexture(current, textures[i]);
        glTexImage2D(current, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(current, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(current, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(current, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(current, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLint allocated_width = 0, allocated_height = 0;
        glGetTexLevelParameteriv(current, 0, GL_TEXTURE_WIDTH, &allocated_width);
        glGetTexLevelParameteriv(current, 0, GL_TEXTURE_HEIGHT, &allocated_height);
        valid = allocated_width == (GLint)width && allocated_height == (GLint)height;
    }
    if (valid)
    {
        // Probe before FrameBuffer's asserting constructor; failure preserves
        // the previous live target and restores the caller's GL bindings.
        glGenFramebuffers(1, &probe_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        valid = probe_fbo != 0 && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    if (probe_fbo) glDeleteFramebuffers(1, &probe_fbo);
    if (valid)
    {
        std::vector<GLuint> targets(1, texture);
        std::unique_ptr<FrameBuffer> next(new FrameBuffer(targets, width, height));
        if (m_texture && (GLuint)previous_2d == m_texture) previous_2d = (GLint)texture;
        if (m_publish_texture && target == GL_TEXTURE_2D && (GLuint)previous_2d == m_publish_texture) previous_2d = (GLint)publish;
#ifdef GL_TEXTURE_RECTANGLE
        if (m_publish_texture && (GLuint)previous_rectangle == m_publish_texture) previous_rectangle = (GLint)publish;
#endif
        destroy();
        m_texture=texture; m_publish_texture=publish; m_publish_texture_target=target;
        m_width=width; m_height=height; m_size=width;
        m_frame_buffer.swap(next);
    }
    else
    {
        if (texture) glDeleteTextures(1, &texture);
        if (publish) glDeleteTextures(1, &publish);
        Log::warn("ImmersiveProjection", "Cannot allocate %ux%u projection; previous target retained.", width, height);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw_fbo);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_2d);
#ifdef GL_TEXTURE_RECTANGLE
    glBindTexture(GL_TEXTURE_RECTANGLE, (GLuint)previous_rectangle);
#endif
    glActiveTexture((GLenum)previous_active);
    return valid;
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
    if (!configure(config))
        return false;
    if (cube_texture == 0)
        return false;
    if (kind() == IMMERSIVE_PROJECTION_EQUIRECTANGULAR ?
        !EquirectangularShader::getInstance()->valid() : !DomemasterShader::getInstance()->valid())
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
            "Projection validation: native six rendered face textures=%s; cube fallback=%u; output=%ux%u.",
            use_multiview ? "on" : "off",
            cube_texture,
            m_width, m_height);
        m_logged_validation = true;
    }
    const float half_fov = config.m_face_fov_deg * 0.5f * DEGREE_TO_RAD;
    const float face_tan_half_fov = tanf(half_fov);
    if (kind() == IMMERSIVE_PROJECTION_EQUIRECTANGULAR)
    {
        EquirectangularShader::getInstance()->scintillaUniforms();
        EquirectangularShader::getInstance()->drawFullScreenEffect(
            irr::core::vector2df((float)m_width, (float)m_height),
            config.m_debug_grid ? 1 : 0, skybox_cube_texture != 0 ? 1 : 0,
            face_tan_half_fov, use_multiview ? 1 : 0);
        ScintillaEngine::measureMissionProjection(EquirectangularShader::getInstance()->measurementProgram(),[&]{
            EquirectangularShader::getInstance()->drawFullScreenEffect(irr::core::vector2df((float)m_width,(float)m_height),config.m_debug_grid?1:0,skybox_cube_texture?1:0,face_tan_half_fov,use_multiview?1:0);
        });
    }
    else
    {
        DomemasterShader::getInstance()->scintillaUniforms();
        DomemasterShader::getInstance()->drawFullScreenEffect(
            irr::core::vector2df((float)m_width, (float)m_height),
            config.m_dome_tilt_deg * DEGREE_TO_RAD, config.m_debug_grid ? 1 : 0,
            skybox_cube_texture != 0 ? 1 : 0, face_tan_half_fov, use_multiview ? 1 : 0);
        ScintillaEngine::measureMissionProjection(DomemasterShader::getInstance()->measurementProgram(),[&]{
            DomemasterShader::getInstance()->drawFullScreenEffect(irr::core::vector2df((float)m_width,(float)m_height),config.m_dome_tilt_deg*DEGREE_TO_RAD,config.m_debug_grid?1:0,skybox_cube_texture?1:0,face_tan_half_fov,use_multiview?1:0);
        });
        dumpDomemasterFboOnce(m_size);
    }
    ScintillaEngine::finishProjectionUniforms();
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
                        (GLsizei)m_width, (GLsizei)m_height);
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
    m_width = 0;
    m_height = 0;
    m_logged_validation = false;
}

} // namespace ImmersiveRelay

#endif // ENABLE_IMMERSIVE_RELAY
