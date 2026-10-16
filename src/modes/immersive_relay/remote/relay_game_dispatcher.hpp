#ifndef HEADER_RELAY_GAME_DISPATCHER_HPP
#define HEADER_RELAY_GAME_DISPATCHER_HPP

#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_command_router.hpp"
#include <cstdint>
#include <memory>

// The socket thread never loads or resets a world. The main loop drains jobs
// between frames, when no World::update call is on the stack.
namespace RelayGameDispatcher
{
struct Request;
typedef std::shared_ptr<Request> Ticket;
// Nonblocking transport adapter; routing still occurs on the existing game queue.
Ticket submit(const std::string& method, const std::string& path,
              const std::string& body, bool api_active, int port);
bool poll(const Ticket& ticket, RelayHttpResponse* response);
void cancel(const Ticket& ticket);
RelayHttpResponse dispatch(const std::string& method, const std::string& path,
                           const std::string& body, bool api_active, int port);
void enqueueLocalReady();
void drain();
void stop();
// Spatial clock probes measure the transport, not frame pacing. While a clock
// is registered (from the game thread) and returns true, the socket thread
// answers GET /spatial/clock at once; otherwise the probe is queued and the
// router reports why the clock is unavailable. nullptr unregisters it.
typedef bool (*SpatialClock)(int64_t* now_us);
void setSpatialClock(SpatialClock clock);
}
#endif
#endif
