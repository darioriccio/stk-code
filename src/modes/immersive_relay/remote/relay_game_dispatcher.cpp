#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>

namespace RelayGameDispatcher
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
}
namespace
{
using RelayGameDispatcher::Request;
std::mutex queue_mutex;
std::deque<std::shared_ptr<Request> > queue;
bool stopped = false;
}

RelayGameDispatcher::Ticket RelayGameDispatcher::submit(
    const std::string& method,const std::string& path,const std::string& body,
    bool api_active,int port)
{
    Ticket request(new Request());request->method=method;request->path=path;
    request->body=body;request->api_active=api_active;request->port=port;
    // Preserve snapshot reads that remain responsive while a scene is loading.
    const std::string route_path=path.substr(0,path.find('?'));
    if(method!="POST"&&route_path.compare(0,std::string("/immersive/v1/spatial/").size(),"/immersive/v1/spatial/")!=0&&route_path.compare(0,std::string("/immersive/v1/scintilla/").size(),"/immersive/v1/scintilla/")!=0&&route_path!="/immersive/v1/tracks"&&
       route_path!="/immersive/v1/karts"&&route_path!="/immersive/v1/input/devices"&&route_path!="/immersive/v1/input/state"&&route_path!="/immersive/v1/capabilities")
    {request->response=RelayCommandRouter::route(method,path,body,api_active,port);request->done=true;return request;}
    std::lock_guard<std::mutex> lock(queue_mutex);
    if(stopped||queue.size()>=64)
    {
        request->response=RelayHttpResponse(503,stopped?"{\"ok\":false,\"error\":\"game_stopping\"}":"{\"ok\":false,\"error\":\"game_command_backpressure\"}");
        request->done=true;
    }
    else queue.push_back(request);
    return request;
}
bool RelayGameDispatcher::poll(const Ticket& ticket,RelayHttpResponse* response)
{
    std::lock_guard<std::mutex> lock(ticket->mutex);
    if(!ticket->done)return false;
    *response=ticket->response;return true;
}
void RelayGameDispatcher::cancel(const Ticket& ticket)
{
    // Routing that has already started retains its existing uncertain-effect
    // semantics; only queued work can be prevented from executing.
    {std::lock_guard<std::mutex> lock(ticket->mutex);if(!ticket->done)ticket->cancelled=true;}
    // Do not let disconnected/expired clients fill the finite queue with jobs
    // that can no longer execute. Never nest queue and ticket mutexes.
    std::lock_guard<std::mutex> lock(queue_mutex);
    for(auto it=queue.begin();it!=queue.end();)if(*it==ticket)it=queue.erase(it);else ++it;
}
RelayHttpResponse RelayGameDispatcher::dispatch(
    const std::string& method,const std::string& path,const std::string& body,
    bool api_active,int port)
{
    Ticket request=submit(method,path,body,api_active,port);
    std::unique_lock<std::mutex> lock(request->mutex);
    if(!request->ready.wait_for(lock,std::chrono::seconds(30),[&request]{return request->done;}))
    {
        request->cancelled=true;
        return RelayHttpResponse(503,"{\"ok\":false,\"error\":\"game_command_timeout_refresh_state\"}");
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
