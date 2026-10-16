// SPDX-License-Identifier: GPL-3.0-or-later
// Production dispatcher with only the final game router replaced by a recorder.
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <vector>
namespace
{
std::vector<std::string> calls;
std::function<void(const std::string&)> duringRoute;
[[noreturn]] void failCheck(const char* expression,int line)
{std::cerr<<__FILE__<<':'<<line<<": CHECK failed: "<<expression<<'\n';std::exit(EXIT_FAILURE);}
#define CHECK(expression) ((expression)?static_cast<void>(0):failCheck(#expression,__LINE__))
}
RelayHttpResponse::RelayHttpResponse():m_status_code(200){}
RelayHttpResponse::RelayHttpResponse(int status,const std::string& body):m_status_code(status),m_body(body){}
RelayHttpResponse RelayCommandRouter::route(const std::string&,const std::string& path,const std::string&,bool,int)
{calls.push_back(path);if(duringRoute)duringRoute(path);return RelayHttpResponse(200,path);}
namespace
{
using namespace RelayGameDispatcher;
void asynchronousQueuePreservesFifoAndFastSnapshots()
{
    calls.clear();RelayHttpResponse response;
    const Ticket first=submit("POST","/first","{}",true,8890),second=submit("POST","/second","{}",true,8890);
    const Ticket clock=submit("GET","/immersive/v1/spatial/clock","",true,8890);
    CHECK(!poll(first,&response)&&!poll(second,&response)&&!poll(clock,&response));
    const Ticket snapshot=submit("GET","/immersive/v1/state","",true,8890);
    CHECK(poll(snapshot,&response)&&response.m_status_code==200&&calls.size()==1);
    drain();CHECK(calls.size()==4&&calls[1]=="/first"&&calls[2]=="/second"&&calls[3]=="/immersive/v1/spatial/clock");
    CHECK(poll(first,&response)&&response.m_body=="/first"&&poll(second,&response)&&response.m_body=="/second");
    CHECK(poll(clock,&response)&&response.m_status_code==200);
    CHECK(dispatch("GET","/immersive/v1/state","",true,8890).m_status_code==200);
}
void combatStateNeverReadsGameStateOnHttpThread()
{
    calls.clear();RelayHttpResponse response;
    const auto state=submit("GET","/immersive/v1/scintilla/state","",true,8890);
    CHECK(!poll(state,&response)&&calls.empty());drain();
    CHECK(poll(state,&response)&&calls.size()==1&&calls[0]=="/immersive/v1/scintilla/state");
}
void combatIntentAndReadyStayOrderedOnTheGameThread()
{
    calls.clear();RelayHttpResponse response;
    const auto intent=submit("POST","/immersive/v1/scintilla/intent","{}",true,8890);
    const auto ready=submit("POST","/immersive/v1/operator/ready","{}",true,8890);
    CHECK(!poll(intent,&response)&&!poll(ready,&response)&&calls.empty());drain();
    CHECK(calls.size()==2&&calls[0]=="/immersive/v1/scintilla/intent"&&calls[1]=="/immersive/v1/operator/ready");
    CHECK(poll(intent,&response)&&poll(ready,&response));
}
void cancellationBeforeDrainFreesCapacityAndCannotExecute()
{
    calls.clear();std::vector<Ticket> tickets;RelayHttpResponse response;
    for(unsigned i=0;i<64;++i)tickets.push_back(submit("POST","/queued_"+std::to_string(i),"{}",true,8890));
    const Ticket overflow=submit("POST","/overflow","{}",true,8890);
    CHECK(poll(overflow,&response)&&response.m_status_code==503&&response.m_body.find("game_command_backpressure")!=std::string::npos);
    cancel(tickets[0]);const Ticket healthy=submit("POST","/healthy","{}",true,8890);CHECK(!poll(healthy,&response));
    drain();CHECK(calls.size()==64&&calls.back()=="/healthy"&&std::find(calls.begin(),calls.end(),"/queued_0")==calls.end());
    CHECK(poll(healthy,&response)&&response.m_status_code==200);
}
void cancelAfterRoutingBeginsPreservesUncertainEffectSemantics()
{
    calls.clear();RelayHttpResponse response;Ticket request=submit("POST","/already_started","{}",true,8890);
    duringRoute=[&](const std::string& path){if(path=="/already_started")cancel(request);};
    drain();duringRoute=nullptr;
    CHECK(calls.size()==1&&poll(request,&response)&&response.m_status_code==200);
}
void stoppingCompletesWaitersAndRefusesFurtherQueuedWork()
{
    calls.clear();RelayHttpResponse response;const Ticket waiting=submit("POST","/waiting","{}",true,8890);
    const Ticket clock=submit("GET","/immersive/v1/spatial/clock","",true,8890);
    stop();CHECK(poll(waiting,&response)&&response.m_status_code==503&&response.m_body.find("game_stopping")!=std::string::npos);
    CHECK(poll(clock,&response)&&response.m_status_code==503);drain();CHECK(calls.empty());
    const Ticket refused=submit("POST","/after_stop","{}",true,8890);CHECK(poll(refused,&response)&&response.m_status_code==503);
}
struct Test {const char* name;void(*run)();};
const Test cases[]={
#define DISPATCHER_CASE(name) {#name,name},
#include "relay_game_dispatcher_cases.inc"
#undef DISPATCHER_CASE
};
}
int main(int argc,char** argv)
{
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto& test:cases)std::cout<<test.name<<'\n';return 0;}
    const bool selected=argc==3&&std::string(argv[1])=="--case";
    if(argc!=1&&!selected){std::cerr<<"Usage: relay_game_dispatcher_regression [--list | --case NAME]\n";return 2;}
    // stop is deliberately last in all-case mode; CTest also isolates each case.
    for(const auto& test:cases)if(!selected||std::string(argv[2])==test.name){std::cout<<"[ RUN ] "<<test.name<<std::endl;test.run();std::cout<<"[ OK ] "<<test.name<<std::endl;if(selected)return 0;}
    if(selected){std::cerr<<"Unknown regression case\n";return 2;}return 0;
}
