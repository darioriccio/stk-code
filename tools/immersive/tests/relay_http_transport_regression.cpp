// SPDX-License-Identifier: GPL-3.0-or-later
// Socketpair-only tests: no listening server, STK runtime or hardware.
#include "modes/immersive_relay/remote/relay_http_transport.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
using namespace RelayHttpTransport;
namespace
{
[[noreturn]] void failCheck(const char* expression,int line)
{std::cerr<<__FILE__<<':'<<line<<": CHECK failed: "<<expression<<'\n';std::exit(EXIT_FAILURE);}
#define CHECK(expression) ((expression)?static_cast<void>(0):failCheck(#expression,__LINE__))
struct Pair
{
    int server=-1,peer=-1;
    Pair(){int pair[2];CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);server=pair[0];peer=pair[1];CHECK(nonblocking(peer));
#ifdef SO_NOSIGPIPE
        int suppress=1;CHECK(setsockopt(peer,SOL_SOCKET,SO_NOSIGPIPE,&suppress,sizeof(suppress))==0);
#endif
    }
    ~Pair(){if(server>=0)close(server);if(peer>=0)close(peer);}
    void admit(Pool& pool,int64_t now){CHECK(pool.add(server,"secret",now));server=-1;}
    ssize_t write(const char* bytes,size_t count)
    {
#ifdef MSG_NOSIGNAL
        return send(peer,bytes,count,MSG_NOSIGNAL);
#else
        return send(peer,bytes,count,0);
#endif
    }
    void writeSmall(const std::string& value){CHECK(write(value.data(),value.size())==ssize_t(value.size()));}
    std::string read()
    {
        std::string value;char buffer[4096];
        for(;;){ssize_t count=recv(peer,buffer,sizeof(buffer),0);if(count>0)value.append(buffer,size_t(count));else{CHECK(count==0||errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR);break;}}
        return value;
    }
};
void pump(Pool& pool,int64_t now,const Submit& submit,unsigned turns=8)
{for(unsigned i=0;i<turns;++i)pool.step(-1,-1,0,[now]{return now;},submit);}
Submit immediate(std::vector<Request>& seen,const std::string& response="{}")
{
    return [&seen,response](const Request& request)
    {seen.push_back(request);Job job;job.poll=[response](Response& result){result=Response(200,response);return true;};return job;};
}
std::string request(const std::string& fields="",const std::string& body="",const std::string& method="POST")
{return method+(method=="GET"?" /immersive/v1/spatial/clock":" /immersive/v1/spatial/update")+" HTTP/1.1\r\nHost: localhost\r\nX-STK-Relay-Token: secret\r\n"+fields+"\r\n"+body;}
std::string exchange(const std::string& wire,std::vector<Request>& seen)
{
    Pool pool;Pair client;client.admit(pool,1000000);const Submit submit=immediate(seen);
    size_t offset=0;std::string response;
    for(unsigned turn=0;pool.size()&&turn<1024;++turn)
    {
        if(offset<wire.size())
        {
            const ssize_t count=client.write(wire.data()+offset,std::min<size_t>(1024,wire.size()-offset));
            if(count>0)offset+=size_t(count);
            else CHECK(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR||errno==EPIPE||errno==ECONNRESET);
        }
        pump(pool,1000000,submit,1);response+=client.read();
    }
    CHECK(pool.size()==0);return response+client.read();
}
void strictHeaderFramingAndEarlyAuthentication()
{
    for(const std::string& fields:{"Content-Length: -1\r\n","Content-Length: +2\r\n","Content-Length: 2junk\r\n","Content-Length: 2,2\r\n",
        "Content-Length: 2\r\ncontent-length: 2\r\n","Transfer-Encoding: chunked\r\n","Content-Length: 2\r\nTransfer-Encoding: identity\r\n",
        "X-STK-Relay-Token: secret\r\n","Content-Length : 2\r\n"," folded: value\r\n"})
    {std::vector<Request> seen;CHECK(exchange(request(fields),seen).find("HTTP/1.1 400 ")==0);CHECK(seen.empty());}
    std::vector<Request> seen;
    CHECK(exchange(request("Content-Length: 9999999999999999999999999999\r\n"),seen).find("HTTP/1.1 413 ")==0&&seen.empty());
    CHECK(exchange("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 65536\r\n\r\n",seen).find("HTTP/1.1 401 ")==0&&seen.empty());
    CHECK(exchange("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 27\r\n\r\nX-STK-Relay-Token: secret\r\n",seen).find("HTTP/1.1 401 ")==0&&seen.empty());
    CHECK(exchange(request("Authorization: Bearer wrong\r\n"),seen).find("HTTP/1.1 401 ")==0&&seen.empty());
    CHECK(exchange("GET / HTTP/1.1\r\nX-STK-Relay-Token: secret\r\n\r\n",seen).find("HTTP/1.1 400 ")==0&&seen.empty());
    CHECK(exchange("GET / HTTP/1.1\r\nHost: \r\nX-STK-Relay-Token: secret\r\n\r\n",seen).find("HTTP/1.1 400 ")==0&&seen.empty());
    CHECK(exchange("GET / HTTP/1.0\r\nX-STK-Relay-Token: secret\r\n\r\n",seen).find("HTTP/1.1 200 ")==0&&seen.size()==1);
    seen.clear();CHECK(exchange("OPTIONS / HTTP/1.1\r\nHost: localhost\r\n\r\n",seen).find("HTTP/1.1 204 ")==0&&seen.empty());
    CHECK(exchange("GET / HTTP/1.1\r\nHost: localhost\r\nAuthorization: bEaReR secret\r\n\r\n",seen).find("HTTP/1.1 200 ")==0&&seen.size()==1);
}
void fullBodyAndIndependentHeaderBodyLimits()
{
    std::vector<Request> seen;Pool pool;Pair client;client.admit(pool,1000000);
    const Submit submit=immediate(seen);client.writeSmall(request("Content-Length: 4\r\n","ab"));pump(pool,1000000,submit);
    CHECK(seen.empty()&&client.read().empty());client.writeSmall("cd");pump(pool,1000000,submit);
    CHECK(seen.size()==1&&seen[0].body=="abcd"&&client.read().find("HTTP/1.1 200 ")==0);
    Pool truncated;Pair shortBody;shortBody.admit(truncated,1000000);shortBody.writeSmall(request("Content-Length: 4\r\n","ab"));
    CHECK(shutdown(shortBody.peer,SHUT_WR)==0);seen.clear();pump(truncated,1000000,submit);
    CHECK(seen.empty()&&shortBody.read().find("HTTP/1.1 400 ")==0);
    seen.clear();Pool full;Pair maximum;maximum.admit(full,1000000);
    maximum.writeSmall(request("Content-Length: 65536\r\n"));pump(full,1000000,submit);
    const std::string body(MaxBody,'x');size_t offset=0;
    for(unsigned turn=0;offset<body.size()&&turn<256;++turn)
    {
        const ssize_t count=maximum.write(body.data()+offset,std::min<size_t>(4096,body.size()-offset));
        if(count>0)offset+=size_t(count);else CHECK(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR);
        pump(full,1000000,submit,1);
    }
    pump(full,1000000,submit);CHECK(offset==MaxBody&&seen.size()==1&&seen[0].body==body);
    CHECK(maximum.read().find("HTTP/1.1 200 ")==0);
    seen.clear();const std::string content="content-length: -100\r\nX-STK-Relay-Token: forged";
    CHECK(exchange(request("Content-Length: "+std::to_string(content.size())+"\r\n",content),seen).find("HTTP/1.1 200 ")==0);
    CHECK(seen.size()==1&&seen[0].body==content);
    seen.clear();CHECK(exchange(request("Content-Length: 65537\r\n"),seen).find("HTTP/1.1 413 ")==0&&seen.empty());
    Pool overHeaders;Pair oversized;oversized.admit(overHeaders,1000000);
    const std::string padded=request("X-Padding: "+std::string(MaxHeaders,'x')+"\r\n");size_t written=0;
    for(unsigned turn=0;overHeaders.size()&&turn<64;++turn)
    {
        if(written<padded.size())
        {
            const ssize_t count=oversized.write(padded.data()+written,std::min<size_t>(1024,padded.size()-written));
            if(count>0)written+=size_t(count);else CHECK(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR||errno==EPIPE||errno==ECONNRESET);
        }
        pump(overHeaders,1000000,submit,1);
    }
    CHECK(oversized.read().find("HTTP/1.1 431 ")==0&&seen.empty());
}
void partialClientsCannotBlockHealthyClocksOrUpdates()
{
    Pool pool;Pair header,body,clock,update;for(Pair* client:{&header,&body,&clock,&update})client->admit(pool,1000000);
    header.writeSmall("POST /immersive/v1/spatial/update HTTP/1.1\r\nHost:");
    body.writeSmall(request("Content-Length: 65536\r\n","{"));
    clock.writeSmall(request("","","GET"));update.writeSmall(request("Content-Length: 2\r\n","{}"));
    std::vector<Request> seen;pump(pool,1000000,immediate(seen));
    CHECK(seen.size()==2&&seen[0].method=="GET"&&seen[0].path=="/immersive/v1/spatial/clock");
    CHECK(seen[1].method=="POST"&&seen[1].path=="/immersive/v1/spatial/update");
    CHECK(clock.read().find("HTTP/1.1 200 ")==0&&update.read().find("HTTP/1.1 200 ")==0);
    CHECK(header.read().empty()&&body.read().empty()&&pool.size()==2);
}
void absoluteDeadlinesDoNotRenewWithDrip()
{
    Pool pool;Pair header;header.admit(pool,1000000);std::vector<Request> seen;const Submit submit=immediate(seen);
    header.writeSmall("GET /");pump(pool,1000000,submit);
    header.writeSmall("a");pump(pool,2999999,submit);CHECK(header.read().empty());
    header.writeSmall("b");pump(pool,3000000,submit);CHECK(header.read().find("HTTP/1.1 408 ")==0&&seen.empty());
    Pool reading;Pair body;body.admit(reading,1000000);body.writeSmall(request("Content-Length: 8\r\n","a"));pump(reading,1000000,submit);
    body.writeSmall("b");pump(reading,5999999,submit);CHECK(body.read().empty());
    body.writeSmall("c");pump(reading,6000000,submit);CHECK(body.read().find("HTTP/1.1 408 ")==0&&seen.empty());
}
void partialWritesAreFairBoundedAndExpireWithoutReader()
{
    Pool pool;Pair slow,healthy;slow.admit(pool,1000000);healthy.admit(pool,1000000);
    const std::string body(MaxResponseBody,'z');std::vector<Request> seen;
    const Submit submit=[&](const Request& input){seen.push_back(input);Job job;job.poll=[&,input](Response& response){response=Response(200,input.method=="POST"?body:"{}");return true;};return job;};
    slow.writeSmall(request());healthy.writeSmall(request("","","GET"));pump(pool,1000000,submit,32);
    CHECK(healthy.read().find("HTTP/1.1 200 ")==0&&pool.size()==1);
    std::string wire;
    for(unsigned turn=0;pool.size()&&turn<1024;++turn){wire+=slow.read();pump(pool,1000000,submit,1);}
    wire+=slow.read();const size_t headers=wire.find("\r\n\r\n");CHECK(headers!=std::string::npos&&wire.substr(headers+4)==body&&pool.size()==0);
    seen.clear();CHECK(exchange(request(),seen).find("HTTP/1.1 200 ")==0);
    Pool oversized;Pair huge;huge.admit(oversized,1000000);huge.writeSmall(request());pump(oversized,1000000,immediate(seen,std::string(MaxResponseBody+1,'x')));
    const std::string denied=huge.read();CHECK(denied.find("HTTP/1.1 503 ")==0&&denied.find("response_too_large")!=std::string::npos);
    Pool expires;Pair unread;unread.admit(expires,1000000);unread.writeSmall(request());pump(expires,1000000,immediate(seen,body),32);
    CHECK(expires.size()==1);pump(expires,3000000,immediate(seen,body));CHECK(expires.size()==0);
}
void asynchronousJobsStayFairAndShutdownCancels()
{
    std::unique_ptr<Pool> pool(new Pool);Pair waiting,healthy;waiting.admit(*pool,1000000);healthy.admit(*pool,1000000);
    unsigned cancelled=0,submitted=0;bool done=false;
    const Submit submit=[&](const Request& input)
    {
        ++submitted;Job job;
        if(input.method=="POST") {job.poll=[](Response&){return false;};job.cancel=[&]{++cancelled;};}
        else job.poll=[&](Response& response){done=true;response=Response(200,"{}");return true;};
        return job;
    };
    waiting.writeSmall(request());CHECK(shutdown(waiting.peer,SHUT_WR)==0);
    healthy.writeSmall(request("","","GET"));pump(*pool,1000000,submit);
    CHECK(submitted==2&&done&&healthy.read().find("HTTP/1.1 200 ")==0&&cancelled==0);
    pump(*pool,30999999,submit);CHECK(cancelled==0);
    pump(*pool,31000000,submit);CHECK(cancelled==1&&waiting.read().find("HTTP/1.1 503 ")==0);
    Pair abandoned;abandoned.admit(*pool,32000000);abandoned.writeSmall(request());pump(*pool,32000000,submit);
    CHECK(cancelled==1);pool.reset();CHECK(cancelled==2);
}
void capacityWakeAndHalfClosePreserveOwnership()
{
    Pool pool;std::vector<std::unique_ptr<Pair>> clients;
    for(size_t i=0;i<MaxClients;++i){clients.emplace_back(new Pair);clients.back()->admit(pool,1000000);}
    Pair excess;CHECK(!pool.add(excess.server,"secret",1000000));CHECK(pool.size()==MaxClients);
    // Refused descriptor remains caller-owned and usable; the pool did not close it.
    excess.writeSmall("x");char byte=0;CHECK(recv(excess.server,&byte,1,0)==1&&byte=='x');
    Pair wake;wake.writeSmall("w");std::vector<Request> seen;
    const PollResult ready=pool.step(-1,wake.server,0,[]{return int64_t(1000000);},immediate(seen));CHECK(ready.wakeReady);
    Pool half;Pair client;client.admit(half,1000000);client.writeSmall(request());CHECK(shutdown(client.peer,SHUT_WR)==0);
    pump(half,1000000,immediate(seen));CHECK(client.read().find("HTTP/1.1 200 ")==0);
    CHECK(pool.size()==MaxClients&&half.size()==0);
}
struct Test {const char* name;void(*run)();};
const Test cases[]={
#define HTTP_CASE(name) {#name,name},
#include "relay_http_transport_cases.inc"
#undef HTTP_CASE
};
}
int main(int argc,char** argv)
{
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto& test:cases)std::cout<<test.name<<'\n';return 0;}
    const bool selected=argc==3&&std::string(argv[1])=="--case";
    if(argc!=1&&!selected){std::cerr<<"Usage: relay_http_transport_regression [--list | --case NAME]\n";return 2;}
    for(const auto& test:cases)if(!selected||std::string(argv[2])==test.name){std::cout<<"[ RUN ] "<<test.name<<std::endl;test.run();std::cout<<"[ OK ] "<<test.name<<std::endl;if(selected)return 0;}
    if(selected){std::cerr<<"Unknown regression case\n";return 2;}return 0;
}
