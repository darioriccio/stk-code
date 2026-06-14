//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifndef HEADER_RELAY_COMMAND_ROUTER_HPP
#define HEADER_RELAY_COMMAND_ROUTER_HPP

#ifdef ENABLE_RELAY_LOCAL_API

#include <string>

struct RelayHttpResponse
{
    int m_status_code;
    std::string m_body;

    RelayHttpResponse();
    RelayHttpResponse(int status_code, const std::string& body);
};

class RelayCommandRouter
{
public:
    static RelayHttpResponse route(const std::string& method,
                                   const std::string& path,
                                   const std::string& body,
                                   bool api_active,
                                   int api_port);
};

#endif // ENABLE_RELAY_LOCAL_API
#endif // HEADER_RELAY_COMMAND_ROUTER_HPP
