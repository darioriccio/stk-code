//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_RELAY_LOCAL_API

#include "modes/immersive_relay/remote/local_control_server.hpp"

#include "modes/immersive_relay/remote/relay_command_router.hpp"
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

LocalControlServer* LocalControlServer::m_instance = NULL;

namespace
{
std::string lowerCopy(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

bool parseRequestLine(const std::string& request, std::string* method,
                      std::string* path)
{
    std::string::size_type line_end = request.find("\r\n");
    if (line_end == std::string::npos)
        return false;
    std::istringstream in(request.substr(0, line_end));
    std::string version;
    in >> *method >> *path >> version;
    if (method->empty() || path->empty())
        return false;
    return true;
}

int parseContentLength(const std::string& request)
{
    std::string lower = lowerCopy(request);
    std::string needle = "content-length:";
    std::string::size_type pos = lower.find(needle);
    if (pos == std::string::npos)
        return 0;
    pos += needle.size();
    while (pos < lower.size() && (lower[pos] == ' ' || lower[pos] == '\t'))
        pos++;
    return std::atoi(lower.c_str() + pos);
}

std::string extractBody(const std::string& request)
{
    std::string::size_type body_start = request.find("\r\n\r\n");
    if (body_start == std::string::npos)
        return "";
    return request.substr(body_start + 4);
}

const char* statusText(int status_code)
{
    switch (status_code)
    {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 429: return "Too Many Requests";
    case 413: return "Payload Too Large";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: return "Internal Server Error";
    }
}
}

LocalControlServer::LocalControlServer()
{
    m_running = false;
    m_bind_host = "127.0.0.1";
    m_port = 8890;
    m_token = "immersive-dev-token";
    m_listen_fd = -1;
}

LocalControlServer::~LocalControlServer()
{
    stop();
}

LocalControlServer* LocalControlServer::get()
{
    if (!m_instance)
        m_instance = new LocalControlServer();
    return m_instance;
}

void LocalControlServer::destroy()
{
    delete m_instance;
    m_instance = NULL;
}

bool LocalControlServer::start(const std::string& bind_host, int port,
                               const std::string& token)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running)
        return true;

    m_bind_host = bind_host.empty() ? "127.0.0.1" : bind_host;
    m_port = port > 0 ? port : 8890;
    m_token = token;
    if (m_token.empty())
        m_token = "immersive-dev-token";

#ifdef WIN32
    Log::warn("ImmersiveRelayAPI",
              "Relay local API is not implemented on Windows in Phase 1.");
    return false;
#else
    m_running = true;
    m_thread = std::thread(&LocalControlServer::runLoop, this);
    return true;
#endif
}

void LocalControlServer::stop()
{
    m_running = false;
#ifndef WIN32
    if (m_listen_fd >= 0)
    {
        shutdown(m_listen_fd, SHUT_RDWR);
        close(m_listen_fd);
        m_listen_fd = -1;
    }
#endif
    if (m_thread.joinable())
        m_thread.join();
}

void LocalControlServer::runLoop()
{
#ifndef WIN32
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        Log::error("ImmersiveRelayAPI", "Unable to create relay API socket.");
        m_running = false;
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)m_port);
    addr.sin_addr.s_addr = inet_addr(m_bind_host.c_str());

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        Log::error("ImmersiveRelayAPI",
                   "Unable to bind relay API to %s:%d.",
                   m_bind_host.c_str(), m_port);
        close(server_fd);
        m_running = false;
        return;
    }

    if (listen(server_fd, 8) < 0)
    {
        Log::error("ImmersiveRelayAPI", "Unable to listen on relay API.");
        close(server_fd);
        m_running = false;
        return;
    }

    m_listen_fd = server_fd;
    Log::info("ImmersiveRelayAPI", "Relay API listening on http://%s:%d.",
              m_bind_host.c_str(), m_port);

    while (m_running)
    {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (m_running)
                Log::warn("ImmersiveRelayAPI", "Relay API accept failed.");
            continue;
        }
        // A timed-out status poll must not terminate the producer via SIGPIPE.
