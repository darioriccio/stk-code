//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_LOCAL_CONTROL_SERVER_HPP
#define HEADER_LOCAL_CONTROL_SERVER_HPP

#ifdef ENABLE_RELAY_LOCAL_API

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class LocalControlServer
{
private:
    static LocalControlServer* m_instance;

    std::atomic<bool> m_running;
    std::thread m_thread;
    std::mutex m_mutex;
    std::string m_bind_host;
    int m_port;
    std::string m_token;
    int m_listen_fd;

    LocalControlServer();
    void runLoop();
    void handleClient(int client_fd);
    bool requestHasValidToken(const std::string& request) const;
    void sendResponse(int client_fd, int status_code,
                      const std::string& body) const;

public:
    ~LocalControlServer();

    static LocalControlServer* get();
    static void destroy();

    bool start(const std::string& bind_host, int port,
               const std::string& token);
    void stop();
    bool isRunning() const { return m_running; }
    int getPort() const { return m_port; }
};

#endif // ENABLE_RELAY_LOCAL_API
#endif // HEADER_LOCAL_CONTROL_SERVER_HPP
