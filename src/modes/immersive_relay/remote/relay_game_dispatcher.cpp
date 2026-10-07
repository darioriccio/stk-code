#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>

namespace
{
struct Request
{
    std::string method, path, body;
    bool api_active;
    int port;
    std::mutex mutex;
    std::condition_variable ready;
    bool done = false;
    bool cancelled = false;
    RelayHttpResponse response;
};
std::mutex queue_mutex;
std::deque<std::shared_ptr<Request> > queue;
bool stopped = false;
}

RelayHttpResponse RelayGameDispatcher::dispatch(
    const std::string& method, const std::string& path, const std::string& body,
    bool api_active, int port)
{
    // Snapshot reads remain responsive while a scene is loading.
    const std::string route_path = path.substr(0, path.find('?'));
    if (method != "POST" && route_path != "/immersive/v1/tracks" &&
        route_path != "/immersive/v1/karts" && route_path != "/immersive/v1/input/devices" &&
        route_path != "/immersive/v1/input/state")
        return RelayCommandRouter::route(method, path, body, api_active, port);

    std::shared_ptr<Request> request(new Request());
    request->method = method;
    request->path = path;
    request->body = body;
    request->api_active = api_active;
    request->port = port;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (stopped)
            return RelayHttpResponse(503, "{\"ok\":false,\"error\":\"game_stopping\"}");
        queue.push_back(request);
    }
    std::unique_lock<std::mutex> lock(request->mutex);
    if (!request->ready.wait_for(lock, std::chrono::seconds(30),
                                [&request] { return request->done; }))
    {
        // A queued job is cancelled. If loading already began, the caller
        // must refresh /state before retrying rather than assume no effect.
        request->cancelled = true;
        return RelayHttpResponse(503,
            "{\"ok\":false,\"error\":\"game_command_timeout_refresh_state\"}");
    }
    return request->response;
}

void RelayGameDispatcher::enqueueLocalReady()
{
    std::shared_ptr<Request> request(new Request());
    request->method = "POST";
    request->path = "/immersive/v1/operator/ready";
    request->body = "{}";
    request->api_active = true;
    request->port = 0;
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (stopped) return;
    // A held button must not reset another race or skip several phases.
    for (const std::shared_ptr<Request>& queued : queue)
        if (queued->path == request->path) return;
    queue.push_back(request);
}

void RelayGameDispatcher::drain()
{
    std::deque<std::shared_ptr<Request> > pending;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        pending.swap(queue);
    }
    for (const std::shared_ptr<Request>& request : pending)
    {
        {
            std::lock_guard<std::mutex> lock(request->mutex);
            if (request->cancelled)
                continue;
        }
        RelayHttpResponse response;
        try
        {
            response = RelayCommandRouter::route(request->method, request->path,
                request->body, request->api_active, request->port);
        }
        catch (const std::exception&)
        {
            response = RelayHttpResponse(500,
                "{\"ok\":false,\"error\":\"game_command_failed\"}");
        }
        {
            std::lock_guard<std::mutex> lock(request->mutex);
            request->response = response;
            request->done = true;
        }
        request->ready.notify_one();
    }
}

void RelayGameDispatcher::stop()
{
    std::deque<std::shared_ptr<Request> > pending;
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stopped = true;
        pending.swap(queue);
    }
    for (const std::shared_ptr<Request>& request : pending)
    {
        {
            std::lock_guard<std::mutex> lock(request->mutex);
            request->response = RelayHttpResponse(503,
                "{\"ok\":false,\"error\":\"game_stopping\"}");
            request->done = true;
        }
        request->ready.notify_one();
    }
}
#endif
