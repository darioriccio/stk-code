#ifndef HEADER_RELAY_GAME_DISPATCHER_HPP
#define HEADER_RELAY_GAME_DISPATCHER_HPP

#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_command_router.hpp"

// The socket thread never loads or resets a world. The main loop drains jobs
// between frames, when no World::update call is on the stack.
namespace RelayGameDispatcher
{
RelayHttpResponse dispatch(const std::string& method, const std::string& path,
                           const std::string& body, bool api_active, int port);
void enqueueLocalReady();
void drain();
void stop();
}
#endif
#endif