#ifdef SO_NOSIGPIPE
        int suppress_sigpipe = 1;
        setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE,
                   &suppress_sigpipe, sizeof(suppress_sigpipe));
#endif
        handleClient(client_fd);
        close(client_fd);
    }
#endif
}

bool LocalControlServer::requestHasValidToken(
    const std::string& request) const
{
    const size_t headers_end = request.find("\r\n\r\n");
    size_t start = request.find("\r\n");
    if (headers_end == std::string::npos || start == std::string::npos)
        return false;
    start += 2;
    while (start < headers_end)
    {
        const size_t end = request.find("\r\n", start);
        const size_t colon = request.find(':', start);
        if (end == std::string::npos || colon == std::string::npos || colon >= end)
            return false;
        const std::string name = lowerCopy(request.substr(start, colon - start));
        size_t value_start = colon + 1, value_end = end;
        while (value_start < value_end && (request[value_start] == ' ' || request[value_start] == '\t'))
            value_start++;
        while (value_end > value_start && (request[value_end - 1] == ' ' || request[value_end - 1] == '\t'))
            value_end--;
        const std::string value = request.substr(value_start, value_end - value_start);
        if (name == "x-stk-relay-token" && value == m_token) return true;
        if (name == "authorization" && value.size() >= 7 &&
            lowerCopy(value.substr(0, 7)) == "bearer " && value.substr(7) == m_token)
            return true;
        start = end + 2;
    }
    return false;
}

void LocalControlServer::handleClient(int client_fd)
{
#ifndef WIN32
    std::string request;
    char buffer[4096];
    const size_t max_request_size = 65536;

    while (request.find("\r\n\r\n") == std::string::npos &&
           request.size() < max_request_size)
    {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0)
            return;
        request.append(buffer, (size_t)n);
    }

    const int content_length = parseContentLength(request);
    if (content_length > (int)max_request_size)
    {
        sendResponse(client_fd, 413,
            "{\"ok\":false,\"error\":\"payload_too_large\"}");
        return;
    }

    std::string::size_type body_start = request.find("\r\n\r\n");
    while (body_start != std::string::npos &&
           request.size() < body_start + 4 + (size_t)content_length)
    {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0)
            break;
        request.append(buffer, (size_t)n);
    }

    std::string method;
    std::string path;
    if (!parseRequestLine(request, &method, &path))
    {
        sendResponse(client_fd, 400,
            "{\"ok\":false,\"error\":\"bad_request\"}");
        return;
    }

    if (method == "OPTIONS")
    {
        sendResponse(client_fd, 204, "");
        return;
    }

    if (!requestHasValidToken(request))
    {
        sendResponse(client_fd, 401,
            "{\"ok\":false,\"error\":\"invalid_token\"}");
        return;
    }

    RelayHttpResponse response = RelayGameDispatcher::dispatch(
        method, path, extractBody(request), m_running, m_port);
    sendResponse(client_fd, response.m_status_code, response.m_body);
#endif
}

void LocalControlServer::sendResponse(int client_fd, int status_code,
                                      const std::string& body) const
{
#ifndef WIN32
    std::ostringstream out;
    out << "HTTP/1.1 " << status_code << " " << statusText(status_code) << "\r\n";
    out << "Content-Type: application/json\r\n";
    out << "Access-Control-Allow-Origin: http://localhost\r\n";
    out << "Access-Control-Allow-Headers: Authorization, X-STK-Relay-Token, Content-Type\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n\r\n";
    out << body;
    const std::string response = out.str();
    size_t sent = 0;
    while (sent < response.size())
    {
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        const ssize_t count = send(client_fd, response.data() + sent,
                                   response.size() - sent, flags);
        if (count <= 0)
            break;
        sent += (size_t)count;
    }
#endif
}

#endif // ENABLE_RELAY_LOCAL_API
