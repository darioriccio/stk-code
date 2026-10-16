//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/local_control_server.hpp"
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"
#include "modes/immersive_relay/remote/relay_http_transport.hpp"
#include "utils/log.hpp"
#include <cstring>
#include <exception>
#ifndef WIN32
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
namespace
{
struct OwnedSocket {int fd;explicit OwnedSocket(int value):fd(value){}~OwnedSocket(){if(fd>=0)close(fd);}};
int sendFlags()
{
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}
}
#endif
LocalControlServer* LocalControlServer::m_instance=NULL;
LocalControlServer::LocalControlServer():m_running(false),m_bind_host("127.0.0.1"),m_port(8890),m_token("immersive-dev-token"),m_wake_write_fd(-1){}
LocalControlServer::~LocalControlServer(){stop();}
LocalControlServer* LocalControlServer::get(){if(!m_instance)m_instance=new LocalControlServer();return m_instance;}
void LocalControlServer::destroy(){delete m_instance;m_instance=NULL;}
bool LocalControlServer::start(const std::string& bind_host,int port,const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(m_running)return true;
    if(m_thread.joinable())m_thread.join();
#ifndef WIN32
    if(m_wake_write_fd>=0){close(m_wake_write_fd);m_wake_write_fd=-1;}
#endif
    m_bind_host=bind_host.empty()?"127.0.0.1":bind_host;m_port=port>0?port:8890;
    m_token=token.empty()?"immersive-dev-token":token;
#ifdef WIN32
    Log::warn("ImmersiveRelayAPI","Relay local API is not implemented on Windows in Phase 1.");return false;
#else
    int wake[2];
    if(socketpair(AF_UNIX,SOCK_STREAM,0,wake)<0)return false;
    if(!RelayHttpTransport::nonblocking(wake[0])||!RelayHttpTransport::nonblocking(wake[1]))
    {close(wake[0]);close(wake[1]);return false;}
#ifdef SO_NOSIGPIPE
    int suppress=1;
    if(setsockopt(wake[1],SOL_SOCKET,SO_NOSIGPIPE,&suppress,sizeof(suppress))<0)
    {close(wake[0]);close(wake[1]);return false;}
#endif
    m_wake_write_fd=wake[1];m_running=true;
    try{m_thread=std::thread(&LocalControlServer::runLoop,this,wake[0]);}
    catch(const std::exception&){close(wake[0]);close(wake[1]);m_wake_write_fd=-1;m_running=false;return false;}
    return true;
#endif
}
void LocalControlServer::stop()
{
    // Serialize start/stop and ownership of the wake writer. Only the worker
    // ever closes its listener or admitted clients; fd numbers cannot be reused
    // underneath an accept/poll/read on another thread.
    std::lock_guard<std::mutex> lock(m_mutex);m_running=false;
#ifndef WIN32
    if(m_wake_write_fd>=0){const char wake=1;send(m_wake_write_fd,&wake,1,sendFlags());}
#endif
    if(m_thread.joinable())m_thread.join();
#ifndef WIN32
    if(m_wake_write_fd>=0){close(m_wake_write_fd);m_wake_write_fd=-1;}
#endif
}
void LocalControlServer::runLoop(int wake_read_fd)
{
#ifndef WIN32
    OwnedSocket wake(wake_read_fd),server(socket(AF_INET,SOCK_STREAM,0));
    if(server.fd<0||!RelayHttpTransport::nonblocking(server.fd))
    {Log::error("ImmersiveRelayAPI","Unable to create relay API socket.");m_running=false;return;}
    int reuse=1;setsockopt(server.fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    sockaddr_in address;std::memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_port=htons(uint16_t(m_port));
    if(inet_pton(AF_INET,m_bind_host.c_str(),&address.sin_addr)!=1||bind(server.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(server.fd,32)<0)
    {Log::error("ImmersiveRelayAPI","Unable to bind/listen relay API to %s:%d.",m_bind_host.c_str(),m_port);m_running=false;return;}
    Log::info("ImmersiveRelayAPI","Relay API listening on http://%s:%d.",m_bind_host.c_str(),m_port);
    RelayHttpTransport::Pool clients;
    const RelayHttpTransport::Submit submit=[this](const RelayHttpTransport::Request& request)
    {
        const RelayGameDispatcher::Ticket ticket=RelayGameDispatcher::submit(request.method,request.path,request.body,m_running.load(),m_port);
        RelayHttpTransport::Job job;
        job.poll=[ticket](RelayHttpTransport::Response& output)
        {
            RelayHttpResponse response;if(!RelayGameDispatcher::poll(ticket,&response))return false;
            output=RelayHttpTransport::Response(response.m_status_code,response.m_body);return true;
        };
        job.cancel=[ticket]{RelayGameDispatcher::cancel(ticket);};return job;
    };
    while(m_running)
    {
        const RelayHttpTransport::PollResult ready=clients.step(server.fd,wake.fd,5,RelayHttpTransport::nowUs,submit);
        if(!m_running)break;
        if(ready.wakeReady){char bytes[32];recv(wake.fd,bytes,sizeof(bytes),0);}
        // Accept is bounded too: an arrival flood cannot starve existing peers.
        if(ready.listenerReady)for(unsigned accepted=0;accepted<8&&m_running;++accepted)
        {
            const int fd=accept(server.fd,NULL,NULL);
            if(fd<0){if(errno==EINTR)continue;break;}
            if(!clients.add(fd,m_token,RelayHttpTransport::nowUs()))close(fd);
        }
    }
#else
    (void)wake_read_fd;
#endif
}
#endif
