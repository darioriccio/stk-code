// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_RELAY_HTTP_TRANSPORT_HPP
#define HEADER_RELAY_HTTP_TRANSPORT_HPP
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#ifndef WIN32
namespace RelayHttpTransport
{
const size_t MaxHeaders=8192,MaxBody=65536,MaxResponseBody=1048576,MaxClients=16,TurnBytes=16384;
const int64_t HeaderUs=2000000,ReadUs=5000000,JobUs=30000000,WriteUs=2000000;
struct Request {std::string method,path,body;};
struct Response {int status=200;std::string body;Response(int code=200,const std::string& value=""):status(code),body(value){}};
struct Job {std::function<bool(Response&)> poll;std::function<void()> cancel;};
typedef std::function<Job(const Request&)> Submit;
typedef std::function<int64_t()> Clock;
struct PollResult {bool listenerReady=false,wakeReady=false;};
// The pool exclusively owns admitted sockets. No syscall waits for peer progress.
class Pool
{
public:
    Pool();~Pool();
    bool add(int fd,const std::string& token,int64_t acceptedUs);
    size_t size()const;
    PollResult step(int listener,int wake,int timeoutMs,const Clock& clock,const Submit& submit);
private:
    struct Impl;std::unique_ptr<Impl> m;
};
bool nonblocking(int fd);
int64_t nowUs();
}
#endif
#endif
