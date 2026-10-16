//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#if defined(ENABLE_IMMERSIVE_RELAY) && defined(ENABLE_SYPHON_OUTPUT)

#include "modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.hpp"

#include "graphics/gl_headers.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"
#include "modes/immersive_relay/spatial/scintilla_engine.hpp"
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <cerrno>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <Syphon/Syphon.h>

namespace ImmersiveRelay
{

SyphonOutputAdapterMac::SyphonOutputAdapterMac()
{
    m_server = NULL;
    m_initialized = false;
    m_unavailable_logged = false;
    m_width = 0;
    m_height = 0;
    m_lease_enabled = false;
    m_lease_directory = m_lease_gate = m_lease_data = -1;
    m_writer_gate = m_writer_data = false;
    m_publish_fence = NULL;
    m_activity = NULL;
}

SyphonOutputAdapterMac::~SyphonOutputAdapterMac()
{
    shutdown();
}

bool SyphonOutputAdapterMac::initialize(const std::string& server_name,
                                       unsigned int width,
                                       unsigned int height)
{
    if (m_initialized && m_server_name == server_name &&
        m_width == width && m_height == height &&
        m_lease_enabled == ScintillaEngine::enabled())
        return true;

    shutdown();
    m_server_name = server_name;
    m_width = width;
    m_height = height;

    CGLContextObj context = CGLGetCurrentContext();
    if (!context)
    {
        Log::error("SyphonOutputAdapterMac",
                   "Cannot initialize Syphon: no current OpenGL context.");
        return false;
    }

    NSString* name = [NSString stringWithUTF8String:server_name.c_str()];
    SyphonOpenGLServer* server =
        [[SyphonOpenGLServer alloc] initWithName:name
                                         context:context
                                         options:nil];
    if (!server)
    {
        Log::error("SyphonOutputAdapterMac",
                   "SyphonOpenGLServer init failed for '%s'.",
                   server_name.c_str());
        return false;
    }

    m_server = server;
    m_lease_enabled = ScintillaEngine::enabled();
    NSString* uuid = [[server serverDescription] objectForKey:SyphonServerDescriptionUUIDKey];
    if (uuid) m_server_uuid = [uuid UTF8String];
    if (m_lease_enabled && !prepareLease()) {
        shutdown(); return false;
    }
    m_initialized = true;
    // The producer window is normally hidden behind its consumer and may run
    // without sound: App Nap would then coalesce its timers and lower its
    // priority, stalling frames and the relay API that carries live input.
    m_activity = [[[NSProcessInfo processInfo]
        beginActivityWithOptions:NSActivityUserInitiated | NSActivityLatencyCritical
                          reason:@"Immersive relay live output"] retain];
    Log::info("SyphonOutputAdapterMac",
              "Publishing immersive projection via Syphon server '%s' at %ux%u.",
              server_name.c_str(), width, height);
    return true;
}

bool SyphonOutputAdapterMac::publishFrame(const ImmersiveOutputFrame& frame,
                                          float* publish_time_ms)
{
    return publishWithTarget(frame.m_texture,
                             frame.m_texture_target == 0
                                ? GL_TEXTURE_2D
                                : frame.m_texture_target,
                             frame.m_width,
                             frame.m_height,
                             publish_time_ms);
}

bool SyphonOutputAdapterMac::publish(unsigned int texture,
                                     unsigned int width,
                                     unsigned int height,
                                     float* publish_time_ms)
{
    return publishWithTarget(texture, GL_TEXTURE_2D, width, height,
                             publish_time_ms);
}

bool SyphonOutputAdapterMac::publishWithTarget(unsigned int texture,
                                               unsigned int texture_target,
                                               unsigned int width,
                                               unsigned int height,
                                               float* publish_time_ms)
{
    const uint64_t start = StkTime::getMonoTimeMs();
    if (!m_initialized || !m_server || texture == 0)
    {
        if (publish_time_ms)
            *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
        return false;
    }

    if (m_lease_enabled && !acquireWriter()) return false;
    SyphonOpenGLServer* server = (SyphonOpenGLServer*)m_server;
    [server publishFrameTexture:(GLuint)texture
                  textureTarget:(GLenum)texture_target
                    imageRegion:NSMakeRect(0, 0, width, height)
              textureDimensions:NSMakeSize(width, height)
                        flipped:NO];
    if (m_lease_enabled) {
        // Release only after the GPU has completed the publisher's IOSurface copy.
        // Next frame polls with timeout 0; neither flock nor GL waits on readers.
        m_publish_fence = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        if (!m_publish_fence) { shutdown(); return false; }
    }

    if (publish_time_ms)
        *publish_time_ms = (float)(StkTime::getMonoTimeMs() - start);
    return true;
}

namespace {
struct RetiredLease { int directory,gate,data; GLsync fence; CGLContextObj context; };
std::vector<RetiredLease> retiredLeases;
void pollRetiredLeases() {
    for(auto it=retiredLeases.begin();it!=retiredLeases.end();) {
        if(!it->fence || it->context!=CGLGetCurrentContext()){++it;continue;}
        const GLenum status=glClientWaitSync(it->fence,0,0);
        if(status!=GL_ALREADY_SIGNALED && status!=GL_CONDITION_SATISFIED){++it;continue;}
        glDeleteSync(it->fence);flock(it->data,LOCK_UN);flock(it->gate,LOCK_UN);
        close(it->data);close(it->gate);close(it->directory);it=retiredLeases.erase(it);
    }
}
bool checkedDirectory(int fd, bool privateDirectory) {
    struct stat info;
    return fd >= 0 && fstat(fd,&info)==0 && info.st_uid==getuid() && S_ISDIR(info.st_mode)
        && (info.st_mode&0022)==0 && (!privateDirectory || (info.st_mode&07777)==0700);
}
int leaseFile(int directory,const std::string& name) {
    int fd=openat(directory,name.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    struct stat info,named;
    if(fd<0)return -1;
    if(fstat(fd,&info)!=0 || !S_ISREG(info.st_mode) || info.st_uid!=getuid()
       || (info.st_mode&07777)!=0600 || info.st_nlink!=1
       || fstatat(directory,name.c_str(),&named,AT_SYMLINK_NOFOLLOW)!=0
       || named.st_dev!=info.st_dev || named.st_ino!=info.st_ino) {close(fd);return -1;}
    return fd;
}
}
bool SyphonOutputAdapterMac::prepareLease()
{
    pollRetiredLeases();
    if(retiredLeases.size()>=16 || m_server_uuid.empty() || m_server_uuid.size()>128)return false;
    for(char c:m_server_uuid)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return false;
    if(m_server_uuid=="." || m_server_uuid=="..")return false;
    struct passwd* user=getpwuid(getuid());if(!user)return false;
    int directory=open(user->pw_dir,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(!checkedDirectory(directory,false)){if(directory>=0)close(directory);return false;}
    const char* names[]={"Library","Application Support","TholusOS","FrameLeases"};
    for(unsigned i=0;i<4;i++) {
        if(mkdirat(directory,names[i],0700)!=0 && errno!=EEXIST){close(directory);return false;}
        int next=openat(directory,names[i],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(directory);directory=next;
        if(!checkedDirectory(directory,i==3)){if(directory>=0)close(directory);return false;}
    }
    m_lease_directory=directory;
    m_lease_gate=leaseFile(directory,m_server_uuid+".gate");
    m_lease_data=leaseFile(directory,m_server_uuid+".lock");
    return m_lease_gate>=0 && m_lease_data>=0;
}
void SyphonOutputAdapterMac::pollPublishedLease()
{
    pollRetiredLeases();
    if(m_publish_fence) {
        GLenum result=glClientWaitSync((GLsync)m_publish_fence,0,0);
        if(result!=GL_ALREADY_SIGNALED && result!=GL_CONDITION_SATISFIED)return;
        glDeleteSync((GLsync)m_publish_fence);m_publish_fence=NULL;
        flock(m_lease_data,LOCK_UN);m_writer_data=false;
        flock(m_lease_gate,LOCK_UN);m_writer_gate=false;
    }
}
bool SyphonOutputAdapterMac::acquireWriter()
{
    if(m_lease_data<0 || m_lease_gate<0)return false;
    pollPublishedLease();
    if(m_publish_fence)return false;
    if(!m_writer_gate) {
        if(flock(m_lease_gate,LOCK_EX|LOCK_NB)!=0)return false;
        m_writer_gate=true;
    }
    // Keep the gate while older readers finish, preventing continuous-reader starvation.
    if(flock(m_lease_data,LOCK_EX|LOCK_NB)!=0)return false;
    m_writer_data=true;return true;
}
void SyphonOutputAdapterMac::closeLease()
{
    if(m_writer_data && m_lease_data>=0) {
        // Reconfiguration must not unlock an IOSurface copy still in flight.
        // Retire its lease until a zero-timeout fence observation, even after server stop.
        // A missing/failed fence stays locked until process exit; no unverifiable read.
        RetiredLease old={m_lease_directory,m_lease_gate,m_lease_data,(GLsync)m_publish_fence,CGLGetCurrentContext()};
        retiredLeases.push_back(old);
        m_lease_directory=m_lease_gate=m_lease_data=-1;m_publish_fence=NULL;
        m_writer_data=m_writer_gate=false;
    }
    if(m_writer_data && m_lease_data>=0)flock(m_lease_data,LOCK_UN);
    if(m_writer_gate && m_lease_gate>=0)flock(m_lease_gate,LOCK_UN);
    if(m_lease_data>=0)close(m_lease_data);
    if(m_lease_gate>=0)close(m_lease_gate);
    if(m_lease_directory>=0)close(m_lease_directory);
    m_lease_data=m_lease_gate=m_lease_directory=-1;
    m_writer_data=m_writer_gate=false;m_lease_enabled=false;m_server_uuid.clear();
    // Never unlink lock files underneath a reader that still holds their inode.
}

void SyphonOutputAdapterMac::shutdown()
{
    closeLease();
    if (m_activity)
    {
        [[NSProcessInfo processInfo] endActivity:(id)m_activity];
        [(id)m_activity release];
        m_activity = NULL;
    }
    if (m_server)
    {
        SyphonOpenGLServer* server = (SyphonOpenGLServer*)m_server;
        [server stop];
        [server release];
        m_server = NULL;
    }
    m_initialized = false;
}

} // namespace ImmersiveRelay

#endif
