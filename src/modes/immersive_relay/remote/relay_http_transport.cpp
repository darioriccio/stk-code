// SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/remote/relay_http_transport.hpp"
#if defined(ENABLE_RELAY_LOCAL_API) && !defined(WIN32)
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <exception>
#include <map>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
namespace RelayHttpTransport
{
int64_t nowUs(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool nonblocking(int fd)
{
    const int flags=fcntl(fd,F_GETFL,0);if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)return false;
    const int descriptor=fcntl(fd,F_GETFD,0);return descriptor>=0&&fcntl(fd,F_SETFD,descriptor|FD_CLOEXEC)==0;
}
namespace
{
bool tokenChar(unsigned char c)
{return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||std::string("!#$%&'*+-.^_`|~").find(c)!=std::string::npos;}
std::string lower(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c+=('a'-'A');return value;}
std::string trim(const std::string& value)
{size_t first=value.find_first_not_of(" \t"),last=value.find_last_not_of(" \t");return first==std::string::npos?"":value.substr(first,last-first+1);}
const char* statusText(int code)
{
    switch(code){case 200:return "OK";case 204:return "No Content";case 400:return "Bad Request";case 401:return "Unauthorized";case 403:return "Forbidden";
    case 404:return "Not Found";case 405:return "Method Not Allowed";case 408:return "Request Timeout";case 409:return "Conflict";case 413:return "Payload Too Large";
    case 417:return "Expectation Failed";case 429:return "Too Many Requests";case 431:return "Request Header Fields Too Large";case 501:return "Not Implemented";
    case 503:return "Service Unavailable";default:return "Internal Server Error";}
}
struct Client
{
    enum Stage {Headers,Body,Dispatch,Writing,Closed} stage=Headers;
    int fd;std::string token,input,output;size_t length=0,sent=0;
    int64_t accepted,deadline;Request request;Job job;
    Client(int descriptor,const std::string& secret,int64_t time):fd(descriptor),token(secret),accepted(time),deadline(time+HeaderUs){}
    ~Client(){if(job.cancel)job.cancel();close(fd);}
    void respond(int code,const std::string& body,int64_t now)
    {
        if(job.cancel)job.cancel();job=Job();
        const bool tooLarge=body.size()>MaxResponseBody;
        const std::string& value=tooLarge?std::string("{\"ok\":false,\"error\":\"response_too_large\"}"):body;
        if(tooLarge)code=503;
        if(code<100||code>599)code=500;
        std::ostringstream out;out<<"HTTP/1.1 "<<code<<' '<<statusText(code)<<"\r\nContent-Type: application/json\r\n"
            <<"Access-Control-Allow-Origin: http://localhost\r\nAccess-Control-Allow-Headers: Authorization, X-STK-Relay-Token, Content-Type\r\n"
            <<"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\nContent-Length: "<<value.size()<<"\r\nConnection: close\r\n\r\n"<<value;
        output=out.str();input.clear();request.body.clear();sent=0;stage=Writing;
        deadline=std::min(now+WriteUs,accepted+ReadUs+JobUs+WriteUs);
    }
    void fail(int code,const char* reason,int64_t now)
    {respond(code,std::string("{\"ok\":false,\"error\":\"")+reason+"\"}",now);}
    bool headers(size_t end,int64_t now)
    {
        if(end+4>MaxHeaders){fail(431,"headers_too_large",now);return false;}
        const size_t line=input.find("\r\n"),first=input.find(' '),second=input.find(' ',first==std::string::npos?0:first+1);
        if(line==std::string::npos||first==std::string::npos||second==std::string::npos||second>=line||input.find(' ',second+1)<line)
        {fail(400,"bad_request_line",now);return false;}
        request.method=input.substr(0,first);request.path=input.substr(first+1,second-first-1);
        const std::string version=input.substr(second+1,line-second-1);
        if(request.method.empty()||request.path.empty()||request.path[0]!='/'||request.path.size()>4096||(version!="HTTP/1.1"&&version!="HTTP/1.0"))
        {fail(400,"bad_request_line",now);return false;}
        for(unsigned char c:request.method)if(!tokenChar(c)){fail(400,"bad_method",now);return false;}
        for(unsigned char c:request.path)if(c<=32||c==127||c=='#'){fail(400,"bad_request_target",now);return false;}
        std::map<std::string,std::string> fields;
        for(size_t start=line+2;start<end;)
        {
            const size_t stop=input.find("\r\n",start),colon=input.find(':',start);
            if(stop==std::string::npos||colon==std::string::npos||colon>=stop){fail(400,"bad_header",now);return false;}
            const std::string name=lower(input.substr(start,colon-start));
            if(name.empty()){fail(400,"bad_header",now);return false;}
            for(unsigned char c:name)if(!tokenChar(c)){fail(400,"bad_header",now);return false;}
            const std::string value=trim(input.substr(colon+1,stop-colon-1));
            for(unsigned char c:value)if((c<32&&c!='\t')||c==127){fail(400,"bad_header",now);return false;}
            if(!fields.emplace(name,value).second){fail(400,"duplicate_header",now);return false;}
            start=stop+2;
        }
        if(version=="HTTP/1.1"&&(!fields.count("host")||fields["host"].empty())){fail(400,"invalid_host",now);return false;}
        if(fields.count("transfer-encoding")||fields.count("trailer")){fail(400,"unsupported_transfer_encoding",now);return false;}
        if(fields.count("content-length"))
        {
            const std::string& value=fields["content-length"];
            if(value.empty()){fail(400,"invalid_content_length",now);return false;}
            for(unsigned char c:value)
            {
                if(c<'0'||c>'9'){fail(400,"invalid_content_length",now);return false;}
                const size_t digit=c-'0';if(length>(MaxBody-digit)/10){fail(413,"payload_too_large",now);return false;}
                length=length*10+digit;
            }
        }
        // Authentication is header-only and occurs as soon as headers are framed,
        // before further body accumulation or any dispatch.
        // The unauthenticated OPTIONS exception is a bodyless CORS preflight.
        if(request.method!="OPTIONS")
        {
            bool credential=false,valid=true;
            if(fields.count("x-stk-relay-token")){credential=true;valid=valid&&fields["x-stk-relay-token"]==token;}
            if(fields.count("authorization"))
            {
                credential=true;const std::string& value=fields["authorization"];
                valid=valid&&value.size()>7&&lower(value.substr(0,7))=="bearer "&&value.substr(7)==token;
            }
            if(!credential||!valid){fail(401,"invalid_token",now);return false;}
        }
        if(fields.count("expect")){fail(417,"unsupported_expectation",now);return false;}
        if(request.method=="OPTIONS"&&length){fail(400,"preflight_body_not_supported",now);return false;}
        input.erase(0,end+4);stage=Body;deadline=accepted+ReadUs;return true;
    }
    void advance(int64_t now,const Submit& submit)
    {
        if(stage==Headers)
        {
            const size_t end=input.find("\r\n\r\n");
            if(end==std::string::npos){if(input.size()>=MaxHeaders)fail(431,"headers_too_large",now);return;}
            if(!headers(end,now))return;
        }
        if(stage!=Body)return;
        if(input.size()>length){fail(400,"unexpected_request_bytes",now);return;}
        if(input.size()!=length)return;
        request.body.swap(input);
        if(request.method=="OPTIONS"){respond(204,"",now);return;}
        try{job=submit(request);stage=Dispatch;deadline=now+JobUs;request.body.clear();}
        catch(const std::exception&){fail(503,"game_command_failed",now);}
    }
    short events()const{return stage==Headers||stage==Body?POLLIN:stage==Writing?POLLOUT:0;}
    void service(short revents,int64_t now,const Submit& submit)
    {
        if(stage==Closed)return;
        if(now>=deadline)
        {
            if(stage==Writing){stage=Closed;return;}
            fail(stage==Dispatch?503:408,stage==Dispatch?"game_command_timeout_refresh_state":"request_timeout",now);
        }
        if(revents&(POLLERR|POLLNVAL)){stage=Closed;return;}
        if((stage==Headers||stage==Body)&&(revents&(POLLIN|POLLHUP)))
        {
            size_t budget=TurnBytes;char bytes[4096];
            while(budget&&(stage==Headers||stage==Body))
            {
                const size_t remaining=stage==Headers?MaxHeaders-input.size():length-input.size();
                if(!remaining){advance(now,submit);break;}
                const ssize_t count=recv(fd,bytes,std::min(sizeof(bytes),std::min(budget,remaining)),0);
                if(count>0){input.append(bytes,size_t(count));budget-=size_t(count);advance(now,submit);}
                else if(count==0){fail(400,"incomplete_request",now);break;}
                else {if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)stage=Closed;break;}
            }
        }
        if(stage==Dispatch)
        {
            Response response;
            if(!job.poll){fail(503,"game_command_unavailable",now);}
            else try{if(job.poll(response))respond(response.status,response.body,now);}
            catch(const std::exception&){fail(503,"game_command_failed",now);}
        }
        if(stage==Writing&&(revents&(POLLOUT|POLLHUP)))
        {
#ifdef MSG_NOSIGNAL
            const int flags=MSG_NOSIGNAL;
#else
            const int flags=0;
#endif
            const ssize_t count=send(fd,output.data()+sent,std::min(TurnBytes,output.size()-sent),flags);
            if(count>0){sent+=size_t(count);if(sent==output.size())stage=Closed;}
            else if(count==0||(revents&POLLHUP)||(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR))stage=Closed;
        }
    }
};
}
struct Pool::Impl {std::vector<std::unique_ptr<Client>> clients;};
Pool::Pool():m(new Impl){}
Pool::~Pool(){}
size_t Pool::size()const{return m->clients.size();}
bool Pool::add(int fd,const std::string& token,int64_t accepted)
{
    if(m->clients.size()>=MaxClients||!nonblocking(fd))return false;
#ifdef SO_NOSIGPIPE
    int suppress=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&suppress,sizeof(suppress))<0)return false;
#endif
    m->clients.emplace_back(new Client(fd,token,accepted));return true;
}
PollResult Pool::step(int listener,int wake,int timeoutMs,const Clock& clock,const Submit& submit)
{
    std::vector<pollfd> descriptors;descriptors.push_back({listener,POLLIN,0});descriptors.push_back({wake,POLLIN,0});
    for(const auto& client:m->clients)descriptors.push_back({client->stage==Client::Dispatch?-1:client->fd,client->events(),0});
    const bool pending=std::any_of(m->clients.begin(),m->clients.end(),[](const std::unique_ptr<Client>& client){return client->stage==Client::Dispatch;});
    const int result=::poll(descriptors.data(),descriptors.size(),std::max(0,std::min(timeoutMs,pending?1:5)));
    PollResult ready;
    if(result>=0){ready.listenerReady=descriptors[0].revents&POLLIN;ready.wakeReady=descriptors[1].revents&POLLIN;}
    for(size_t i=0;i<m->clients.size();++i)m->clients[i]->service(result>=0?descriptors[i+2].revents:0,clock(),submit);
    m->clients.erase(std::remove_if(m->clients.begin(),m->clients.end(),[](const std::unique_ptr<Client>& client){return client->stage==Client::Closed;}),m->clients.end());
    return ready;
}
}
#endif
