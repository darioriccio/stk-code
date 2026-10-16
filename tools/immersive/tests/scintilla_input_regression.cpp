// SPDX-License-Identifier: GPL-3.0-or-later
// Core regression source: standalone CMake/CTest target or opt-in Relay tooling.
// Uses no STK graphics, network, hardware, or acquisition API.
#include "modes/immersive_relay/spatial/scintilla_input.hpp"
#include "modes/immersive_relay/spatial/scintilla_target_lifetime.hpp"
#include "modes/immersive_relay/spatial/scintilla_readback_cache.hpp"
#include "modes/immersive_relay/spatial/scintilla_depth_visibility.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <set>
#include <string>
#include <utility>
using namespace ScintillaSpatial;
namespace
{
[[noreturn]] void failCheck(const char* expression,const char* file,int line)
{
 std::cerr<<file<<':'<<line<<": CHECK failed: "<<expression<<'\n';
 std::exit(EXIT_FAILURE);
}
// Every check evaluates in Release/STKRelease too, including setup side effects.
#define CHECK(expression) ((expression)?static_cast<void>(0):failCheck(#expression,__FILE__,__LINE__))
const std::string identity="\"sourceId\":\"s1\",\"deviceBindingRevision\":\"b1\",\"trackingSpaceEpoch\":\"e1\",\"poseContinuityId\":\"c1\",\"roomCalibrationRevision\":\"r1\",\"gripCalibrationRevision\":\"g1\"";
const std::string context="\"contract\":\"tholus.spatial-input/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\"";
const std::string pose="{\"originM\":[0,0,0],\"direction\":[0,0,-1],\"basis\":\"room.rhs-y-up-front-minus-z/1\"}";
std::string prepare()
{
 return "{\"contract\":\"tholus.stk-spatial/1\",\"roster\":{"+context+",\"type\":\"roster\",\"installationID\":\"venue\",\"consumerID\":\"external.immersive_supertuxkart\",\"capabilities\":[\"roomRay6dof\",\"primaryAction\",\"eventTimePose\",\"secondaryAction\"],\"sources\":[{"+identity+"}],\"projectionContextId\":\"projection\",\"expiresAtHostUs\":5000000},\"issuedAtHostUs\":990000,\"inputUncertaintyUs\":0,\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}],\"clockSamples\":[{\"hostSentUs\":990000,\"serverReceivedUs\":990000,\"serverSentUs\":990000,\"hostReceivedUs\":990000},{\"hostSentUs\":991000,\"serverReceivedUs\":991000,\"serverSentUs\":991000,\"hostReceivedUs\":991000},{\"hostSentUs\":992000,\"serverReceivedUs\":992000,\"serverSentUs\":992000,\"hostReceivedUs\":992000}],\"routeRevision\":\"route\",\"projection\":{\"kind\":\"equirectangular\",\"width\":2048,\"height\":1024,\"domeTiltDeg\":0}}";
}
std::string message(const std::string& fields,const std::string& point="")
{return "{\"contract\":\"tholus.stk-spatial/1\",\"message\":{"+context+","+identity+","+fields+"}"+(point.empty()?"":",\"point\":"+point)+"}";}
std::string edge(const std::string& event,const std::string& kind,unsigned seq)
{
 const std::string q=std::to_string(seq);
 std::string point="{\"u\":0.5,\"v\":0.5,\"viewId\":\"main\",\"sourceId\":\"s1\",\"sequence\":"+q+",\"projectionContextId\":\"projection\"}";
 return message("\"type\":\"action\",\"sequence\":"+q+",\"eventId\":\""+event+"\",\"control\":\"primary\",\"edge\":\""+kind+"\",\"time\":{\"evaluatedAtHostUs\":999990,\"uncertaintyUs\":0},\"projectionContextId\":\"projection\""+(kind=="pressed"?",\"rayAtEvent\":"+pose:""),kind=="pressed"?point:"");
}
void ready(Runtime& r)
{
 CHECK(r.route("/spatial/prepare",prepare(),1000000).status==200);r.tick(1000000,true);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\""),1000000).status==200);
}
Frame frame(uint64_t id,double x=0)
{
 Frame f;f.stream="0123456789abcdef0123456789abcdef";f.id=id;f.generation=1;f.captureUs=995000;
 f.projection.kind="equirectangular";f.projection.width=2048;f.projection.height=1024;
 f.cameraRight=Vec3(1,0,0);f.cameraUp=Vec3(0,1,0);f.cameraForward=Vec3(0,0,1);
 Target t;t.id=7;t.instance=70;t.center=Vec3(x,0,10);t.radius=1;f.targets.push_back(t);return f;
}
std::string presentation(bool ambiguous=false)
{
 return "{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"frameStream\":\"0123456789abcdef0123456789abcdef\",\"timingRevision\":\"timing\",\"proofFromUs\":999000,\"proofUntilUs\":1000000,\"runs\":[{\"frameId\":1,\"presentedFromUs\":999000,\"presentedUntilUs\":"+std::string(ambiguous?"999990":"1000000")+"}"+std::string(ambiguous?",{\"frameId\":2,\"presentedFromUs\":999990,\"presentedUntilUs\":1000000}":"")+"]}";
}
void replace(std::string& s,const std::string& from,const std::string& to)
{size_t p=s.find(from);CHECK(p!=std::string::npos);s.replace(p,from.size(),to);}
// Independent acquisition time and control make delayed transport and held-state
// transitions explicit; the legacy fixtures retain their original timestamp.
std::string edgeAt(const std::string& event,const std::string& kind,unsigned sequence,
                   int64_t time,int64_t uncertainty=0,const std::string& control="primary")
{
 std::string value=edge(event,kind,sequence);
 replace(value,"\"evaluatedAtHostUs\":999990","\"evaluatedAtHostUs\":"+std::to_string(time));
 replace(value,"\"uncertaintyUs\":0","\"uncertaintyUs\":"+std::to_string(uncertainty));
 replace(value,"\"control\":\"primary\"","\"control\":\""+control+"\"");
 return value;
}
void strictParserAndGrant()
{
 Runtime r;std::string s=prepare();replace(s,"\"issuedAtHostUs\":990000","\"issuedAtHostUs\":990000,\"issuedAtHost\\u0055s\":990000");CHECK(r.route("/spatial/prepare",s,1000000).status==400);
 s=prepare();replace(s,"\"issuedAtHostUs\":990000","\"issuedAtHostUs\":9007199254740991.00000001");CHECK(r.route("/spatial/prepare",s,1000000).status==400);
 s=prepare();replace(s,",\"secondaryAction\"","");CHECK(r.route("/spatial/prepare",s,1000000).status==409);
 CHECK(r.route("/spatial/prepare",std::string(65537,'x'),1000000).status==400);
}
void releasesReplayPauseAndContinuity()
{
 Runtime r;ready(r);CHECK(r.route("/spatial/input",edge("no_release","pressed",1),1000000).status==409);
 CHECK(r.route("/spatial/input",edge("release","released",2),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("reordered","pressed",1),1000000).status==400);
 CHECK(r.route("/spatial/input",edge("release","released",3),1000000).status==409);
 r.tick(1000000,false);r.tick(1000000,true);CHECK(r.route("/spatial/input",edge("after_pause","pressed",4),1000000).status==409);
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),1000000).status==200);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":2,\"state\":\"tracked\",\"reason\":\"reacquired\""),1000000).status==400);
}
void asyncHistoricalHit()
{
 Runtime r;ready(r);r.addFrame(frame(1));bool depthReady=false;
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return depthReady?Visibility::Visible:Visibility::Pending;});r.setLiveTargetOracle([](uint64_t i,uint64_t n){return i==7&&n==70;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);r.route("/spatial/input",edge("release","released",0),1000000);CHECK(r.route("/spatial/input",edge("shot","pressed",1),1000000).status==200);
 CHECK(r.events().empty());depthReady=true;r.tick(1000000,true);CHECK(r.events().size()==1&&r.events().back().result=="hit"&&r.events().back().targetId==7);
}
void ambiguousAndOccluded()
{
 for(unsigned mode=0;mode<2;++mode)
 {
  Runtime r;ready(r);r.addFrame(frame(1));if(mode==0)r.addFrame(frame(2,5));
  r.setVisibilityOracle([mode](const Frame&,const Target&,const Vec3&,double){return mode==0?Visibility::Visible:Visibility::Occluded;});r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(mode==0),1000000).status==200);r.route("/spatial/input",edge("release","released",0),1000000);r.route("/spatial/input",edge("shot","pressed",1),1000000);
  CHECK(r.events().size()==1&&r.events().back().result==(mode==0?"ambiguous_presentation":"miss"));
 }
}
void orderedBatchAndMalformedInvalidation()
{
 Runtime r;ready(r);
 std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":["+edge("release","released",0)+","+edge("shot","pressed",1)+"]}";
 Reply reply=r.route("/spatial/update",batch,1000000);CHECK(reply.status==200&&reply.json.find("\"results\":[")!=std::string::npos);
 // A stale/replayed valid edge is a result, while malformed data closes the grant.
 batch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":["+edge("shot","pressed",1)+"]}";
 CHECK(r.route("/spatial/update",batch,1000000).status==200);
 CHECK(r.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":[{}]}",1000000).status==400);
 CHECK(r.stateJSON().find("\"prepared\":false")!=std::string::npos);
}
void deniedTimeCannotReplayAfterClockRenewal()
{
 Runtime r;ready(r);CHECK(r.route("/spatial/input",edge("release","released",0),1000000).status==200);
 std::string future=edge("future","pressed",1);replace(future,"\"evaluatedAtHostUs\":999990","\"evaluatedAtHostUs\":1000010");
 CHECK(r.route("/spatial/input",future,1000000).status==409);CHECK(!r.reticles()[0].primaryHeld);
 const std::string renewal="{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expiresAtHostUs\":6000000,\"clockSamples\":[{\"hostSentUs\":1000010,\"serverReceivedUs\":1000010,\"serverSentUs\":1000010,\"hostReceivedUs\":1000010},{\"hostSentUs\":1000020,\"serverReceivedUs\":1000020,\"serverSentUs\":1000020,\"hostReceivedUs\":1000020},{\"hostSentUs\":1000030,\"serverReceivedUs\":1000030,\"serverSentUs\":1000030,\"hostReceivedUs\":1000030}]}";
 CHECK(r.route("/spatial/renew",renewal,1000100).status==200);
 Reply retry=r.route("/spatial/input",future,1000100);CHECK(retry.status==409&&retry.json.find("duplicate_event")!=std::string::npos);
 Runtime roster;roster.setParticipants({"real_person"});CHECK(roster.route("/spatial/prepare",prepare(),1000000).status==409);
}
void pauseClearsBothHoldsAndRequiresAReleasePerControl()
{
 Runtime r;ready(r);
 CHECK(r.route("/spatial/input",edgeAt("primary_release","released",0,999990),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool_release","released",1,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("primary_hold","pressed",2,999990),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool_hold","pressed",3,999990,0,"secondary"),1000000).status==200);
 CHECK(r.reticles()[0].primaryHeld&&r.reticles()[0].toolHeld);
 r.tick(1010000,false);
 CHECK(!r.reticles()[0].primaryHeld&&!r.reticles()[0].toolHeld);
 CHECK(r.stateJSON().find("\"pending\":0")!=std::string::npos);
 r.tick(1020000,true);
 Reply noRelease=r.route("/spatial/input",edgeAt("tool_still_down","pressed",4,1021000,0,"secondary"),1022000);
 CHECK(noRelease.status==409&&noRelease.json.find("release_required")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("new_tool_release","released",5,1023000,0,"secondary"),1024000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new_tool_press","pressed",6,1023100,0,"secondary"),1024000).status==200);
 CHECK(!r.reticles()[0].primaryHeld&&r.reticles()[0].toolHeld);
}
void delayedPausedEdgesCannotRearmAfterResumeOrReplay()
{
 Runtime r;ready(r);r.tick(1010000,false);
 // These were acquired while paused but arrive less than 60 ms after resumption.
 r.tick(1020000,true);
 const std::string release=edgeAt("paused_release","released",1,1018000);
 const std::string press=edgeAt("paused_press","pressed",2,1019000);
 Reply denied=r.route("/spatial/input",release,1021000);
 CHECK(denied.status==409&&denied.json.find("event_precedes_gameplay_activation")!=std::string::npos);
 CHECK(r.route("/spatial/input",press,1021000).status==409);
 CHECK(!r.reticles()[0].primaryHeld);
 // A newer event cannot reuse the sequence consumed by a rejected press.
 CHECK(r.route("/spatial/input",edgeAt("new_but_old_sequence","released",2,1022000),1023000).status==400);
 // Rewriting the rejected event's time does not bypass its consumed event UUID.
 Reply replay=r.route("/spatial/input",edgeAt("paused_press","pressed",3,1022000),1023000);
 CHECK(replay.status==409&&replay.json.find("duplicate_event")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("fresh_release","released",3,1023000),1024000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("fresh_press","pressed",4,1023100),1024000).status==200);
 CHECK(r.reticles()[0].primaryHeld);
}
void pausedDeliveryAndBoundaryUncertaintyCannotArmTheNextActivation()
{
 Runtime r;ready(r);r.tick(1010000,false);
 CHECK(r.route("/spatial/input",edgeAt("delivered_paused_release","released",0,1011000),1012000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("delivered_paused_press","pressed",1,1013000),1014000).status==409);
 r.tick(1020000,true);
 CHECK(r.route("/spatial/input",edgeAt("press_without_new_release","pressed",2,1021000),1022000).status==409);
 // Its center is after resume, but the full uncertainty interval crosses the barrier.
 Reply boundary=r.route("/spatial/input",edgeAt("uncertain_release","released",3,1020010,20),1022000);
 CHECK(boundary.status==409&&boundary.json.find("event_precedes_gameplay_activation")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("after_uncertain_release","pressed",4,1021000),1022000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("certain_release","released",5,1023000,20),1024000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("certain_press","pressed",6,1023100,20),1024000).status==200);
}
void lifecycleRevisionRemembersPauseAndResumeBetweenInputTicks()
{
 Runtime r;ready(r);
 CHECK(r.route("/spatial/input",edgeAt("old_release","released",0,999990),1000000).status==200);
 // No inactive runtime tick occurs: the controller's atomic snapshot preserves
 // a complete pause/resume pair that happened between render/input callbacks.
 r.tick(1021000,true,1,1020000);
 CHECK(r.route("/spatial/input",edgeAt("missed_pause_release","released",1,1018000),1021000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("missed_pause_press","pressed",2,1019000),1021000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("after_revision_release","released",3,1023000),1024000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("after_revision_press","pressed",4,1023100),1024000).status==200);
 CHECK(r.reticles()[0].primaryHeld);
}
void firstPreparedHeldWorldRequiresFreshInputAfterInitialResume()
{
 Runtime r;r.tick(995000,false);
 CHECK(r.route("/spatial/prepare",prepare(),1000000).status==200);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\""),1000000).status==200);
 r.tick(1010000,true,1,1010000);
 CHECK(r.route("/spatial/input",edgeAt("before_first_start_release","released",0,1009000),1012000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("before_first_start_press","pressed",1,1009500),1012000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("after_first_start_release","released",2,1011000),1012000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("after_first_start_press","pressed",3,1011100),1012000).status==200);
}
void renewedClockCannotMakeAnUnchangedHostProofRegress()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 // Exact zero-RTT probes move the admissible server-minus-host offset from
 // [-2,+2] to [-4,0] us. The original host proof and candidate set are unchanged.
 const std::string renewal=R"({"contract":"tholus.stk-spatial/1","inputSessionID":"session","routingGeneration":"generation","expiresAtHostUs":6000000,"clockSamples":[{"hostSentUs":999970,"serverReceivedUs":999968,"serverSentUs":999968,"hostReceivedUs":999970},{"hostSentUs":999980,"serverReceivedUs":999978,"serverSentUs":999978,"hostReceivedUs":999980},{"hostSentUs":999990,"serverReceivedUs":999988,"serverSentUs":999988,"hostReceivedUs":999990}]})";
 CHECK(r.route("/spatial/renew",renewal,1000010).status==200);
 CHECK(r.route("/spatial/presentation",presentation(),1000010).status==200);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 // Monotonicity still applies to the host's immutable frontier.
 std::string backwards=presentation();replace(backwards,"\"proofUntilUs\":1000000","\"proofUntilUs\":999999");
 CHECK(r.route("/spatial/presentation",backwards,1000010).status==400);
 // An actual rewrite of an overlapping sealed candidate set remains forbidden.
 std::string rewritten=presentation();replace(rewritten,"\"presentedFromUs\":999000","\"presentedFromUs\":999001");
 CHECK(r.route("/spatial/presentation",rewritten,1000010).status==400);
 // Once a sealed prefix is discarded, it cannot be reopened (and rewritten)
 // outside the overlap checked against the remaining rolling window.
 std::string advanced=presentation();
 replace(advanced,"\"proofFromUs\":999000","\"proofFromUs\":999100");
 replace(advanced,"\"presentedFromUs\":999000","\"presentedFromUs\":999100");
 CHECK(r.route("/spatial/presentation",advanced,1000010).status==200);
 std::string reopened=presentation();
 replace(reopened,"\"proofUntilUs\":1000000","\"proofUntilUs\":1000001");
 replace(reopened,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":1000001");
 replace(reopened,"\"presentedFromUs\":999000","\"presentedFromUs\":999050");
 CHECK(r.route("/spatial/presentation",reopened,1000010).status==400);
}
std::string shiftedRenewal(int offset)
{
 std::string samples;
 for(int64_t host:{999970,999980,999990})
 {
  if(!samples.empty())samples+=",";
  samples+="{\"hostSentUs\":"+std::to_string(host)+",\"serverReceivedUs\":"+std::to_string(host+offset)+
   ",\"serverSentUs\":"+std::to_string(host+offset)+",\"hostReceivedUs\":"+std::to_string(host)+"}";
 }
 return "{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expiresAtHostUs\":6000000,\"clockSamples\":["+samples+"]}";
}
void clockRefinementPreservesHostCandidatesAcrossBatchedClippedProofs()
{
 for(int offset:{-2,2})for(bool cached:{false,true})
 {
  Runtime r;ready(r);r.addFrame(frame(1));r.addFrame(frame(2,5));bool depthReady=!cached;
  unsigned firstFrameQueries=0;
  r.setVisibilityOracle([&](const Frame& f,const Target&,const Vec3&,double){
   if(f.id==1)++firstFrameQueries;
   return depthReady?Visibility::Visible:Visibility::Pending;
  });
  r.setLiveTargetOracle([](uint64_t i,uint64_t n){return i==7&&n==70;});
  // Both frames are possible at the host boundary 999990. Shared clock-offset
  // uncertainty must not introduce frame 1 for the exact frame-2 event.
  std::string complete=presentation(true);
  std::string initial=complete;
  if(!cached)
  {
   initial=presentation();
   replace(initial,"\"proofUntilUs\":1000000","\"proofUntilUs\":999985");
   replace(initial,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":999985");
  }
  CHECK(r.route("/spatial/presentation",initial,1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("host_release","released",0,999991),1000000).status==200);
  // With uncertainty 1 both frames are genuinely possible; the pending depth
  // query has already cached both before the renewal. Without it, proof is late.
  CHECK(r.route("/spatial/input",edgeAt("host_shot","pressed",1,999991,cached?1:0),1000000).status==200);
  CHECK(r.events().empty());
  replace(complete,"\"proofFromUs\":999000","\"proofFromUs\":999100");
  replace(complete,"\"presentedFromUs\":999000","\"presentedFromUs\":999100");
  const std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":[],\"renew\":"+shiftedRenewal(offset)+",\"presentation\":"+complete+"}";
  CHECK(r.route("/spatial/update",batch,1000010).status==200);
  depthReady=true;r.tick(1000010,true);
  CHECK(r.events().size()==1&&r.events().back().result==(cached?"ambiguous_presentation":"miss"));
  CHECK(cached?firstFrameQueries>0:firstFrameQueries==0);
  CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 }
}
// The rolling clock probes are distinct and renewed every 500 ms in the long
// session fixture. Lease renewal deliberately does not reset action sequences.
std::string renewAt(int64_t now)
{
 std::string samples;
 for(int64_t time:{now-3000,now-2000,now-1000})
 {
  if(!samples.empty())samples+=",";
  samples+="{\"hostSentUs\":"+std::to_string(time)+",\"serverReceivedUs\":"+std::to_string(time)+
   ",\"serverSentUs\":"+std::to_string(time)+",\"hostReceivedUs\":"+std::to_string(time)+"}";
 }
 return "{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expiresAtHostUs\":"+
  std::to_string(now+5000000)+",\"clockSamples\":["+samples+"]}";
}
size_t recentEventCount(const Runtime& r)
{
 const std::string state=r.stateJSON(),key="\"recentEvents\":";
 const size_t at=state.find(key);CHECK(at!=std::string::npos);
 return std::stoul(state.substr(at+key.size()));
}
std::string secondSource(std::string value)
{
 const std::string from="\"sourceId\":\"s1\"",to="\"sourceId\":\"s2\"";
 size_t at=0;while((at=value.find(from,at))!=std::string::npos){value.replace(at,from.size(),to);at+=to.size();}
 return value;
}
void longSessionRetiresReceiptsWithoutLosingSequenceGuards()
{
 Runtime r;ready(r);std::string first;r.addFrame(frame(1));
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 // 6000 edges over 30 seconds exceeds the old lifetime cap. Secondary edges
 // avoid the independent primary fire-rate limit. Keep presentation available:
 // missing-history disarms are a different concern from receipt retirement.
 for(unsigned i=0;i<6000;++i)
 {
  const int64_t now=1000000+(i+1)*5000;
  if((i+1)%100==0)CHECK(r.route("/spatial/renew",renewAt(now),now).status==200);
  r.tick(now,true);
  auto proof=presentation();
  replace(proof,"999000",std::to_string(now-2000));replace(proof,"1000000",std::to_string(now-100));
  replace(proof,"999000",std::to_string(now-2000));replace(proof,"1000000",std::to_string(now-100));
  CHECK(r.route("/spatial/presentation",proof,now).status==200);
  const std::string packet=edgeAt("long_"+std::to_string(i),i%2?"pressed":"released",i,now-1000,0,"secondary");
  if(i==0)first=packet;
  CHECK(r.route("/spatial/input",packet,now).status==200);
  CHECK(recentEventCount(r)<=512);
  CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 }
 CHECK(recentEventCount(r)<512);
 // The UUID has retired, but the exact canonical packet's sequence never did.
 Reply replay=r.route("/spatial/input",first,31000000);
 CHECK(replay.status==400&&replay.json.find("action_out_of_order")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
}
void recentUUIDWindowUsesReceiveTimeAndPreservesContinuitySequence()
{
 Runtime r;ready(r);
 const std::string original=edgeAt("recent","released",0,999990);
 CHECK(r.route("/spatial/input",original,1000000).status==200);
 Reply duplicate=r.route("/spatial/input",edgeAt("recent","released",1,999995),1000000);
 CHECK(duplicate.status==409&&duplicate.json.find("duplicate_event")!=std::string::npos);
 // A denied future event still consumes its sequence and a receive-time receipt.
 const std::string future=edgeAt("future_receipt","pressed",1,4000000);
 CHECK(r.route("/spatial/input",future,1000000).status==409);
 CHECK(recentEventCount(r)==2);
 CHECK(r.route("/spatial/renew",renewAt(2000000),2000000).status==200);
 r.tick(2000000,true);CHECK(recentEventCount(r)==0);
 Reply replay=r.route("/spatial/input",future,2000000);
 CHECK(replay.status==400&&replay.json.find("action_out_of_order")!=std::string::npos);
 // UUID reuse after the window with a new sequence/time is a different packet.
 CHECK(r.route("/spatial/input",edgeAt("recent","released",2,1999000),2000000).status==200);
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),2000000).status==200);
 std::string tracked=message("\"type\":\"sourceState\",\"sequence\":2,\"state\":\"tracked\",\"reason\":\"reacquired\"");
 replace(tracked,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(r.route("/spatial/input",tracked,2000000).status==200);
 std::string reusedSequence=edgeAt("fresh_continuity_old_sequence","released",2,1999000);
 replace(reusedSequence,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(r.route("/spatial/input",reusedSequence,2000000).status==400);
}
void sourceQuotaCannotRevokeOrStarveOtherCrew()
{
 Runtime r;std::string grant=prepare();
 replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");
 replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]",
  "\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 const std::string tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
 CHECK(r.route("/spatial/input",tracked,1000000).status==200);
 CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 for(unsigned i=0;i<512;++i)
  CHECK(r.route("/spatial/input",edgeAt("flood_"+std::to_string(i),"released",i,999990,0,"secondary"),1000000).status==200);
 CHECK(recentEventCount(r)==512);
 const std::string overflow=edgeAt("overflow","pressed",512,999990,0,"secondary");
 Reply denied=r.route("/spatial/input",overflow,1000000);
 CHECK(denied.status==429&&denied.json.find("source_replay_capacity_exceeded")!=std::string::npos);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Rejected&&r.reticles()[0].feedback.reason=="source_replay_capacity_exceeded");
 CHECK(!r.reticles()[0].primaryHeld&&!r.reticles()[0].toolHeld&&recentEventCount(r)==512);
 CHECK(r.route("/spatial/input",edgeAt("quota_sequence_replay","released",512,999990),1000000).status==400);
 Reply crossSource=r.route("/spatial/input",secondSource(edgeAt("flood_0","released",0,999990,0,"secondary")),1000000);
 CHECK(crossSource.status==409&&crossSource.json.find("duplicate_event")!=std::string::npos);
 CHECK(r.route("/spatial/input",secondSource(edgeAt("other_release","released",0,999990,0,"secondary")),1000000).status==200);
 CHECK(r.route("/spatial/input",secondSource(edgeAt("other_press","pressed",1,999990,0,"secondary")),1000000).status==200);
 CHECK(r.reticles()[1].toolHeld&&recentEventCount(r)==514);
 // Operational admission errors stay per-input results through the real batch API.
 const std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":["+
  edgeAt("overflow_again","pressed",513,999990,0,"secondary")+"]}";
 Reply batched=r.route("/spatial/update",batch,1000000);
 CHECK(batched.status==200&&batched.json.find("source_replay_capacity_exceeded")!=std::string::npos);
 CHECK(r.reticles()[1].toolHeld&&r.stateJSON().find("\"pending\":1")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 CHECK(r.route("/spatial/renew",renewAt(2000000),2000000).status==200);r.tick(2000000,true);
 CHECK(recentEventCount(r)==0);
 CHECK(r.route("/spatial/input",edgeAt("quota_recovered","released",514,1999000),2000000).status==200);
 CHECK(recentEventCount(r)==1);
 Reply oldOverflow=r.route("/spatial/input",overflow,2000000);
 CHECK(oldOverflow.status==400&&oldOverflow.json.find("action_out_of_order")!=std::string::npos);
}
void retiredProofPrefixIsOperationalButRewriteAndUnknownIDsAreFatal()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 for(uint64_t id=2;id<=65;++id)r.addFrame(frame(id));
 const std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"presentation\":"+presentation()+
  ",\"inputs\":["+edge("still_releases","released",0)+"]}";
 Reply retired=r.route("/spatial/update",batch,1000000);
 CHECK(retired.status==200&&retired.json.find("\"presentationAccepted\":false")!=std::string::npos);
 CHECK(retired.json.find("frame_history_unavailable")!=std::string::npos&&retired.json.find("released")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 // Missing old metadata cannot hide rewriting a previously sealed interval.
 std::string rewrite=presentation();replace(rewrite,"\"presentedFromUs\":999000","\"presentedFromUs\":999001");
 Reply malformed=r.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"presentation\":"+rewrite+",\"inputs\":[]}",1000000);
 CHECK(malformed.status==400&&malformed.json.find("presentation_rewrite")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":false")!=std::string::npos);
 for(uint64_t unknown:{1,66})
 {
  Runtime fresh;ready(fresh);fresh.addFrame(frame(65));
  std::string proof=presentation();replace(proof,"\"frameId\":1","\"frameId\":"+std::to_string(unknown));
  Reply denied=fresh.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"presentation\":"+proof+",\"inputs\":[]}",1000000);
  // Merely being below the first ever recorded ID does not prove retirement.
  CHECK(denied.status==400&&denied.json.find("unknown_presentation_frame")!=std::string::npos);
  CHECK(fresh.stateJSON().find("\"prepared\":false")!=std::string::npos);
 }
}
void neverPublishedGapIsNotMistakenForRetiredMetadata()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 for(uint64_t id=3;id<=67;++id)r.addFrame(frame(id));
 // Actual retired IDs include 1 and 3; an integer watermark would incorrectly
 // classify unpublished frame 2 as ordinary history eviction.
 std::string proof=presentation();replace(proof,"\"frameId\":1","\"frameId\":2");
 Reply denied=r.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"presentation\":"+proof+",\"inputs\":[]}",1000000);
 CHECK(denied.status==400&&denied.json.find("unknown_presentation_frame")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":false")!=std::string::npos);
}
void pendingCandidatesOwnImmutableMetadataAndAllDepthLeases()
{
 Runtime r;ready(r);r.addFrame(frame(1));r.addFrame(frame(2));
 CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
 bool gpuReady=false;unsigned pins=0,queries=0;
 std::vector<std::weak_ptr<void>> leases;
 r.setFramePinOracle([&](const Frame& f){
  CHECK(f.id==1||f.id==2);++pins;
  std::shared_ptr<void> lease=std::make_shared<unsigned>(unsigned(f.id));leases.push_back(lease);return lease;
 });
 r.setVisibilityOracle([&](const Frame& f,const Target& target,const Vec3&,double){
  // Both candidates must be acquired before even the first asynchronous query.
  CHECK(pins==2&&f.visibilityLease&&target.id==7&&target.instance==70);++queries;
  return gpuReady?Visibility::Visible:Visibility::Pending;
 });
 r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
 CHECK(r.route("/spatial/input",edgeAt("pinned_release","released",0,999991),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("pinned_shot","pressed",1,999991,1),1000000).status==200);
 CHECK(pins==2&&queries==1&&r.events().empty());
 // Rolling metadata evicts BOTH originals. The event still owns its actual
 // camera/targets and both producer resources, rather than looking IDs up again.
 for(uint64_t id=3;id<=66;++id)r.addFrame(frame(id,5));
 CHECK(!leases[0].expired()&&!leases[1].expired());
 gpuReady=true;r.tick(1000010,true);
 CHECK(r.events().size()==1&&r.events().back().result=="hit");
 CHECK(leases[0].expired()&&leases[1].expired());
}
void unavailableCandidateNeverProducesPartialHitAndLateProofCannotPin()
{
 Runtime r;ready(r);r.addFrame(frame(1));r.addFrame(frame(2));unsigned pins=0,queries=0;
 CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
 // Simulate the second depth frame retiring after proof admission.
 r.setFramePinOracle([&](const Frame& f)->std::shared_ptr<void>{++pins;return f.id==1?std::make_shared<unsigned>(1):std::shared_ptr<void>();});
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/input",edgeAt("missing_release","released",0,999991),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("missing_shot","pressed",1,999991,1),1000000).status==200);
 CHECK(pins==2&&queries==0&&r.events().size()==1&&r.events().back().result=="depth_history_unavailable");
 CHECK(!r.reticles()[0].primaryHeld&&!r.reticles()[0].toolHeld);
 CHECK(r.route("/spatial/input",edgeAt("missing_retry_needs_release","pressed",2,999991,1),1000000).status==409);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 Runtime late;ready(late);late.addFrame(frame(1));unsigned latePins=0;
 late.setFramePinOracle([&](const Frame&)->std::shared_ptr<void>{++latePins;return std::make_shared<unsigned>(1);});
 CHECK(late.route("/spatial/input",edgeAt("late_release","released",0,999990),1000000).status==200);
 CHECK(late.route("/spatial/input",edgeAt("late_shot","pressed",1,999990),1000000).status==200);
 std::string proof=presentation();replace(proof,"\"proofUntilUs\":1000000","\"proofUntilUs\":1060000");
 replace(proof,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":1060000");
 CHECK(late.route("/spatial/presentation",proof,1060000).status==200);
 // One immediate readiness probe is released; no pending candidate is pinned.
 CHECK(latePins==1&&late.events().size()==1&&late.events().back().result=="frame_history_unavailable");
}
void restrictiveHistoryHintDisarmsAndRecoveryRequiresPostReadinessRelease()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 CHECK(r.route("/spatial/input",edgeAt("history_release","released",0,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("history_hold","pressed",1,999990,0,"secondary"),1000000).status==200);
 CHECK(r.reticles()[0].toolHeld);
 const std::string unavailable="{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":false,\"inputs\":["+
  edgeAt("unready_release","released",2,999990,0,"secondary")+","+edgeAt("unready_press","pressed",3,999990,0,"secondary")+"]}";
 Reply denied=r.route("/spatial/update",unavailable,1000000);
 CHECK(denied.status==200&&denied.json.find("history_unavailable")!=std::string::npos);
 CHECK(!r.reticles()[0].toolHeld&&r.stateJSON().find("\"pending\":0")!=std::string::npos);
 CHECK(r.stateJSON().find("\"historyReady\":false")!=std::string::npos);
 // A positive host hint grants nothing without server-accepted producer proof.
 const std::string hintOnly="{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":true,\"inputs\":["+
  edgeAt("hint_release","released",4,999990,0,"secondary")+","+edgeAt("hint_press","pressed",5,999990,0,"secondary")+"]}";
 CHECK(r.route("/spatial/update",hintOnly,1000000).status==200);
 CHECK(!r.reticles()[0].toolHeld&&r.stateJSON().find("\"historyReady\":false")!=std::string::npos);
 const std::string recovery="{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":true,\"presentation\":"+presentation()+",\"inputs\":[]}";
 CHECK(r.route("/spatial/update",recovery,1001000).status==200);
 CHECK(r.stateJSON().find("\"historyReady\":true")!=std::string::npos);
 Reply queuedRelease=r.route("/spatial/input",edgeAt("before_recovery_release","released",6,1000999,0,"secondary"),1002000);
 CHECK(queuedRelease.status==409&&queuedRelease.json.find("event_precedes_gameplay_activation")!=std::string::npos);
 Reply held=r.route("/spatial/input",edgeAt("after_recovery_still_down","pressed",7,1001500,0,"secondary"),1002000);
 CHECK(held.status==409&&held.json.find("release_required")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("after_recovery_release","released",8,1002000,0,"secondary"),1003000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("after_recovery_press","pressed",9,1002100,0,"secondary"),1003000).status==200);
 CHECK(r.reticles()[0].toolHeld&&r.stateJSON().find("\"prepared\":true")!=std::string::npos);
}
void actualDepthReadinessOverridesPositiveHintWithoutRevokingGrant()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 bool available=false;unsigned probes=0;
 r.setFramePinOracle([&](const Frame&)->std::shared_ptr<void>{++probes;return available?std::make_shared<unsigned>(1):std::shared_ptr<void>();});
 const std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":true,\"presentation\":"+presentation()+
  ",\"inputs\":["+edgeAt("depth_lost_release","released",0,999990,0,"secondary")+","+
  edgeAt("depth_lost_press","pressed",1,999990,0,"secondary")+"]}";
 Reply failed=r.route("/spatial/update",batch,1000000);
 CHECK(failed.status==200&&failed.json.find("\"presentationAccepted\":false")!=std::string::npos);
 CHECK(failed.json.find("depth_history_unavailable")!=std::string::npos);
 CHECK(probes==1&&!r.reticles()[0].toolHeld&&r.stateJSON().find("\"historyReady\":false")!=std::string::npos);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 available=true;
 const std::string recovery="{\"contract\":\"tholus.stk-spatial/1\",\"presentation\":"+presentation()+",\"inputs\":[]}";
 CHECK(r.route("/spatial/update",recovery,1001000).status==200);
 CHECK(probes==2&&r.stateJSON().find("\"historyReady\":true")!=std::string::npos);
 // An explicit restrictive hint still wins over otherwise valid proof.
 const std::string restricted="{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":false,\"presentation\":"+presentation()+",\"inputs\":[]}";
 CHECK(r.route("/spatial/update",restricted,1002000).status==200);
 CHECK(r.stateJSON().find("\"historyReady\":false")!=std::string::npos);
}
std::string feedbackAim(int64_t time,double u,double v,unsigned sequence=0)
{
 const std::string q=std::to_string(sequence);
 return message("\"type\":\"aim\",\"sequence\":"+q+",\"time\":{\"evaluatedAtHostUs\":"+std::to_string(time)+
  ",\"uncertaintyUs\":0},\"projectionContextId\":\"projection\",\"ray\":"+pose,
  "{\"u\":"+std::to_string(u)+",\"v\":"+std::to_string(v)+",\"viewId\":\"main\",\"sourceId\":\"s1\",\"sequence\":"+q+",\"projectionContextId\":\"projection\"}");
}
void supersededAimCannotMoveOrCancelNewerPendingPress()
{
 for(bool batched:{false,true})for(int64_t aimTime:{940000,999990,1047990})
 {
  Runtime r;ready(r);r.addFrame(frame(1));bool complete=false;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return complete?Visibility::Visible:Visibility::Pending;});
  r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
  std::string proof=presentation();
  replace(proof,"\"proofUntilUs\":1000000","\"proofUntilUs\":1049000");
  replace(proof,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":1049000");
  CHECK(r.route("/spatial/presentation",proof,1049000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("order-release","released",0,1046990),1049000).status==200);
  const std::string press=edgeAt("order-press","pressed",1,1047990);
  // Sequence 999 is legal: aim and action sequence spaces are independent.
  const std::string aim=feedbackAim(aimTime,0.75,0.5,999);
  if(batched)
   CHECK(r.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":["+press+","+aim+"]}",1049000).status==200);
  else
  {
   CHECK(r.route("/spatial/input",press,1049000).status==200);
   const int64_t updated=r.reticles()[0].updatedUs;
   CHECK(r.route("/spatial/input",aim,1050000).status==200);
   CHECK(r.reticles()[0].updatedUs==updated);
  }
  CHECK(r.events().empty()&&r.reticles()[0].valid&&r.reticles()[0].u==0.5&&r.reticles()[0].primaryHeld);
  // The old valid aim is now >60ms old; the event-time cursor is only 13ms old.
  r.tick(1061000,true);CHECK(r.events().empty()&&r.reticles()[0].valid&&r.reticles()[0].primaryHeld);
  complete=true;r.tick(1061001,true);
  CHECK(r.events().size()==1&&r.events().back().result=="hit"&&r.events().back().targetId==7);
  CHECK(r.route("/spatial/input",aim,1061001).status==400); // Supersession consumed its aim watermark.
 }
}
void olderActionResolvesItsOwnRayWithoutOverwritingNewerAim()
{
 for(bool aimFirst:{false,true})
 {
  Runtime r;ready(r);r.addFrame(frame(1));
  r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
  r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("cursor-release","released",0),1000000).status==200);
  const std::string aim=feedbackAim(1004000,0.75,0.5,999),press=edge("cursor-press","pressed",1);
  const std::string batch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":["+(aimFirst?aim+","+press:press+","+aim)+"]}";
  CHECK(r.route("/spatial/update",batch,1005000).status==200);
  CHECK(r.events().size()==1&&r.events().back().result=="hit"&&r.events().back().targetId==7);
  CHECK(r.reticles()[0].valid&&r.reticles()[0].u==0.75&&r.reticles()[0].primaryHeld);
  CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Hit&&r.reticles()[0].feedback.u==0.5);
 }
}
void deniedPressCannotSuppressItsPrecedingUsefulAim()
{
 Runtime r;ready(r);r.addFrame(frame(1));
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("admitted-release","released",0),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("admitted-press","pressed",1),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("limited-release","released",2,1004000),1006000).status==200);
 const Reply denied=r.route("/spatial/input",edgeAt("limited-press","pressed",3,1005000),1006000);
 CHECK(denied.status==409&&denied.json.find("fire_rate_limited")!=std::string::npos);
 CHECK(r.route("/spatial/input",feedbackAim(1004000,0.75,0.5),1006000).status==200);
 CHECK(r.reticles()[0].valid&&r.reticles()[0].u==0.75&&!r.reticles()[0].primaryHeld);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Rejected);
 CHECK(r.events().size()==1&&r.events().back().result=="hit");
}
std::string withoutFeedbackPoint(std::string packet)
{
 const size_t point=packet.rfind(",\"point\":");CHECK(point!=std::string::npos);packet.resize(point);return packet+"}";
}
void resolvedFeedbackSurvivesAimReleaseDuplicatesAndStaleCursorUntilExpiry()
{
 for(bool hit:{false,true})
 {
  Runtime r;ready(r);r.addFrame(frame(1));bool complete=false;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return !complete?Visibility::Pending:hit?Visibility::Visible:Visibility::Occluded;});
  r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("feedback_release","released",0),1000000).status==200);
  const std::string shot=edge("feedback_shot","pressed",1);
  CHECK(r.route("/spatial/input",shot,1000000).status==200);
  const Feedback pending=r.reticles()[0].feedback;
  CHECK(pending.kind==FeedbackKind::Pending&&pending.untilUs==1250000&&pending.actionSequence==1);
  complete=true;r.tick(1005000,true);
  const Feedback outcome=r.reticles()[0].feedback;
  CHECK(outcome.kind==(hit?FeedbackKind::Hit:FeedbackKind::Miss)&&outcome.startedUs==1005000&&outcome.untilUs==1505000);
  CHECK(outcome.hasPoint&&outcome.u==0.5&&outcome.v==0.5);
  CHECK(r.route("/spatial/input",feedbackAim(1004000,0.75,0.5),1005000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("after_feedback_release","released",2,1004000),1005000).status==200);
  CHECK(r.reticles()[0].u==0.75&&r.reticles()[0].feedback.u==0.5&&r.reticles()[0].feedback.kind==outcome.kind);
  CHECK(r.route("/spatial/input",shot,1005000).status==409);
  CHECK(r.route("/spatial/input",edgeAt("feedback_shot","pressed",3,1004000),1005000).status==409);
  CHECK(r.reticles()[0].feedback.kind==outcome.kind&&r.reticles()[0].feedback.startedUs==1005000);
  r.tick(1504999,true);CHECK(!r.reticles()[0].valid&&r.reticles()[0].feedback.kind==outcome.kind);
  CHECK(Runtime::feedbackForRendering(r.reticles()[0],1505000).kind==FeedbackKind::None);
  r.tick(1505000,true);CHECK(r.reticles()[0].feedback.kind==FeedbackKind::None);
 }
}
void rejectedFeedbackUsesQualifiedPointLastAnchorOrSourceBadge()
{
 Runtime badge;ready(badge);
 CHECK(badge.route("/spatial/input",edge("badge_release","released",0),1000000).status==200);
 CHECK(badge.route("/spatial/input",withoutFeedbackPoint(edge("badge_press","pressed",1)),1000000).status==409);
 CHECK(badge.reticles()[0].feedback.kind==FeedbackKind::Rejected&&!badge.reticles()[0].feedback.hasPoint);
 CHECK(badge.reticles()[0].feedback.reason=="point_unavailable");
 CHECK(badge.route("/spatial/input",feedbackAim(999990,0.75,0.5),1000000).status==200);
 CHECK(badge.route("/spatial/input",edge("anchor_release","released",2),1000000).status==200);
 CHECK(badge.route("/spatial/input",withoutFeedbackPoint(edge("anchor_press","pressed",3)),1000000).status==409);
 const Feedback anchor=badge.reticles()[0].feedback;
 CHECK(anchor.kind==FeedbackKind::Rejected&&anchor.hasPoint&&anchor.u==0.75&&anchor.v==0.5);
 CHECK(badge.route("/spatial/input",edgeAt("future_feedback","pressed",4,2000000),1000000).status==409);
 CHECK(badge.reticles()[0].feedback.kind==FeedbackKind::Rejected&&badge.reticles()[0].feedback.reason=="stale_or_uncertain_pose");
 Runtime rate;ready(rate);
 CHECK(rate.route("/spatial/input",edge("rate_release","released",0),1000000).status==200);
 CHECK(rate.route("/spatial/input",edge("rate_first","pressed",1),1000000).status==200);
 CHECK(rate.route("/spatial/input",edge("rate_again_release","released",2),1000000).status==200);
 CHECK(rate.route("/spatial/input",edge("rate_again","pressed",3),1000000).status==409);
 CHECK(rate.reticles()[0].feedback.kind==FeedbackKind::Rejected&&rate.reticles()[0].feedback.reason=="fire_rate_limited");
 Runtime release;ready(release);
 CHECK(release.route("/spatial/input",edge("no_feedback_release","pressed",0),1000000).status==409);
 CHECK(release.reticles()[0].feedback.kind==FeedbackKind::Rejected&&release.reticles()[0].feedback.reason=="release_required");
}
void pendingFeedbackBecomesRejectedOnTimeoutOrStaleAimAndClearsOnPause()
{
 Runtime timeout;ready(timeout);timeout.addFrame(frame(1));
 timeout.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Pending;});
 CHECK(timeout.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(timeout.route("/spatial/input",edge("timeout_release","released",0),1000000).status==200);
 CHECK(timeout.route("/spatial/input",edge("timeout_shot","pressed",1),1000000).status==200);
 CHECK(timeout.route("/spatial/presentation",presentation(),1250001).status==200);
 const Feedback rejected=timeout.reticles()[0].feedback;
 CHECK(rejected.kind==FeedbackKind::Rejected&&rejected.reason=="history_timeout");
 CHECK(rejected.startedUs==1250001&&rejected.untilUs==1750001&&rejected.u==0.5);
 Runtime stale;ready(stale);
 CHECK(stale.route("/spatial/input",edge("stale_release","released",0),1000000).status==200);
 CHECK(stale.route("/spatial/input",edge("stale_shot","pressed",1),1000000).status==200);
 stale.tick(1060010,true);
 CHECK(stale.reticles()[0].feedback.kind==FeedbackKind::Rejected&&stale.reticles()[0].feedback.reason=="aim_stale");
 stale.tick(1061000,false);CHECK(stale.reticles()[0].feedback.kind==FeedbackKind::None);
 CHECK(stale.route("/spatial/input",edgeAt("paused_feedback","pressed",2,1061500),1062000).status==409);
 CHECK(stale.reticles()[0].feedback.kind==FeedbackKind::Rejected);
 stale.tick(1562000,false);CHECK(stale.reticles()[0].feedback.kind==FeedbackKind::None);
}
void olderAsyncHitCannotOverwriteNewerMissFeedback()
{
 Runtime r;ready(r);r.addFrame(frame(1));bool complete=false;
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return complete?Visibility::Visible:Visibility::Pending;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("old_release","released",0,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("old_async","pressed",1,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new_release","released",2,999990,0,"secondary"),1000000).status==200);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Pending&&r.reticles()[0].feedback.actionSequence==1);
 std::string newer=edgeAt("new_miss","pressed",3,999990,0,"secondary");replace(newer,"\"u\":0.5","\"u\":0.75");
 CHECK(r.route("/spatial/input",newer,1000000).status==200);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Miss&&r.reticles()[0].feedback.actionSequence==3);
 complete=true;r.tick(1000010,true);
 CHECK(r.events().size()==2&&r.events().back().eventId=="old_async"&&r.events().back().result=="hit");
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Miss&&r.reticles()[0].feedback.actionSequence==3&&r.reticles()[0].feedback.u==0.75);
}
void feedbackIsIndependentPerSourceAndClearsOnCancelAndNewSession()
{
 Runtime r;std::string grant=prepare();
 replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");
 replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]",
  "\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 const std::string tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
 CHECK(r.route("/spatial/input",tracked,1000000).status==200);
 CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 r.addFrame(frame(1));r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("first_rejected","pressed",0),1000000).status==409);
 CHECK(r.route("/spatial/input",secondSource(edge("second_release","released",0)),1000000).status==200);
 std::string miss=secondSource(edge("second_miss","pressed",1));replace(miss,"\"u\":0.5","\"u\":0.75");
 CHECK(r.route("/spatial/input",miss,1000000).status==200);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Rejected&&r.reticles()[1].feedback.kind==FeedbackKind::Miss);
 CHECK(r.route("/spatial/input",edge("first_release","released",1),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("first_hit","pressed",2),1000000).status==200);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Hit&&r.reticles()[1].feedback.kind==FeedbackKind::Miss);
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),1000000).status==200);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::None&&r.reticles()[1].feedback.kind==FeedbackKind::Miss);
 r.reset("route_changed");CHECK(r.reticles()[1].feedback.kind==FeedbackKind::None);
 std::string next=prepare();replace(next,"\"inputSessionID\":\"session\"","\"inputSessionID\":\"next_session\"");
 CHECK(r.route("/spatial/prepare",next,1000000).status==200);
 CHECK(r.reticles().size()==1&&r.reticles()[0].feedback.kind==FeedbackKind::None);
}
void historyLossRejectsCanceledPendingButPreservesRealTerminalOutcome()
{
 for(bool completed:{false,true})
 {
  Runtime r;ready(r);r.addFrame(frame(1));
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return completed?Visibility::Visible:Visibility::Pending;});
  r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("resource_release","released",0),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("resource_shot","pressed",1),1000000).status==200);
  CHECK(r.reticles()[0].feedback.kind==(completed?FeedbackKind::Hit:FeedbackKind::Pending));
  CHECK(r.route("/spatial/update","{\"contract\":\"tholus.stk-spatial/1\",\"historyReady\":false,\"inputs\":[]}",1000100).status==200);
  const Feedback feedback=r.reticles()[0].feedback;
  CHECK(feedback.kind==(completed?FeedbackKind::Hit:FeedbackKind::Rejected));
  CHECK(feedback.startedUs==(completed?1000000:1000100)&&feedback.untilUs==(completed?1500000:1500100));
  CHECK(feedback.reason==(completed?"hit":"history_unavailable")&&feedback.hasPoint&&feedback.u==0.5);
  CHECK(r.stateJSON().find("\"pending\":0")!=std::string::npos);
  CHECK(completed?r.events().size()==1:r.events().empty());
  r.tick(1000200,false);CHECK(r.reticles()[0].feedback.kind==FeedbackKind::None);
 }
}
void boundedPendingBackpressureHasRejectedFeedbackWithoutFalseOutcome()
{
 Runtime r;ready(r);
 for(unsigned i=0;i<64;++i)
 {
  CHECK(r.route("/spatial/input",edgeAt("capacity_release_"+std::to_string(i),"released",i*2,999990,0,"secondary"),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("capacity_press_"+std::to_string(i),"pressed",i*2+1,999990,0,"secondary"),1000000).status==200);
 }
 CHECK(r.stateJSON().find("\"pending\":64")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("capacity_final_release","released",128,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("capacity_final_press","pressed",129,999990,0,"secondary"),1000000).status==429);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Rejected&&r.reticles()[0].feedback.reason=="input_backpressure");
 CHECK(r.reticles()[0].feedback.actionSequence==129&&r.events().empty());
 CHECK(r.stateJSON().find("\"pending\":0")!=std::string::npos&&r.stateJSON().find("\"prepared\":true")!=std::string::npos);
}
void historicalCameraPoseWinsOverMovingCurrentCamera()
{
 Runtime r;ready(r);Frame first=frame(1),second=frame(2);
 second.captureUs=995100;second.cameraPosition=Vec3(5,0,0);
 second.cameraRight=Vec3(0,0,-1);second.cameraForward=Vec3(1,0,0);
 r.addFrame(first);r.addFrame(second);std::vector<uint64_t> visibilityFrames;
 r.setVisibilityOracle([&](const Frame& f,const Target&,const Vec3&,double){visibilityFrames.push_back(f.id);return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
 CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("before_camera_move_release","released",0,999985,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("before_camera_move_hit","pressed",1,999985,0,"secondary"),1000000).status==200);
 CHECK(r.events().size()==1&&r.events().back().result=="hit"&&r.events().back().frameId==1);
 CHECK(r.route("/spatial/input",edgeAt("after_camera_move_release","released",2,999995,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("after_camera_move_miss","pressed",3,999995,0,"secondary"),1000000).status==200);
 CHECK(r.events().size()==2&&r.events().back().result=="miss"&&r.events().back().frameId==2);
 CHECK(visibilityFrames.size()==1&&visibilityFrames[0]==1);
}
std::string crewSource(std::string packet,unsigned number)
{
 const std::string from="\"sourceId\":\"s1\"",to="\"sourceId\":\"s"+std::to_string(number)+"\"";
 size_t at=0;while((at=packet.find(from,at))!=std::string::npos){packet.replace(at,from.size(),to);at+=to.size();}
 return packet;
}
void fiveSourcesKeepOwnershipAndFeedbackIndependentThroughOneTrackingLoss()
{
 Runtime r;std::string grant=prepare(),sources,bindings;
 for(unsigned source=1;source<=5;++source)
 {
  if(!sources.empty()){sources+=",";bindings+=",";}
  sources+="{"+crewSource(identity,source)+"}";
  bindings+="{\"sourceId\":\"s"+std::to_string(source)+"\",\"participantId\":\"person"+std::to_string(source)+"\"}";
 }
 replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":["+sources+"]");
 replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]","\"bindings\":["+bindings+"]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 const std::string tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
 for(unsigned source=1;source<=5;++source)CHECK(r.route("/spatial/input",crewSource(tracked,source),1000000).status==200);
 r.addFrame(frame(1));r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 for(unsigned source=1;source<=5;++source)
 {
  CHECK(r.route("/spatial/input",crewSource(edgeAt("crew_release_"+std::to_string(source),"released",0,999990,0,"secondary"),source),1000000).status==200);
  std::string shot=crewSource(edgeAt("crew_shot_"+std::to_string(source),"pressed",1,999990,0,"secondary"),source);
  if(source%2==0)replace(shot,"\"u\":0.5","\"u\":0.75");
  CHECK(r.route("/spatial/input",shot,1000000).status==200);
  CHECK(r.events().back().sourceId=="s"+std::to_string(source)&&r.events().back().participantId=="person"+std::to_string(source));
  CHECK(r.reticles()[source-1].feedback.kind==(source%2?FeedbackKind::Hit:FeedbackKind::Miss));
 }
 const std::string lost=crewSource(message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),3);
 CHECK(r.route("/spatial/input",lost,1000000).status==200);
 for(unsigned source=1;source<=5;++source)
  CHECK(r.reticles()[source-1].feedback.kind==(source==3?FeedbackKind::None:source%2?FeedbackKind::Hit:FeedbackKind::Miss));
 std::string regained=crewSource(tracked,3);replace(regained,"\"sequence\":0","\"sequence\":2");
 replace(regained,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(r.route("/spatial/input",regained,1000000).status==200);
 for(unsigned sequence=2;sequence<=3;++sequence)
 {
  std::string packet=crewSource(edgeAt("regained_"+std::to_string(sequence),sequence==2?"released":"pressed",sequence,999990,0,"secondary"),3);
  replace(packet,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
  CHECK(r.route("/spatial/input",packet,1000000).status==200);
 }
 CHECK(r.events().size()==6&&r.events().back().participantId=="person3"&&r.events().back().result=="hit");
 CHECK(r.reticles()[2].feedback.actionSequence==3&&r.reticles()[0].feedback.actionSequence==1&&r.reticles()[4].feedback.actionSequence==1);
 CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
}
void twentyMinutePrimarySessionNearFourHzRenewsEveryHalfSecond()
{
 Runtime r;ready(r);r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
 int64_t nextRenewal=1500000;
 // 251 ms = about 3.984 Hz: a 1 ms margin exceeds the clock drift bound around
 // the exact 250 ms fire-rate boundary. Renewals run on their own 500 ms schedule.
 // 4800 shots span 20 min 4.8 s, with 9600 distinct action UUIDs (>4096).
 for(unsigned shot=0;shot<4800;++shot)
 {
  const int64_t now=1000000+int64_t(shot+1)*251000;
  while(nextRenewal<=now)
  {
   CHECK(r.route("/spatial/renew",renewAt(nextRenewal),nextRenewal).status==200);
   r.tick(nextRenewal,true);nextRenewal+=500000;
  }
  r.tick(now,true);Frame captured=frame(shot+1);captured.captureUs=now-2000;r.addFrame(captured);
  const std::string proof="{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"frameStream\":\"0123456789abcdef0123456789abcdef\",\"timingRevision\":\"timing\",\"proofFromUs\":"+
   std::to_string(now-1500)+",\"proofUntilUs\":"+std::to_string(now)+",\"runs\":[{\"frameId\":"+std::to_string(shot+1)+
   ",\"presentedFromUs\":"+std::to_string(now-1500)+",\"presentedUntilUs\":"+std::to_string(now)+"}]}";
  CHECK(r.route("/spatial/presentation",proof,now).status==200);
  CHECK(r.route("/spatial/input",edgeAt("normal_release_"+std::to_string(shot),"released",shot*2,now-1000),now).status==200);
  const std::string event="normal_shot_"+std::to_string(shot);
  CHECK(r.route("/spatial/input",edgeAt(event,"pressed",shot*2+1,now-1000),now).status==200);
  CHECK(r.events().back().eventId==event&&r.events().back().result=="hit"&&r.events().back().frameId==shot+1);
  CHECK(recentEventCount(r)<=10&&r.reticles()[0].feedback.kind==FeedbackKind::Hit);
  CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 }
 CHECK(r.events().size()==256&&r.reticles()[0].feedback.actionSequence==9599);
}
// Aim the real consumer through a complete grant/proof/action exchange. The
// oracle reports every queried surface visible, so clipping cannot accidentally
// rely on an oracle to hide a wrong selection or skip a visible off-axis target.
void checkClippedShot(Frame supplied,double u,double v,uint64_t expectedTarget,const std::string& expectedResult="")
{
 Runtime r;std::string grant=prepare();std::ostringstream projection;
 projection<<std::setprecision(17)<<"\"kind\":\""<<supplied.projection.kind<<"\",\"width\":"<<supplied.projection.width
  <<",\"height\":"<<supplied.projection.height<<",\"domeTiltDeg\":"<<supplied.projection.domeTiltDeg;
 replace(grant,"\"kind\":\"equirectangular\",\"width\":2048,\"height\":1024,\"domeTiltDeg\":0",projection.str());
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\""),1000000).status==200);
 r.addFrame(supplied);unsigned queries=0;
 r.setVisibilityOracle([&](const Frame&,const Target& target,const Vec3&,double){++queries;CHECK(target.id==expectedTarget);return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("clip-release","released",0),1000000).status==200);
 std::string shot=edge("clip-shot","pressed",1);std::ostringstream uv;
 uv<<std::setprecision(17)<<"\"u\":"<<u<<",\"v\":"<<v;
 replace(shot,"\"u\":0.5,\"v\":0.5",uv.str());
 CHECK(r.route("/spatial/input",shot,1000000).status==200);
 CHECK(r.events().size()==1&&r.events().back().result==(expectedResult.empty()?(expectedTarget?"hit":"miss"):expectedResult));
 CHECK(r.events().back().targetId==expectedTarget&&queries==(expectedTarget?1u:0u));
}
Frame clippingFrame(bool domemaster)
{
 Frame result=frame(1);result.targets.clear();result.nearPlane=0.25;result.farPlane=16;
 if(domemaster){result.projection.kind="fisheye_domemaster";result.projection.width=result.projection.height=1536;}
 return result;
}
Target markerOnRay(const Frame& supplied,double u,double v,double contact,double radius,uint64_t id)
{
 Vec3 local;CHECK(Runtime::projectionDirection(supplied.projection,u,v,&local));
 const Vec3 direction(supplied.cameraRight.x*local.x+supplied.cameraUp.x*local.y+supplied.cameraForward.x*local.z,
  supplied.cameraRight.y*local.x+supplied.cameraUp.y*local.y+supplied.cameraForward.y*local.z,
  supplied.cameraRight.z*local.x+supplied.cameraUp.z*local.y+supplied.cameraForward.z*local.z);
 Target result;result.id=id;result.instance=id*10;result.radius=radius;
 result.center=Vec3(supplied.cameraPosition.x+direction.x*(contact+radius),
  supplied.cameraPosition.y+direction.y*(contact+radius),supplied.cameraPosition.z+direction.z*(contact+radius));
 return result;
}
void distantTiesCannotHideTheUniqueNearestMarkerInAnyRegistrationOrder()
{
 for(bool dome:{false,true})for(bool rotated:{false,true})
 {
  Frame supplied=clippingFrame(dome);
  if(rotated)
  {
   const double c=std::sqrt(0.5);
   supplied.cameraPosition=Vec3(3,2,-5);supplied.cameraRight=Vec3(c,0,-c);supplied.cameraForward=Vec3(c,0,c);
  }
  Target left=markerOnRay(supplied,0.5,0.5,9.65,0.35,7),right=left;
  left.center.x-=0.1*supplied.cameraRight.x;left.center.z-=0.1*supplied.cameraRight.z;
  right.center.x+=0.1*supplied.cameraRight.x;right.center.z+=0.1*supplied.cameraRight.z;
  right.id=8;right.instance=80;
  const std::vector<Target> markers={left,right,markerOnRay(supplied,0.5,0.5,4.75,0.25,9)};
  // Audit counterexample: far entries are both 9.6645898m, near entry 4.75m.
  std::vector<unsigned> order={0,1,2};
  do
  {
   supplied.targets={markers[order[0]],markers[order[1]],markers[order[2]]};
   checkClippedShot(supplied,0.5,0.5,9);
  }while(std::next_permutation(order.begin(),order.end()));
 }
}
void onlyFinalNearestSurfaceTiesRemainAmbiguousInEveryOrder()
{
 for(bool dome:{false,true})for(double separation:{0.0,0.5e-7,2e-7})
 {
  Frame supplied=clippingFrame(dome);
  const std::vector<Target> markers={markerOnRay(supplied,0.5,0.5,4.75,0.25,7),
   markerOnRay(supplied,0.5,0.5,4.75+separation,0.25,8),markerOnRay(supplied,0.5,0.5,9.65,0.35,9)};
  std::vector<unsigned> order={0,1,2};
  do
  {
   supplied.targets={markers[order[0]],markers[order[1]],markers[order[2]]};
   checkClippedShot(supplied,0.5,0.5,separation<1e-7?0:7,separation<1e-7?"ambiguous_target":"hit");
  }while(std::next_permutation(order.begin(),order.end()));
 }
}
void clippedNearestMarkerCannotExposeFartherTarget()
{
 for(bool dome:{false,true})for(bool reverse:{false,true})
 {
  Frame supplied=clippingFrame(dome);supplied.nearPlane=0.2;supplied.farPlane=1000;
  const Target near=markerOnRay(supplied,0.5,0.5,0.1,0.35,7);
  const Target far=markerOnRay(supplied,0.5,0.5,9.65,0.35,8);
  supplied.targets=reverse?std::vector<Target>{far,near}:std::vector<Target>{near,far};
  checkClippedShot(supplied,0.5,0.5,0);
 }
}
void clippingUsesCaptureFaceDepthAcrossProjectionsAndCameraRotations()
{
 for(bool dome:{false,true})for(bool rotated:{false,true})for(double fov:{90.0,120.0})
 {
  // Both UVs produce a 45-degree capture-local ray: max(abs(direction))=sqrt(.5).
  const double u=dome?0.75:0.625;
  for(double contact:{0.3,20.0})
  {
   Frame supplied=clippingFrame(dome);supplied.faceFovDeg=fov;
   if(rotated)
   {
    // Rotate 45 degrees, so the local diagonal is a world cardinal direction.
    // Measuring clip depth in world axes would incorrectly reject the far hit.
    const double c=std::sqrt(0.5);
    supplied.cameraPosition=Vec3(3,2,-5);supplied.cameraRight=Vec3(c,0,-c);supplied.cameraForward=Vec3(c,0,c);
   }
   supplied.targets.push_back(markerOnRay(supplied,u,0.5,contact,0.25,7));
   // .3 is radially beyond near=.25, but face depth=.212... is clipped.
   // 20 is radially beyond far=16, but face depth=14.142... is visible.
   checkClippedShot(supplied,u,0.5,contact>1?7:0);
  }
 }
 // Domemaster tilt also precedes capture-face selection/clipping in GLSL.
 for(double tilt:{-60.0,-30.0,30.0,60.0})for(double contact:{0.26,18.0})
 {
  Frame supplied=clippingFrame(true);supplied.projection.domeTiltDeg=tilt;
  supplied.targets.push_back(markerOnRay(supplied,0.5,0.5,contact,0.25,7));
  checkClippedShot(supplied,0.5,0.5,contact>1?7:0);
 }
}
void clipPlanesAndPositiveFrontSurfaceUseStrictBoundaries()
{
 for(bool dome:{false,true})
 {
  // Binary-exact distances/radii make equality at both clip planes unambiguous.
  for(double contact:{-0.25,0.0,0.125,0.25,0.5,15.75,16.0,16.25})
  {
   Frame supplied=clippingFrame(dome);
   supplied.targets.push_back(markerOnRay(supplied,0.5,0.5,contact,0.25,7));
   checkClippedShot(supplied,0.5,0.5,contact>0.25&&contact<16?7:0);
  }
  // A nonpositive entry surface is ignored, as in GLSL, without substituting
  // the sphere's exit root. The farther positive surface remains selectable.
  for(double contact:{-0.25,0.0})
  {
   Frame supplied=clippingFrame(dome);
   supplied.targets={markerOnRay(supplied,0.5,0.5,contact,0.25,7),markerOnRay(supplied,0.5,0.5,2,0.25,8)};
   checkClippedShot(supplied,0.5,0.5,8);
  }
 }
 // The same strict face planes apply on all six cubemap axes, including seams/poles.
 const double directions[][2]={{0.5,0.5},{0,0.5},{0.75,0.5},{0.25,0.5},{0.5,0},{0.5,1}};
 for(const auto& uv:directions)for(double contact:{0.125,0.5,15.75,16.25})
 {
  Frame supplied=clippingFrame(false);
  supplied.targets.push_back(markerOnRay(supplied,uv[0],uv[1],contact,0.25,7));
  checkClippedShot(supplied,uv[0],uv[1],contact>0.25&&contact<16?7:0);
 }
}
// Production lifetime helpers with current-owner fixtures, independent of GL.
struct LifetimeKart
{
 bool eliminated=false,finished=false;
 bool isEliminated()const{return eliminated;}
 bool hasFinishedRace()const{return finished;}
};
struct LifetimeWorld
{
 std::vector<LifetimeKart*> karts;
 LifetimeKart* player=nullptr;
 unsigned getNumKarts()const{return static_cast<unsigned>(karts.size());}
 LifetimeKart* getKart(int index)const{CHECK(index>=0&&static_cast<unsigned>(index)<karts.size());return karts[index];}
 LifetimeKart* getPlayerKart(unsigned index)const{CHECK(index==0);return player;}
};
struct LifetimeItem
{
 enum Type {BonusBox,Other};
 Type type=BonusBox;bool available=true,usedUp=false;
 Type getType()const{return type;}
 bool isAvailable()const{return available;}
 bool isUsedUp()const{return usedUp;}
};
struct LifetimeItems
{
 std::vector<LifetimeItem*> items;
 unsigned getNumberOfItems()const{return static_cast<unsigned>(items.size());}
 LifetimeItem* getItem(unsigned index)const{CHECK(index<items.size());return items[index];}
};
TargetLifetime::Registered capturedTarget(const void* pointer,uint64_t instance)
{
 TargetLifetime::Registered result;result.pointer=pointer;result.instance=instance;result.present=true;return result;
}
void liveKartIdentityUsesCurrentWorldStateBeforeNextCapture()
{
 for(unsigned reason=0;reason<7;++reason)
 {
  LifetimeKart kart,replacement,other;LifetimeWorld world;world.karts={&kart,&other};
  LifetimeWorld* currentWorld=&world;
  std::map<uint64_t,TargetLifetime::Registered> records;
  records[1]=capturedTarget(&kart,70);records[2]=capturedTarget(&other,80);unsigned lookups=0;
  auto lookup=[&](uint64_t id)->TargetLifetime::Current{++lookups;return TargetLifetime::currentKart(currentWorld,id-1);};
  CHECK(TargetLifetime::isLive(records,1,70,lookup));
  CHECK(TargetLifetime::isLive(records,1,70,lookup)&&lookups==2);
  CHECK(!TargetLifetime::isLive(records,1,69,lookup)&&lookups==2);
  CHECK(!TargetLifetime::isLive(records,99,70,lookup)&&lookups==2);
  // No capture refresh between the recorded identity and this world mutation.
  switch(reason)
  {
   case 0:kart.eliminated=true;break;
   case 1:kart.finished=true;break;
   case 2:world.player=&kart;break;
   case 3:world.karts[0]=&replacement;break;
   case 4:world.karts[0]=nullptr;break;
   case 5:world.karts.clear();break;
   default:currentWorld=nullptr;break;
  }
  CHECK(!TargetLifetime::isLive(records,1,70,lookup)&&lookups==3);
  CHECK(records.count(1)==0&&records.count(2)==1&&records[2].instance==80);
  CHECK(!TargetLifetime::currentKart(&world,uint64_t(1)<<63).eligible);
 }
}
void liveItemIdentityRetiresCollectionTypeChangesAndSlotReuse()
{
 for(unsigned reason=0;reason<7;++reason)
 {
  LifetimeItem item,replacement;LifetimeItems items;items.items={&item};LifetimeItems* currentItems=&items;
  std::map<uint64_t,TargetLifetime::Registered> records;records[100000]=capturedTarget(&item,70);
  unsigned lookups=0;
  auto lookup=[&](uint64_t id)->TargetLifetime::Current{++lookups;return TargetLifetime::currentItem(currentItems,id-100000,LifetimeItem::BonusBox);};
  CHECK(TargetLifetime::isLive(records,100000,70,lookup)&&lookups==1);
  switch(reason)
  {
   case 0:item.available=false;break;
   case 1:item.usedUp=true;break;
   case 2:item.type=LifetimeItem::Other;break;
   case 3:items.items[0]=&replacement;break;
   case 4:items.items[0]=nullptr;break;
   case 5:items.items.clear();break;
   default:currentItems=nullptr;break;
  }
  CHECK(!TargetLifetime::isLive(records,100000,70,lookup)&&lookups==2&&records.empty());
  // Restoring the same pointer/slot cannot resurrect its retired instance.
  currentItems=&items;items.items={&item};item.available=true;item.usedUp=false;item.type=LifetimeItem::BonusBox;
  CHECK(!TargetLifetime::isLive(records,100000,70,lookup)&&lookups==2);
  records[100000]=capturedTarget(&item,71);
  CHECK(!TargetLifetime::isLive(records,100000,70,lookup)&&lookups==2);
  CHECK(records[100000].instance==71);
  CHECK(TargetLifetime::isLive(records,100000,71,lookup)&&lookups==3);
  CHECK(!TargetLifetime::currentItem(&items,uint64_t(1)<<63,LifetimeItem::BonusBox).eligible);
 }
}
void pendingOrCachedDepthCannotConfirmRetiredLiveInstance()
{
 for(bool itemTarget:{false,true})for(bool pending:{false,true})for(bool retire:{false,true})
 {
  LifetimeKart kart;LifetimeWorld world;world.karts={&kart};
  LifetimeItem item;LifetimeItems items;items.items={&item};
  const uint64_t id=itemTarget?100000:1;
  std::map<uint64_t,TargetLifetime::Registered> records;
  records[id]=capturedTarget(itemTarget?static_cast<const void*>(&item):static_cast<const void*>(&kart),70);
  auto lookup=[&](uint64_t target)->TargetLifetime::Current{
   return itemTarget?TargetLifetime::currentItem(&items,target-100000,LifetimeItem::BonusBox):TargetLifetime::currentKart(&world,target-1);
  };
  Runtime r;ready(r);Frame supplied=frame(1);supplied.targets[0].id=id;r.addFrame(supplied);
  bool depthReady=!pending;unsigned queries=0;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return depthReady?Visibility::Visible:Visibility::Pending;});
  r.setLiveTargetOracle([&](uint64_t target,uint64_t instance){return TargetLifetime::isLive(records,target,instance,lookup);});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("retirement-release","released",0),1000000).status==200);
  // A ready/cached visibility result must revalidate too; pending depth cannot
  // preserve authority across the intervening physics update either.
  if(!pending&&retire){item.available=false;kart.eliminated=true;}
  CHECK(r.route("/spatial/input",edge("retirement-shot","pressed",1),1000000).status==200);
  if(pending)
  {
   CHECK(r.events().empty());
   if(retire){item.available=false;kart.eliminated=true;}
   depthReady=true;
   r.tick(1000010,true);
  }
  const std::string result=retire?"target_instance_retired":"hit";
  CHECK(r.events().size()==1&&r.events().back().result==result);
  CHECK(r.events().back().targetId==(retire?0:id)&&r.events().back().targetInstance==(retire?0:70));
  CHECK(r.reticles()[0].feedback.kind==(retire?FeedbackKind::Rejected:FeedbackKind::Hit)&&r.reticles()[0].feedback.reason==result);
  CHECK(queries==(pending?2u:1u)&&records.empty()==retire);
  CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 }
}
// Fake only fence readiness/mapping; production cache maintenance is exercised
// directly, with no graphics context, sleeps, driver or hardware assumption.
struct ReadbackFixture
{
 int64_t lastRequestedUs=1000000,signaledUs=1090000;
 bool complete=false,failed=false;
 unsigned polls=0,maps=0;
 float depth=0;
 bool poll(int64_t now)
 {
  ++polls;
  if(complete)return true;
  if(failed||now<signaledUs)return false;
  complete=true;++maps;depth=0.75f;return true;
 }
};
void readyReadbackSurvivesLateObservationAndIsReadBeforeRetirement()
{
 std::map<unsigned,ReadbackFixture> cache;cache[1]=ReadbackFixture();
 std::vector<ReadbackFixture> retired;
 auto sweep=[&](int64_t now){ReadbackCache::collectIdle(cache,now,
  [&](ReadbackFixture& q){return q.poll(now);},
  [&](ReadbackFixture&& q){retired.push_back(std::move(q));});};
 sweep(1083333);CHECK(cache.size()==1&&!cache.at(1).complete);
 // Original counterexample: signaled at 90 ms, next render at 100.2 ms.
 sweep(1100200);
 CHECK(cache.size()==1&&retired.empty()&&cache.at(1).complete);
 CHECK(cache.at(1).maps==1&&cache.at(1).depth==0.75f);
 sweep(1250000);CHECK(cache.size()==1&&cache.at(1).maps==1);
 sweep(1250001);CHECK(cache.empty()&&retired.size()==1&&retired[0].maps==1);
 // Even an idle entry must be polled before retiring it. The retire callback
 // receives the mapped value, rather than silently throwing away a ready PBO.
 ReadbackFixture late;late.signaledUs=1249999;cache[2]=late;
 sweep(1250001);
 CHECK(cache.empty()&&retired.size()==2&&retired[1].complete&&retired[1].maps==1);
}
void sharedReadbackRetentionUsesLastRequestAndMaintenanceCannotRenewIt()
{
 std::map<unsigned,ReadbackFixture> cache;cache[1]=ReadbackFixture();unsigned retired=0;
 auto sweep=[&](int64_t now){ReadbackCache::collectIdle(cache,now,
  [&](ReadbackFixture& q){return q.poll(now);},[&](ReadbackFixture&&){++retired;});};
 sweep(1100200);CHECK(cache.at(1).complete);
 // Another admitted action reuses this immutable pixel, without a new query.
 cache.at(1).lastRequestedUs=1220000;
 sweep(1250001);CHECK(cache.size()==1&&retired==0&&cache.at(1).maps==1);
 sweep(1470000);CHECK(cache.size()==1&&cache.at(1).lastRequestedUs==1220000);
 sweep(1470001);CHECK(cache.empty()&&retired==1);
}
void idlePendingAndFailedReadbacksRetireWithoutManufacturingAResult()
{
 std::map<unsigned,ReadbackFixture> cache;
 for(unsigned i=0;i<64;++i)
 {
  ReadbackFixture q;q.signaledUs=2000000;q.failed=i%2==0;cache[i]=q;
 }
 unsigned retired=0,failed=0;
 auto sweep=[&](int64_t now){ReadbackCache::collectIdle(cache,now,
  [&](ReadbackFixture& q){return q.poll(now);},[&](ReadbackFixture&& q){
   CHECK(q.polls==2&&!q.complete&&q.maps==0);++retired;if(q.failed)++failed;
  });};
 sweep(1250000);CHECK(cache.size()==64&&retired==0);
 sweep(1250001);CHECK(cache.empty()&&retired==64&&failed==32);
 // Retirement transfers ownership to deferred GPU cleanup; it does not claim
 // that an unsignaled/failed fence is safe to delete or reset its global budget.
}
void delayedReadbackResolvesWithinOriginalShotDeadlineOnly()
{
 for(int64_t delay:{90000,100200,150000,249999,250000,250001})
 for(Visibility outcome:{Visibility::Visible,Visibility::Occluded})
 {
  Runtime r;ready(r);r.addFrame(frame(1));int64_t now=1000000,renderNow=now;
  const int64_t signal=1000000+(delay==100200?90000:delay);
  std::map<unsigned,ReadbackFixture> cache;unsigned issued=0,retired=0;
  r.setVisibilityOracle([&](const Frame& f,const Target&,const Vec3&,double){
   CHECK(f.id==1);
   auto found=cache.find(1);
   if(found==cache.end())
   {
    ReadbackFixture q;q.lastRequestedUs=now;q.signaledUs=signal;
    cache[1]=q;++issued;return Visibility::Pending;
   }
   found->second.lastRequestedUs=now;
   return found->second.poll(renderNow)?outcome:Visibility::Pending;
  });
  r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
  CHECK(r.route("/spatial/presentation",presentation(),now).status==200);
  CHECK(r.route("/spatial/input",edge("readback-release","released",0),now).status==200);
  CHECK(r.route("/spatial/input",edge("readback-shot","pressed",1),now).status==200);
  CHECK(issued==1&&r.events().empty());
  auto maintain=[&](){renderNow=now;ReadbackCache::collectIdle(cache,now,
   [&](ReadbackFixture& q){return q.poll(now);},[&](ReadbackFixture&&){++retired;});};
  // Refresh aim independently. For the 100.2 ms case no render/poll observes
  // the fence between 90 ms and the final render, reproducing the audit trace.
  unsigned aimSequence=0;
  for(int64_t elapsed=50000;elapsed<delay;elapsed+=50000)
  {
   now=1000000+elapsed;std::string aim=feedbackAim(now-1000,0.5,0.5);
   auto proof=presentation();replace(proof,"\"proofUntilUs\":1000000","\"proofUntilUs\":"+std::to_string(now-500));
   replace(proof,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":"+std::to_string(now-500));
   CHECK(r.route("/spatial/presentation",proof,now).status==200);
   const std::string sequence="\"sequence\":"+std::to_string(aimSequence++);
   replace(aim,"\"sequence\":0",sequence);replace(aim,"\"sequence\":0",sequence);
   CHECK(r.route("/spatial/input",aim,now).status==200);
   if(now<signal){maintain();r.tick(now,true);}
   CHECK(r.events().empty()&&issued==1&&retired==0);
  }
  now=1000000+delay;maintain();r.tick(now,true);
  CHECK(cache.size()==1&&cache.at(1).complete&&cache.at(1).maps==1&&issued==1&&retired==0);
  const bool timedOut=delay>PendingTimeoutUs;
  const std::string expected=timedOut?"history_timeout":outcome==Visibility::Visible?"hit":"miss";
  CHECK(r.events().size()==1&&r.events().back().result==expected);
  CHECK(r.events().back().targetId==(!timedOut&&outcome==Visibility::Visible?7u:0u));
  CHECK(r.reticles()[0].feedback.reason==expected&&r.stateJSON().find("\"prepared\":true")!=std::string::npos);
  CHECK(r.stateJSON().find("\"pending\":0")!=std::string::npos);
 }
}
// Depth fixtures encode an independent perspective projection, then quantize
// to the real D24 representation before calling the production visibility rule.
float capturedDepth(double z,double n,double f,int rounding=0)
{
 const double depth=f/(f-n)-f*n/((f-n)*z),maximum=16777215.0;
 const double scaled=std::max(0.0,std::min(1.0,depth))*maximum;
 const double code=rounding<0?std::floor(scaled):rounding>0?std::ceil(scaled):std::floor(scaled+0.5);
 return static_cast<float>(code/maximum);
}
void depthBiasCounterexampleAndUnresolvedIntervalsAreOccluded()
{
 const float n=0.2f,f=1000.0f,wall=capturedDepth(99.95,n,f);
 const double previousExpected=f/(double(f)-n)-double(f)*n/((double(f)-n)*100.0);
 CHECK(double(wall)+0.000002>=previousExpected); // Audit's former false hit.
 CHECK(!DepthVisibility::visible(wall,100.0f,n,f));
 for(int rounding:{-1,0,1})
 {
  CHECK(!DepthVisibility::visible(capturedDepth(100.0,n,f,rounding),100.0f,n,f));
  CHECK(!DepthVisibility::visible(capturedDepth(100.0,n,f,rounding),99.999f,n,f));
  CHECK(DepthVisibility::visible(capturedDepth(100.05,n,f,rounding),100.0f,n,f));
 }
 // Clear depth is not a bypass for the ambiguous last depth bins at far.
 CHECK(!DepthVisibility::visible(1.0f,999.99f,n,f));
 CHECK(DepthVisibility::visible(1.0f,100.0f,n,f));
}
void depthVisibilityRejectsNonfiniteSamplesAndStrictClipBoundaries()
{
 const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 for(float sample:{-1.0f,1.000001f,nan,inf,-inf})CHECK(!DepthVisibility::visible(sample,100.0f,0.2f,1000.0f));
 for(float z:{-1.0f,0.0f,0.2f,1000.0f,1001.0f,nan,inf,-inf})CHECK(!DepthVisibility::visible(1.0f,z,0.2f,1000.0f));
 for(float n:{-1.0f,0.0f,nan,inf,-inf})CHECK(!DepthVisibility::visible(1.0f,100.0f,n,1000.0f));
 for(float f:{-1.0f,0.0f,0.2f,100.0f,nan,inf,-inf})CHECK(!DepthVisibility::visible(1.0f,100.0f,0.2f,f));
 CHECK(!DepthVisibility::visible(0.0f,1.0f,0.2f,1000.0f));
}
void depthVisibilityStaysConservativeAcrossDistanceAndClipRatios()
{
 for(float n:{0.05f,0.2f,1.0f})for(float f:{50.0f,1000.0f,5000.0f})
 for(double fraction:{0.0001,0.001,0.01,0.1,0.5,0.99})
 {
  const double wall=n+(f-n)*fraction;
  for(int rounding:{-1,0,1})
  {
   const float sample=capturedDepth(wall,n,f,rounding);
   for(double separation:{0.0,0.0000001,0.00001,0.0005,0.05})
   {
    const float behind=static_cast<float>(wall*(1.0+separation));
    CHECK(!DepthVisibility::visible(sample,behind,n,f));
   }
   // A sufficiently separated marker in front must remain visible too.
   CHECK(DepthVisibility::visible(sample,static_cast<float>((n+wall)*0.5),n,f));
  }
 }
}
void historicalOcclusionUsesConservativeDepthAcrossViewsAndCameraPoses()
{
 for(bool dome:{false,true})for(bool rotated:{false,true})
 for(double wall:{99.95,100.0,100.001,100.05,1000.0})
 {
  Runtime r;Frame supplied=clippingFrame(dome);supplied.nearPlane=0.2;supplied.farPlane=1000.0;
  if(rotated)
  {
   supplied.cameraPosition=Vec3(3,2,-5);supplied.cameraRight=Vec3(0,0,-1);supplied.cameraForward=Vec3(1,0,0);
  }
  supplied.targets.push_back(markerOnRay(supplied,0.5,0.5,100.0,1.0,7));
  std::string grant=prepare();
  if(dome)replace(grant,"\"kind\":\"equirectangular\",\"width\":2048,\"height\":1024","\"kind\":\"fisheye_domemaster\",\"width\":1536,\"height\":1536");
  CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
  CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\""),1000000).status==200);
  r.addFrame(supplied);const float scene=capturedDepth(wall,0.2f,1000.0f);unsigned queries=0;
  r.setVisibilityOracle([&](const Frame& historical,const Target& target,const Vec3& direction,double distance){
   ++queries;CHECK(historical.id==1&&target.id==7&&std::abs(distance-100.0)<1e-9);
   const auto dot=[](const Vec3& a,const Vec3& b){return a.x*b.x+a.y*b.y+a.z*b.z;};
   const double ma=std::max({std::abs(dot(direction,historical.cameraRight)),std::abs(dot(direction,historical.cameraUp)),std::abs(dot(direction,historical.cameraForward))});
   return DepthVisibility::visible(scene,static_cast<float>(distance*ma),static_cast<float>(historical.nearPlane),static_cast<float>(historical.farPlane))?Visibility::Visible:Visibility::Occluded;
  });
  r.setLiveTargetOracle([](uint64_t id,uint64_t instance){return id==7&&instance==70;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("occlusion-release","released",0),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("occlusion-shot","pressed",1),1000000).status==200);
  const bool hit=wall>=100.05;
  CHECK(queries==1&&r.events().size()==1&&r.events().back().result==(hit?"hit":"miss"));
  CHECK(r.events().back().targetId==(hit?7u:0u)&&r.events().back().frameId==1);
  CHECK(r.reticles()[0].feedback.kind==(hit?FeedbackKind::Hit:FeedbackKind::Miss));
  CHECK(r.stateJSON().find("\"prepared\":true")!=std::string::npos);
 }
}
void combatDeliveryIsOnceOnlyAndPreservesHistoricalWindows()
{
 for(bool mixed:{false,true}) {
  Runtime r;ready(r);Frame f=frame(1);
  ScintillaCombat::Snapshot snapshot;snapshot.target=7;snapshot.instance=700;snapshot.window=9;
  snapshot.phase=ScintillaCombat::Phase::Breached;snapshot.eligible=true;
  f.targets[0].combat=snapshot;r.setCombatContext(true,100,{snapshot});r.addFrame(f);
  if(mixed){f.id=2;f.targets[0].combat.window=10;r.addFrame(f);}
  r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
  r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(mixed),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("combat-release","released",0),1000000).status==200);
  CHECK(r.route("/spatial/input",edge("combat-shot","pressed",1),1000000).status==200);
  CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Pending);
  auto shots=r.drainCombatShots();CHECK(shots.size()==1&&shots[0].admittedTick==100);
  CHECK(shots[0].historicalEligible&&shots[0].historicalOpen==!mixed);
  CHECK(shots[0].historical.instance==700&&shots[0].admittedTargets[0].window==9);
  CHECK(r.drainCombatShots().empty());
  r.combatFeedback(shots[0],FeedbackKind::Rebound,"shield_rebound",1000000);
  CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Rebound);
 }
}
void combatBeamNeedsFreshHistoricalVisibility()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,0,{});r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-release","released",0,999980,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-press","pressed",1,999990,0,"secondary"),1000000).status==200);
 auto proofs=r.beamProofs(1000000);CHECK(proofs.size()==1&&proofs[0].held&&proofs[0].visible&&proofs[0].instance==700);
 CHECK(!r.beamProofs(1061000)[0].visible);
 CHECK(r.route("/spatial/input",edgeAt("tool-off","released",2,1000000,0,"secondary"),1000010).status==200);
 CHECK(!r.beamProofs(1000010)[0].held);
}
// Exercise the actual input resolver and pure authoritative core together;
// several terminal aim results deliberately occur between two combat updates.
void combatBeamInterruptionSurvivesPositiveBeforeGameplay()
{
 for(bool breached:{false,true})for(unsigned loss=0;loss<4;++loss){
  Runtime r;ready(r);Frame f=frame(1);f.targets[0].radius=0.3;
  f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;
  Target other=f.targets[0];other.id=8;other.instance=80;other.center=Vec3(3,0,10);other.combat.instance=800;
  f.targets.push_back(other);r.addFrame(f);r.setCombatContext(true,0,{});
  Visibility visibility=Visibility::Visible;bool live=true;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return visibility;});
  r.setLiveTargetOracle([&](uint64_t,uint64_t){return live;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("tool-release","released",0,999970,0,"secondary"),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("tool-press","pressed",1,999980,0,"secondary"),1000000).status==200);
  const auto beams=[&](){std::vector<ScintillaCombat::Beam> out;
   for(const auto& p:r.beamProofs(1000000)){ScintillaCombat::Beam b;b.participant=p.participantId;
    b.target=p.target;b.instance=p.instance;b.controlEpoch=p.controlEpoch;b.held=p.held;b.visible=p.visible;out.push_back(b);}return out;};
  ScintillaCombat::Runtime combat;CHECK(combat.prepare(1,1,{"person1","B","C"},120));combat.observeTarget(7,700,true);
  const int last=breached?72:20;
  for(int tick=0;tick<=last;++tick)combat.update(tick,true,beams());
  CHECK(combat.snapshot(7).phase==(breached?ScintillaCombat::Phase::Breached:ScintillaCombat::Phase::Acquiring));
  ScintillaCombat::Shot shot;shot.race=shot.inputEpoch=1;shot.serial=1;shot.admitted=last;
  shot.target=7;shot.instance=700;shot.window=combat.snapshot(7).window;shot.participant="B";shot.event="first";
  shot.historicalEligible=shot.historicalOpen=shot.admittedOpen=true;
  if(breached){CHECK(combat.shoot(shot).contributed);CHECK(combat.snapshot(7).count==1);}
  auto late=shot;late.serial=2;late.participant="C";late.event="pending-before-loss";
  const auto epoch=r.beamProofs(1000000)[0].controlEpoch;
  live=loss!=3;visibility=loss==0?Visibility::Occluded:loss==2?Visibility::Unavailable:Visibility::Visible;
  CHECK(r.route("/spatial/input",feedbackAim(999985,loss==1?0.5463868:0.5,0.5,10),1000000).status==200);
  r.tick(1000000,true); // Resolved miss, changed target, unavailable depth or retired instance.
  live=true;visibility=Visibility::Visible;
  CHECK(r.route("/spatial/input",feedbackAim(999990,0.5,0.5,11),1000000).status==200);r.tick(1000000,true);
  // Read twice: inspection is not an acknowledgement that can erase the loss.
  for(unsigned read=0;read<2;++read){const auto proof=r.beamProofs(1000000)[0];
   CHECK(proof.held&&!proof.visible&&proof.target==7&&proof.controlEpoch==epoch);}
  combat.update(last+1,true,beams());const auto closed=combat.snapshot(7);
  CHECK(closed.phase==ScintillaCombat::Phase::Shielded&&closed.count==0&&closed.contributors==0&&closed.progress==0);
  const auto rejected=combat.shoot(late);CHECK(!rejected.contributed&&!rejected.requestEffect);
  combat.update(last+2,true,beams());CHECK(combat.snapshot(7).phase==ScintillaCombat::Phase::Shielded);
  // Release/press can occur entirely between authoritative ticks as well.
  CHECK(r.route("/spatial/input",edgeAt("new-release","released",2,999991,0,"secondary"),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("new-press","pressed",3,999992,0,"secondary"),1000000).status==200);
  CHECK(r.beamProofs(1000000)[0].visible&&r.beamProofs(1000000)[0].controlEpoch!=epoch);
  combat.update(last+3,true,beams());CHECK(combat.snapshot(7).phase==ScintillaCombat::Phase::Acquiring);
  CHECK(combat.snapshot(7).window!=shot.window&&combat.snapshot(7).progress==0);
 }
}
void combatBeamInitialMissAndContinuousProofsDoNotInventALoss()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,0,{});Visibility visibility=Visibility::Occluded;
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return visibility;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("press","pressed",1,999980,0,"secondary"),1000000).status==200);
 CHECK(!r.beamProofs(1000000)[0].visible);visibility=Visibility::Visible;
 for(unsigned i=0;i<8;++i){
  CHECK(r.route("/spatial/input",feedbackAim(999981+i,0.5,0.5,i),1000000).status==200);r.tick(1000000,true);
  CHECK(r.beamProofs(1000000)[0].visible);
 }
}
void combatBeamLossIsIsolatedToItsSource()
{
 Runtime r;std::string grant=prepare();
 replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");
 replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]",
  "\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 const auto tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
 CHECK(r.route("/spatial/input",tracked,1000000).status==200);CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);r.setCombatContext(true,0,{});
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 for(unsigned source=0;source<2;++source){
  auto release=edgeAt("release"+std::to_string(source),"released",0,999970,0,"secondary");
  auto press=edgeAt("press"+std::to_string(source),"pressed",1,999980,0,"secondary");
  CHECK(r.route("/spatial/input",source?secondSource(release):release,1000000).status==200);
  CHECK(r.route("/spatial/input",source?secondSource(press):press,1000000).status==200);
 }
 CHECK(r.route("/spatial/input",feedbackAim(999985,0.75,0.5,10),1000000).status==200);r.tick(1000000,true);
 CHECK(r.route("/spatial/input",feedbackAim(999990,0.5,0.5,11),1000000).status==200);r.tick(1000000,true);
 const auto proofs=r.beamProofs(1000000);CHECK(proofs.size()==2&&proofs[0].held&&!proofs[0].visible&&proofs[1].visible);
}
void combatBeamOldEpochFailureCannotPoisonANewPress()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,0,{});Visibility visibility=Visibility::Visible;
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return visibility;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("press","pressed",1,999980,0,"secondary"),1000000).status==200);
 visibility=Visibility::Pending;
 CHECK(r.route("/spatial/input",feedbackAim(999985,0.5,0.5,10),1000000).status==200);r.tick(1000000,true);
 CHECK(r.route("/spatial/input",edgeAt("new-release","released",2,999986,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new-press","pressed",3,999987,0,"secondary"),1000000).status==200);
 CHECK(!r.beamProofs(1000000)[0].visible); // Old positive cannot seed the new press.
 visibility=Visibility::Occluded;r.tick(1000000,true); // Terminal old aim is ignored by epoch.
 visibility=Visibility::Visible;r.tick(1000000,true); // New epoch's queued aim can now resolve.
 CHECK(r.beamProofs(1000000)[0].held&&r.beamProofs(1000000)[0].visible);
}
void combatToolFeedbackIsPersistentIndependentAndEpochChecked()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,0,{});r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-press","pressed",1,999980,0,"secondary"),1000000).status==200);
 ScintillaCombat::ToolSnapshot state;state.participant="person1";state.controlEpoch=r.reticles()[0].toolEpoch;
 state.target=7;state.status=ScintillaCombat::ToolStatus::TargetBusy;state.owner=1;r.setCombatToolFeedback({state});
 CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::TargetBusy);
 CHECK(r.stateJSON().find("\"reason\":\"target_busy\"")!=std::string::npos);
 state.status=ScintillaCombat::ToolStatus::ReleaseRequired;state.owner=-1;r.setCombatToolFeedback({state});
 CHECK(r.route("/spatial/input",edgeAt("primary-release","released",2,999985),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("primary-press","pressed",3,999990),1000000).status==200);
 auto shot=r.drainCombatShots();CHECK(shot.size()==1);r.combatFeedback(shot[0],FeedbackKind::Contribution,"contribution",1000000);
 CHECK(r.reticles()[0].feedback.kind==FeedbackKind::Contribution);
 CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::ReleaseRequired);
 // The tool helper has no timer: expiry of transient shot feedback cannot hide it.
 Reticle late=r.reticles()[0];CHECK(Runtime::feedbackForRendering(late,2000000).kind==FeedbackKind::None);
 CHECK(Runtime::toolFeedbackForRendering(late).status==ScintillaCombat::ToolStatus::ReleaseRequired);
 CHECK(r.stateJSON().find("Release the tool button, then press it again.")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("release-new","released",4,999991,0,"secondary"),1000000).status==200);
 CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::None);
 CHECK(r.route("/spatial/input",edgeAt("press-new","pressed",5,999992,0,"secondary"),1000000).status==200);
 r.setCombatToolFeedback({state});CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::None);
 state.controlEpoch=r.reticles()[0].toolEpoch;r.setCombatToolFeedback({state});
 r.setCombatContext(false,0,{});CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::None);
}
void combatToolInterruptionInstructionCannotBeHiddenByAStaleCoreView()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,0,{});r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("press","pressed",1,999980,0,"secondary"),1000000).status==200);
 ScintillaCombat::ToolSnapshot stale;stale.participant="person1";stale.controlEpoch=r.reticles()[0].toolEpoch;
 CHECK(r.route("/spatial/input",feedbackAim(999985,0.75,0.5,10),1000000).status==200);r.tick(1000000,true);
 CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::ReleaseRequired);
 CHECK(r.route("/spatial/input",feedbackAim(999990,0.5,0.5,11),1000000).status==200);r.tick(1000000,true);
 for(auto state:{ScintillaCombat::ToolStatus::None,ScintillaCombat::ToolStatus::TargetBusy}){
  stale.status=state;r.setCombatToolFeedback({stale});
  CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::ReleaseRequired);
 }
 CHECK(r.stateJSON().find("\"reason\":\"tool_release_required\"")!=std::string::npos);
 r.tick(1000000,false);CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::None);
 r.tick(1000000,true);r.setCombatToolFeedback({stale});
 CHECK(Runtime::toolFeedbackForRendering(r.reticles()[0]).status==ScintillaCombat::ToolStatus::None);
}
void combatCancellationSettlesReliableDelivery()
{
 Runtime r;ready(r);r.addFrame(frame(1));r.setCombatContext(true,0,{});
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Pending;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("pending-release","released",0),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("pending-combat","pressed",1),1000000).status==200);
 CHECK(r.drainCombatShots().empty());
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),1000000).status==200);
 const auto shots=r.drainCombatShots();CHECK(shots.size()==1&&shots[0].result!="hit"&&shots[0].result!="pending");
}
void combatGpuCompletionCannotReorderReliableAdmissions()
{
 Runtime r;std::string grant=prepare();
 replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");
 replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]",
  "\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 auto tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
 CHECK(r.route("/spatial/input",tracked,1000000).status==200);
 CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 r.setCombatContext(true,10,{});Frame f=frame(1);r.addFrame(f);
 bool firstReady=false;unsigned leases=0;
 r.setQueryLeaseOracle([&](uint64_t){++leases;return std::shared_ptr<void>(new unsigned(1),[&](void* p){--leases;delete static_cast<unsigned*>(p);});});
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3& direction,double){return direction.x>0.02||firstReady?Visibility::Visible:Visibility::Pending;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("first-release","released",0),1000000).status==200);
 CHECK(r.route("/spatial/input",secondSource(edge("second-release","released",0)),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("first-press","pressed",1),1000000).status==200);
 auto second=secondSource(edge("second-press","pressed",1));replace(second,"\"u\":0.5","\"u\":0.51");
 CHECK(r.route("/spatial/input",second,1000000).status==200);
 CHECK(r.events().size()==1&&r.events()[0].eventId=="second-press"&&r.drainCombatShots().empty()&&leases==1);
 firstReady=true;r.tick(1000010,true);const auto results=r.drainCombatShots();
 CHECK(results.size()==2&&results[0].eventId=="first-press"&&results[1].eventId=="second-press");
 CHECK(results[0].serial<results[1].serial&&leases==0);
}
void combatPrepareValidationRejectsUnknownOrStaleFields()
{
 Runtime r;ready(r);
 const std::string request="{\"contract\":\"tholus.stk-scintilla-combat/1\",\"profile\":\"crew_standard_sc2_v1\",\"expectedRaceEpoch\":\"1\",\"expectedInputSessionId\":\"session\",\"expectedRosterRevision\":\"2\"}";
 CHECK(r.validateCombatPrepare(request,"1","2").status==200);
 CHECK(r.validateCombatPrepare(request,"2","2").status==409);
 auto unknown=request;unknown.insert(1,"\"targetId\":7,");CHECK(r.validateCombatPrepare(unknown,"1","2").status==400);
 auto duplicate=request;duplicate.insert(1,"\"profile\":\"crew_standard_sc2_v1\",");CHECK(r.validateCombatPrepare(duplicate,"1","2").status==400);
}
void combatIntentIsStrictAndIndependentOfAnInputGrant()
{
 Runtime r;bool required=false;
 const std::string request="{\"contract\":\"tholus.stk-scintilla-combat/1\",\"expectedRaceEpoch\":\"1\",\"expectedIntentRevision\":\"3\",\"required\":true}";
 CHECK(!r.prepared());CHECK(r.validateCombatIntent(request,"1","3",&required).status==200&&required);
 auto off=request;replace(off,"true","false");CHECK(r.validateCombatIntent(off,"1","3",&required).status==200&&!required);
 CHECK(r.validateCombatIntent(request,"2","3",&required).status==409&&!required);
 CHECK(r.validateCombatIntent(request,"1","4",&required).status==409&&!required);
 for(const std::string value:{"1","\"true\"","null","[]","{}"}){
  auto malformed=request;replace(malformed,"true",value);
  CHECK(r.validateCombatIntent(malformed,"1","3",&required).status==400&&!required);
 }
 auto missing=request;replace(missing,",\"required\":true","");
 CHECK(r.validateCombatIntent(missing,"1","3",&required).status==400&&!required);
 auto numeric=request;replace(numeric,"\"expectedRaceEpoch\":\"1\"","\"expectedRaceEpoch\":1");
 CHECK(r.validateCombatIntent(numeric,"1","3",&required).status==400&&!required);
 for(const std::string field:{"\"duration\":1,","\"required\":false,","\"expectedRaceEpoch\":\"1\","}){
  auto malformed=request;malformed.insert(1,field);
  CHECK(r.validateCombatIntent(malformed,"1","3",&required).status==400&&!required);
 }
 auto contract=request;replace(contract,"tholus.stk-scintilla-combat/1","tholus.stk-scintilla-combat/2");
 CHECK(r.validateCombatIntent(contract,"1","3",&required).status==400&&!required);
 CHECK(r.validateCombatIntent(request,"1","3",nullptr).status==400);
 auto absent=request;replace(absent,",\"expectedIntentRevision\":\"3\"","");
 CHECK(r.validateCombatIntent(absent,"1","3",&required).status==400&&!required);
 CHECK(!r.prepared());
}
void pressureCannotEvictAnOwnedOrUnreadGpuResult()
{
 std::map<unsigned,ReadbackFixture> cache;std::set<unsigned> liveOwners={1};
 for(unsigned i=1;i<=3;++i){ReadbackFixture q;q.signaledUs=i==3?2000000:1090000;q.lastRequestedUs=1000000+i;cache[i]=q;}
 unsigned disposed=0;
 const auto reclaim=[&](){return ReadbackCache::reclaimCompletedUnowned(cache,
  [&](ReadbackFixture& q){return q.poll(1100200);},
  [&](const ReadbackFixture& q){return liveOwners.count(unsigned(q.lastRequestedUs-1000000))!=0;},
  [&](ReadbackFixture& q){CHECK(q.complete&&q.maps==1);++disposed;});};
 CHECK(reclaim()&&cache.count(1)&&!cache.count(2)&&cache.count(3)&&disposed==1);
 CHECK(!reclaim()&&cache.size()==2&&cache.at(1).maps==1&&cache.at(3).maps==0);
 liveOwners.clear();CHECK(reclaim()&&!cache.count(1)&&cache.count(3)&&disposed==2);
 CHECK(!reclaim());
}
void combatBeamCannotRegressOnAnOlderReliablePress()
{
 Runtime r;ready(r);Frame f=frame(1);f.targets[0].radius=0.3;
 f.targets[0].combat.instance=700;f.targets[0].combat.eligible=true;
 Target other=f.targets[0];other.id=8;other.instance=80;other.center=Vec3(3,0,10);other.combat.instance=800;f.targets.push_back(other);
 r.addFrame(f);r.setCombatContext(true,0,{});
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("tool-press","pressed",1,999980,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(999995,0.5463868,0.5,10),1000000).status==200);r.tick(1000000,true);
 CHECK(r.beamProofs(1000000)[0].target==8);
 CHECK(r.route("/spatial/input",edgeAt("primary-release","released",2,999980),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("older-primary","pressed",3,999990),1000000).status==200);
 CHECK(r.events().back().targetId==7&&r.beamProofs(1000000)[0].target==8);
}
void defenseHUDNeverFallsThroughToWorld()
{
 for(unsigned area=0;area<3;++area){Runtime r;ready(r);Frame f=frame(1);f.defense.enabled=true;f.defense.threat=9;f.layout=ScintillaDefense::Layout::create(2048,1024);r.addFrame(f);r.setCombatContext(true,0,{});r.setDefenseContext(true,{});
  unsigned queries=0;r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return Visibility::Visible;});r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970),1000000).status==200);
  auto press=edgeAt("press","pressed",1,999980);const double y=area==0?.925:area==1?.24:.72;
  replace(press,"\"v\":0.5","\"v\":"+std::to_string(y));CHECK(r.route("/spatial/input",press,1000000).status==200);CHECK(r.events().back().result=="non_targetable_surface");CHECK(queries==0);CHECK(r.drainCombatShots().back().result=="non_targetable_surface");
 }
}
void defenseToolExcludesOffensiveBeamAndLatchesContextLoss()
{
 Runtime r;ready(r);Frame f=frame(1);f.defense.enabled=true;f.defense.threat=9;f.layout=ScintillaDefense::Layout::create(2048,1024);r.addFrame(f);r.setCombatContext(true,3,{});r.setDefenseContext(true,{});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970,0,"secondary"),1000000).status==200);
 auto press=edgeAt("press","pressed",1,999980,0,"secondary");replace(press,"\"v\":0.5","\"v\":0.925");CHECK(r.route("/spatial/input",press,1000000).status==200);
 CHECK(!r.beamProofs(1000000)[0].held);auto beam=r.defenseBeams(1000000)[0];CHECK(beam.held&&beam.visible&&beam.threat==9&&beam.context==ScintillaDefense::Context::ServiceBoost&&beam.admitted==3);
 CHECK(r.route("/spatial/input",feedbackAim(999985,.5,.24,10),1000000).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(999990,.5,.925,11),1000000).status==200);
 CHECK(!r.defenseBeams(1000000)[0].visible);CHECK(!r.defenseBeams(1000000)[0].visible);
 CHECK(r.route("/spatial/input",edgeAt("release2","released",2,999991,0,"secondary"),1000000).status==200);
 press=edgeAt("press2","pressed",3,999992,0,"secondary");replace(press,"\"v\":0.5","\"v\":0.925");CHECK(r.route("/spatial/input",press,1000000).status==200);CHECK(r.defenseBeams(1000000)[0].visible);
}
void defenseRejectsAmbiguousHistoricalLayout()
{
 Runtime r;ready(r);Frame a=frame(1),b=frame(2);a.defense.enabled=b.defense.enabled=true;a.defense.threat=9;b.defense.threat=10;a.layout=b.layout=ScintillaDefense::Layout::create(2048,1024);r.addFrame(a);r.addFrame(b);r.setCombatContext(true,0,{});r.setDefenseContext(true,{});
 CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970),1000000).status==200);
 auto press=edge("ambiguous","pressed",1);replace(press,"\"v\":0.5","\"v\":0.925");CHECK(r.route("/spatial/input",press,1000000).status==200);CHECK(r.events().back().result=="ambiguous_layout_context");
}
void defenseWarningDoesNotInterruptWorldOffense()
{
 Runtime r;ready(r);Frame f=frame(1);f.defense.enabled=true;f.layout=ScintillaDefense::Layout::create(2048,1024);
 f.targets[0].combat.eligible=true;f.targets[0].combat.instance=70;r.addFrame(f);
 r.setCombatContext(true,3,{});r.setDefenseContext(true,{});
 bool depthReady=true;
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return depthReady?Visibility::Visible:Visibility::Pending;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("world-release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("world-press","pressed",1,999980,0,"secondary"),1000000).status==200);
 const auto epoch=r.beamProofs(1000000)[0].controlEpoch;CHECK(r.beamProofs(1000000)[0].visible);
 for(uint64_t threat:{1u,2u}){
  f.id=threat+1;f.defense.threat=threat;f.defense.phase=ScintillaDefense::Phase::AwaitWarning;r.addFrame(f);
  auto proof=presentation();replace(proof,"\"frameId\":1","\"frameId\":"+std::to_string(f.id));
  const int64_t from=1000000+int64_t(threat)*1000,until=from+999,now=until+5;
  replace(proof,"\"proofFromUs\":999000","\"proofFromUs\":"+std::to_string(from));replace(proof,"\"proofUntilUs\":1000000","\"proofUntilUs\":"+std::to_string(until));
  replace(proof,"\"presentedFromUs\":999000","\"presentedFromUs\":"+std::to_string(from));replace(proof,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":"+std::to_string(until));
  CHECK(r.route("/spatial/presentation",proof,now).status==200);
  CHECK(r.route("/spatial/input",feedbackAim(from+100,.5,.5,10+threat),now).status==200);
  CHECK(r.beamProofs(now)[0].visible&&r.beamProofs(now)[0].controlEpoch==epoch&&r.beamProofs(now)[0].target==7);
 }
 // Even with an older GPU aim still pending, a known HUD departure cannot
 // disappear when the newest world aim coalesces the queued HUD sample.
 depthReady=false;CHECK(r.route("/spatial/input",feedbackAim(1002150,.5,.5,19),1003004).status==200);r.tick(1003004,true);
 CHECK(r.beamProofs(1003004)[0].visible);
 // Same epoch changing to the HUD still latches release, even on returning.
 auto hud=feedbackAim(1002200,.5,.925,20);CHECK(r.route("/spatial/input",hud,1003004).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(1002300,.5,.5,21),1003004).status==200);
 CHECK(!r.beamProofs(1003004)[0].visible);
 depthReady=true;r.tick(1003004,true);CHECK(!r.beamProofs(1003004)[0].visible);
 // The same transition must survive even if the initial reliable press itself
 // has not finished GPU validation yet. No positive proof is granted early.
 Runtime pending;ready(pending);Frame initial=frame(1);initial.defense.enabled=true;initial.layout=f.layout;
 initial.targets[0].combat.eligible=true;initial.targets[0].combat.instance=70;pending.addFrame(initial);
 pending.setCombatContext(true,3,{});pending.setDefenseContext(true,{});depthReady=false;
 pending.setLiveTargetOracle([](uint64_t,uint64_t){return true;});pending.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return depthReady?Visibility::Visible:Visibility::Pending;});
 CHECK(pending.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(pending.route("/spatial/input",edgeAt("pending-release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(pending.route("/spatial/input",edgeAt("pending-press","pressed",1,999980,0,"secondary"),1000000).status==200);
 CHECK(pending.events().empty()&&!pending.beamProofs(1000000)[0].visible);
 CHECK(pending.route("/spatial/input",feedbackAim(999985,.5,.925,10),1000000).status==200);
 CHECK(pending.route("/spatial/input",feedbackAim(999990,.5,.5,11),1000000).status==200);
 depthReady=true;pending.tick(1000000,true);CHECK(!pending.beamProofs(1000000)[0].visible);
}
void defenseWarningBoundaryKeepsWorldContextUnambiguous()
{
 Runtime r;ready(r);Frame a=frame(1),b=frame(2);a.defense.enabled=b.defense.enabled=true;
 a.layout=b.layout=ScintillaDefense::Layout::create(2048,1024);b.defense.threat=1;b.defense.phase=ScintillaDefense::Phase::AwaitWarning;
 a.targets[0].combat.eligible=b.targets[0].combat.eligible=true;a.targets[0].combat.instance=b.targets[0].combat.instance=70;
 r.addFrame(a);r.addFrame(b);r.setCombatContext(true,3,{});r.setDefenseContext(true,{});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("boundary-release","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("boundary-press","pressed",1,999990,1,"secondary"),1000000).status==200);
 CHECK(r.beamProofs(1000000)[0].visible);CHECK(r.events().back().result=="hit");
}
void defenseProtocolClosedAndEpochChecked()
{
 Runtime r;bool required=false;const std::string intent="{\"contract\":\"tholus.stk-scintilla-defense/1\",\"raceEpoch\":\"1\",\"intentRevision\":\"0\",\"required\":true}";
 CHECK(r.validateDefenseIntent(intent,"1","0",&required).status==200&&required);CHECK(r.validateDefenseIntent(intent,"2","0",&required).status==409);auto malformed=intent;malformed.insert(1,"\"target\":1,");CHECK(r.validateDefenseIntent(malformed,"1","0",&required).status==400);
 ready(r);DefenseAdmission a;const std::string prepareDefense="{\"contract\":\"tholus.stk-scintilla-defense/1\",\"profile\":\"sc3_sector_rear_readonly_v1\",\"inputSessionID\":\"session\",\"inputEpoch\":\"1\",\"raceEpoch\":\"1\",\"rosterRevision\":\"2\",\"combatProfile\":\"crew_standard_sc2_v1\",\"layoutProfile\":\"front_rear_readonly_v1\",\"layoutRevision\":\"1\",\"routeRevision\":\"route\",\"projectionContextId\":\"projection\",\"visibilityEvidenceID\":\"measured-profile\",\"visibleRegions\":[{\"left\":0,\"top\":0,\"right\":1,\"bottom\":1}]}";
 CHECK(r.validateDefensePrepare(prepareDefense,"1","2",&a).status==200&&a.regions.size()==4);CHECK(r.validateDefensePrepare(prepareDefense,"2","2",&a).status==409);
 malformed=prepareDefense;replace(malformed,"\"right\":1","\"right\":0");CHECK(r.validateDefensePrepare(malformed,"1","2",&a).status==400);
 malformed=prepareDefense;replace(malformed,"\"inputEpoch\":\"1\"","\"inputEpoch\":\"2\"");CHECK(r.validateDefensePrepare(malformed,"1","2",&a).status==409);
}
void defensePresentationIsProducerOwnedAndUnavailableAfterReset()
{
 Runtime r;ready(r);Frame f=frame(1);f.defense.enabled=true;f.defense.threat=9;f.defense.phase=ScintillaDefense::Phase::AwaitWarning;f.layout=ScintillaDefense::Layout::create(2048,1024);r.addFrame(f);
 CHECK(r.presentedDefenseFrames(1000000).empty());CHECK(r.route("/spatial/presentation",presentation(),1000010).status==200);auto frames=r.presentedDefenseFrames(1000010);CHECK(frames.size()==1&&frames[0].frame.defense.threat==9&&frames[0].high<=1000010);
 CHECK(r.presentedDefenseFrames(1200000).empty());r.reset("test");CHECK(r.presentedDefenseFrames(1000000).empty());CHECK(!r.defenseVideoReady(1000000));
}


std::string participantRebind(const std::string& epoch="1",const std::string& person="person2")
{return "{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expectedInputEpoch\":\""+epoch+"\",\"expectedRosterRevision\":\"2\",\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\""+person+"\"}]}";}
void defenseParticipantRebindIsAtomicAndIdempotent()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});const auto epoch=r.inputEpoch();CHECK(epoch==1);
 CHECK(r.route("/spatial/input",edgeAt("old-release","released",0,999980,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("old-press","pressed",1,999990,0,"secondary"),1000000).status==200);
 r.setParticipants({"person2"});CHECK(!r.prepared());CHECK(!r.reticles()[0].toolHeld);CHECK(r.drainCombatShots().empty());
 CHECK(r.route("/spatial/input",feedbackAim(1000000,.5,.5),1000000).json.find("participant_rebind_required")!=std::string::npos);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":1,\"state\":\"tracked\",\"reason\":\"still_same_physical_source\""),1000000).status==200);
 CHECK(r.rebindParticipants(participantRebind(),"3",1000000).status==409);CHECK(r.inputEpoch()==epoch&&!r.prepared());
 CHECK(r.rebindParticipants(participantRebind(),"2",1000000).status==200);CHECK(r.inputEpoch()==epoch+1&&r.prepared());CHECK(r.participants()==std::vector<std::string>({"person2"}));
 CHECK(r.rebindParticipants(participantRebind(),"2",1000000).status==200);CHECK(r.inputEpoch()==epoch+1);CHECK(r.rebindParticipants(participantRebind("1","person1"),"2",1000000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("old-press","pressed",1,999990,0,"secondary"),1000000).json.find("duplicate_event")!=std::string::npos);
 r.tick(1000002,true);CHECK(r.route("/spatial/input",edgeAt("held-cannot-rearm","pressed",2,1000010,0,"secondary"),1000020).status==409);
 CHECK(r.route("/spatial/input",edgeAt("new-release","released",3,1000030,0,"secondary"),1000040).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new-press","pressed",4,1000050,0,"secondary"),1000060).status==200);
}
void defenseParticipantRebindRejectsPhysicalChanges()
{
 Runtime r;ready(r);r.setParticipants({"person1"});r.setParticipants({"person2"});CHECK(!r.prepared());CHECK(r.rebindParticipants(participantRebind(),"2",1000000).status==409);
 Runtime q;ready(q);q.allowParticipantRebind(true);q.setParticipants({"person1"});q.setParticipants({"person2"});auto changed=participantRebind();replace(changed,"\"sourceId\":\"s1\"","\"sourceId\":\"other\"");CHECK(q.rebindParticipants(changed,"2",1000000).status==400);CHECK(!q.prepared()&&q.inputEpoch()==1);
 q.setParticipants({"person2","person3"});CHECK(q.rebindParticipants(participantRebind(),"2",1000000).status==409);CHECK(!q.prepared());
}


void defenseTwoSourcesMovingFramesPreserveIndependentContexts()
{
 Runtime r;auto grant=prepare();replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]","\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);auto tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");CHECK(r.route("/spatial/input",tracked,1000000).status==200);CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 Frame a=frame(1),b=frame(2,1);b.cameraPosition=Vec3(1,0,0);a.defense.enabled=b.defense.enabled=true;a.defense.threat=b.defense.threat=9;a.defense.phase=b.defense.phase=ScintillaDefense::Phase::Telegraph;a.layout=b.layout=ScintillaDefense::Layout::create(2048,1024);a.targets[0].combat.instance=b.targets[0].combat.instance=70;a.targets[0].combat.eligible=b.targets[0].combat.eligible=true;
 r.addFrame(a);r.addFrame(b);r.setCombatContext(true,3,{});r.setDefenseContext(true,{});r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
 for(int i=0;i<2;++i){auto off=edgeAt("off"+std::to_string(i),"released",0,999970,0,"secondary");CHECK(r.route("/spatial/input",i?secondSource(off):off,1000000).status==200);auto on=edgeAt("on"+std::to_string(i),"pressed",1,i?999995:999985,0,"secondary");if(!i){replace(on,"\"u\":0.5","\"u\":0.45");replace(on,"\"v\":0.5","\"v\":0.81");}CHECK(r.route("/spatial/input",i?secondSource(on):on,1000000).status==200);}
 CHECK(r.defenseBeams(1000000)[0].visible);CHECK(r.defenseBeams(1000000)[0].sector==ScintillaDefense::Sector::Front);CHECK(!r.beamProofs(1000000)[0].held);CHECK(r.beamProofs(1000000)[1].visible);
 CHECK(r.route("/spatial/input",feedbackAim(999996,.5,.24,10),1000000).status==200);CHECK(r.route("/spatial/input",feedbackAim(999997,.45,.81,11),1000000).status==200);CHECK(!r.defenseBeams(1000000)[0].visible);CHECK(r.beamProofs(1000000)[1].visible);
}

std::string racePresentation(uint64_t id,int64_t from,int64_t until)
{
 auto proof=presentation();replace(proof,"\"frameId\":1","\"frameId\":"+std::to_string(id));
 replace(proof,"\"proofFromUs\":999000","\"proofFromUs\":"+std::to_string(from));replace(proof,"\"proofUntilUs\":1000000","\"proofUntilUs\":"+std::to_string(until));
 replace(proof,"\"presentedFromUs\":999000","\"presentedFromUs\":"+std::to_string(from));replace(proof,"\"presentedUntilUs\":1000000","\"presentedUntilUs\":"+std::to_string(until));return proof;
}
void defenseRaceResetReusesGrantAndRearmsNewRace()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});Frame f=frame(1);r.addFrame(f);r.setCombatContext(true,3,{},1);
 bool gpu=false;r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return gpu?Visibility::Visible:Visibility::Pending;});r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("old-release","released",0,999970),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("old-shot","pressed",1,999980),1000000).status==200);
 const auto epoch=r.inputEpoch();CHECK(r.resetForRace());CHECK(!r.prepared());CHECK(r.inputSessionID()=="session");CHECK(r.inputEpoch()==epoch);
 CHECK(r.drainCombatShots().empty());CHECK(!r.reticles()[0].primaryHeld);CHECK(!r.defenseVideoReady(1000001));
 // An already-in-flight old optical proof is unavailable, not grant revocation.
 const auto oldBatch="{\"contract\":\"tholus.stk-spatial/1\",\"inputs\":[],\"presentation\":"+presentation()+"}";
 CHECK(r.route("/spatial/update",oldBatch,1000001).status==200);
 f.id=2;f.generation=2;f.captureUs=1000010;f.targets[0].instance=71;r.addFrame(f);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000020,1000900),1001000).status==200);
 auto binding=participantRebind(std::to_string(epoch),"person1");CHECK(r.rebindParticipants(binding,"2",1001000).status==200);
 CHECK(r.prepared()&&r.inputEpoch()==epoch+1);CHECK(r.rebindParticipants(binding,"2",1001000).status==200);CHECK(r.inputEpoch()==epoch+1);
 r.setCombatContext(true,0,{},2);gpu=true;r.tick(1002000,true);
 CHECK(r.drainCombatShots().empty());
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000020,1002900),1003000).status==200);
 // The old event and sequence cannot be replayed after resetting the race.
 CHECK(r.route("/spatial/input",edgeAt("old-shot","pressed",1,999980),1003000).status==409);
 CHECK(r.route("/spatial/input",edgeAt("new-release","released",2,1002200),1003000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new-shot","pressed",3,1002300),1003000).status==409); // fire-rate guard retained
 CHECK(r.route("/spatial/input",edgeAt("new-tool-release","released",4,1002400,0,"secondary"),1003000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("new-tool","pressed",5,1002500,0,"secondary"),1003000).status==200);
 CHECK(r.events().back().result=="hit"&&r.events().back().targetInstance==71);
 CHECK(r.resetForRace());f.id=3;f.generation=3;f.captureUs=1003010;r.addFrame(f);
 CHECK(r.route("/spatial/presentation",racePresentation(3,1003020,1003900),1004000).status==200);
 CHECK(r.rebindParticipants(participantRebind(std::to_string(epoch+1),"person1"),"2",1004000).status==200);CHECK(r.inputEpoch()==epoch+2);
}
void defenseRaceResetKeepsLeaseAndRevocationAuthoritative()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});r.addFrame(frame(1));CHECK(r.resetForRace());
 const std::string renew="{\"contract\":\"tholus.stk-spatial/1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expiresAtHostUs\":6000000,\"clockSamples\":[{\"hostSentUs\":1000000,\"serverReceivedUs\":1000000,\"serverSentUs\":1000000,\"hostReceivedUs\":1000000},{\"hostSentUs\":1001000,\"serverReceivedUs\":1001000,\"serverSentUs\":1001000,\"hostReceivedUs\":1001000},{\"hostSentUs\":1002000,\"serverReceivedUs\":1002000,\"serverSentUs\":1002000,\"hostReceivedUs\":1002000}]}";
 CHECK(r.route("/spatial/renew",renew,1003000).status==200);
 CHECK(r.rebindParticipants(participantRebind("1","person1"),"2",1003000).status==200);
 const auto revoked="{\"contract\":\"tholus.stk-spatial/1\",\"message\":{"+context+",\"type\":\"sessionRevoked\",\"reason\":\"revoked\"}}";
 CHECK(r.route("/spatial/input",revoked,1003001).status==200);CHECK(!r.resetForRace());
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",1003001).status==409);
}
void defenseRaceResetNeverRevivesAnExpiredOrReleasedGrant()
{
 for(bool release:{false,true}){
  Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});r.addFrame(frame(1));CHECK(r.resetForRace());
  if(release)r.reset("released");else r.tick(2000001,false);
  CHECK(!r.resetForRace());CHECK(r.rebindParticipants(participantRebind("1","person1"),"2",2000001).status==409);
 }
 Runtime vanilla;ready(vanilla);CHECK(!vanilla.resetForRace());CHECK(vanilla.prepared());
}
void defenseRaceResetRejectsOldFrameOrChangedStream()
{
 for(bool changedStream:{false,true}){
  Runtime r;ready(r);r.allowParticipantRebind(true);r.addFrame(frame(1));CHECK(r.resetForRace());
  auto f=frame(changedStream?2:1);f.generation=2;if(changedStream)f.stream=std::string(32,'a');r.addFrame(f);
  CHECK(!r.resetForRace());CHECK(!r.prepared());
 }
}
void projectionMatchesShader()
{
 Projection p;p.kind="equirectangular";Vec3 d;CHECK(Runtime::projectionDirection(p,0.5,0.5,&d)&&std::abs(d.z-1)<1e-12);
 CHECK(Runtime::projectionDirection(p,0.75,0.5,&d)&&std::abs(d.x-1)<1e-12);CHECK(Runtime::projectionDirection(p,0.5,0,&d)&&std::abs(d.y-1)<1e-12);
 p.kind="fisheye_domemaster";p.domeTiltDeg=30;CHECK(Runtime::projectionDirection(p,0.5,0.5,&d)&&std::abs(d.y+0.5)<1e-12);CHECK(!Runtime::projectionDirection(p,0,0,&d));
}
}
namespace
{
const uint64_t missionEntity=ScintillaMission::Runtime::CapsuleEntityBase+1;
ScintillaMission::Snapshot missionSnapshot(ScintillaMission::Phase phase=ScintillaMission::Phase::Open)
{
 ScintillaMission::Snapshot s;s.id=missionEntity;s.instance=(UINT64_C(1)<<53)+1;
 s.module=(UINT64_C(1)<<54)+3;s.phase=phase;s.phaseRevision=phase==ScintillaMission::Phase::Shell?1:2;
 s.eligible=true;s.deadline=10000;return s;
}
Frame missionFrame(uint64_t id,const ScintillaMission::Snapshot& snapshot)
{
 Frame f=frame(id);f.missionEnabled=true;f.missionRaceEpoch=1;f.missionRevision=2;
 f.layout=ScintillaDefense::Layout::create(f.projection.width,f.projection.height);
 f.targets[0].kind=snapshot.kind==ScintillaMission::Kind::Tether?TargetKind::MissionTether:TargetKind::MissionCapsule;
 f.targets[0].id=snapshot.id;f.targets[0].instance=snapshot.instance;f.targets[0].radius=2;
 f.targets[0].mission=snapshot;return f;
}
void missionReady(Runtime& r,const ScintillaMission::Snapshot& snapshot,bool twoSources=false)
{
 if(!twoSources)ready(r);
 else{
  std::string grant=prepare();
  replace(grant,"\"sources\":[{"+identity+"}]","\"sources\":[{"+identity+"},{"+secondSource(identity)+"}]");
  replace(grant,"\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"}]",
   "\"bindings\":[{\"sourceId\":\"s1\",\"participantId\":\"person1\"},{\"sourceId\":\"s2\",\"participantId\":\"person2\"}]");
  CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
  const auto tracked=message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\"");
  CHECK(r.route("/spatial/input",tracked,1000000).status==200);
  CHECK(r.route("/spatial/input",secondSource(tracked),1000000).status==200);
 }
 r.setCombatContext(true,0,{},1);r.setMissionContext(true,2,{snapshot},{});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
}
void missionToolPress(Runtime& r,unsigned source=0,const std::string& tag="mission",double u=.5)
{
 auto release=edgeAt(tag+"-release","released",0,999970,0,"secondary");
 auto press=edgeAt(tag+"-press","pressed",1,999980,0,"secondary");
 if(u!=.5)replace(press,"\"u\":0.5","\"u\":"+std::to_string(u));
 CHECK(r.route("/spatial/input",source?secondSource(release):release,1000000).status==200);
 CHECK(r.route("/spatial/input",source?secondSource(press):press,1000000).status==200);
}
void missionCoreReady(ScintillaMission::Runtime& core,const Runtime& input,const ScintillaMission::Snapshot& snapshot,bool twoSources=false)
{
 CHECK(core.prepare(1,2,input.inputEpoch(),4,twoSources?std::vector<std::string>{"person1","person2"}:std::vector<std::string>{"person1"},120));
 CHECK(core.advanceTo(0,true));ScintillaMission::CapsuleSpec capsule;
 capsule.id=snapshot.id;capsule.instance=snapshot.instance;capsule.module=snapshot.module;capsule.occurrence=0;capsule.expires=10000;
 CHECK(core.activateCapsule(capsule).changed);ScintillaMission::Shot open;
 open.race=1;open.revision=2;open.inputEpoch=input.inputEpoch();open.serial=1;
 open.entity=snapshot.id;open.instance=snapshot.instance;open.phaseRevision=1;open.participant="person1";open.event="core_fixture_open";
 open.historicalEligible=open.admittedEligible=true;CHECK(core.openCapsule(open).changed);
}
void missionHistoricalRangeAndWireIdentityStayTyped()
{
 Runtime r;auto snapshot=missionSnapshot();missionReady(r,snapshot);Frame f=missionFrame(1,snapshot);
 f.targets[0].center=Vec3(0,0,75);r.addFrame(f);CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("primary-release","released",0),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("primary-press","pressed",1),1000000).status==200);
 const auto shots=r.drainCombatShots();CHECK(shots.size()==1&&shots[0].kind==TargetKind::MissionCapsule);
 CHECK(shots[0].historicalEligible&&shots[0].historicalMission.id==snapshot.id);
 CHECK(shots[0].historicalMission.instance==snapshot.instance&&shots[0].missionRevision==2);
 CHECK(shots[0].admittedMissionTargets.size()==1&&shots[0].admittedTargets.empty());
 const auto json=r.stateJSON();
 CHECK(json.find("\"targetId\":\""+std::to_string(snapshot.id)+"\"")!=std::string::npos);
 CHECK(json.find("\"targetInstance\":\""+std::to_string(snapshot.instance)+"\"")!=std::string::npos);
 CHECK(json.find("\"moduleId\":\""+std::to_string(snapshot.module)+"\"")!=std::string::npos);
}
void missionAllPlausibleFramesMustAgreeOnPhaseAndKind()
{
 for(unsigned mutation=0;mutation<4;++mutation){
  Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);
  Frame a=missionFrame(1,snapshot),b=missionFrame(2,snapshot);
  if(mutation==0){b.targets[0].mission.phase=ScintillaMission::Phase::Shell;b.targets[0].mission.phaseRevision=1;}
  if(mutation==1)++b.targets[0].mission.phaseRevision;
  if(mutation==2)b.targets[0].kind=TargetKind::Rival;
  if(mutation==3)++b.targets[0].mission.module;
  r.addFrame(a);r.addFrame(b);CHECK(r.route("/spatial/presentation",presentation(true),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970,0,"secondary"),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("press","pressed",1,999990,0,"secondary"),1000000).status==200);
  CHECK(r.events().size()==1&&r.events().back().result!="hit");
  const auto beams=r.missionBeams(1000000);CHECK(beams.size()==1&&beams[0].held&&!beams[0].visible);
 }
}
void missionPressAdmissionCannotBorrowANewerOpenSnapshot()
{
 Runtime r;auto shell=missionSnapshot(ScintillaMission::Phase::Shell);missionReady(r,shell);
 const auto opened=missionSnapshot();r.addFrame(missionFrame(1,opened));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 auto beams=r.missionBeams(1000000);CHECK(beams.size()==1&&beams[0].held&&!beams[0].historicalEligible);
 r.setMissionContext(true,2,{opened},{});beams=r.missionBeams(1000000);
 CHECK(beams.size()==1&&!beams[0].visible&&!beams[0].historicalEligible);
}
void missionPreOpenPressAndPhaseChangesStayInterrupted()
{
 Runtime r;auto shell=missionSnapshot(ScintillaMission::Phase::Shell);missionReady(r,shell);
 r.addFrame(missionFrame(1,shell));CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 missionToolPress(r);CHECK(!r.missionBeams(1000000)[0].visible);
 const auto opened=missionSnapshot();r.setMissionContext(true,2,{opened},{});r.addFrame(missionFrame(2,opened));
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000001,1000010),1000020).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(1000005,.5,.5,10),1000020).status==200);
 auto beams=r.missionBeams(1000020);CHECK(beams.size()==1&&!beams[0].visible&&beams[0].interrupted);
 CHECK(beams[0].phaseRevision==1); // The press still means the closed shell.
}
void missionPressSerialIsImmutableAcrossNewAims()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.addFrame(missionFrame(1,snapshot));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 const auto first=r.missionBeams(1000000);CHECK(first.size()==1&&first[0].visible&&first[0].serial>first[0].releaseSerial);
 for(unsigned i=0;i<5;++i){
  CHECK(r.route("/spatial/input",feedbackAim(999981+i,.5,.5,10+i),1000000).status==200);
  const auto beams=r.missionBeams(1000000);CHECK(beams.size()==1&&beams[0].visible);
  CHECK(beams[0].serial==first[0].serial&&beams[0].controlEpoch==first[0].controlEpoch);
 }
 CHECK(!r.beamProofs(1000000)[0].held); // The same beam is never offensive SC2.
}
void missionPendingEarlierPressKeepsOwnershipOrder()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot,true);r.addFrame(missionFrame(1,snapshot));
 bool firstReady=false;r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3& direction,double){
  return std::abs(direction.x)<.001&&!firstReady?Visibility::Pending:Visibility::Visible;
 });
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 missionToolPress(r,0,"earlier");CHECK(r.missionBeams(1000000).empty());
 missionToolPress(r,1,"later",.51);CHECK(r.events().size()==1); // Later GPU proof is ready.
 CHECK(r.missionBeams(1000000).empty());
 ScintillaMission::Runtime core;missionCoreReady(core,r,snapshot,true);core.updateBeams(r.missionBeams(1000000));
 CHECK(core.snapshot(snapshot.id).owner==-1);
 firstReady=true;r.tick(1000000,true);const auto beams=r.missionBeams(1000000);
 CHECK(beams.size()==2&&beams[0].visible&&beams[1].visible&&beams[0].serial<beams[1].serial);
 core.updateBeams(beams);CHECK(core.snapshot(snapshot.id).owner==0);
 CHECK(core.feedback()[1].status==ScintillaMission::Status::TargetBusy);
}
void missionStickyLossAndCoalescedRealReleaseReachTheCore()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.addFrame(missionFrame(1,snapshot));
 Visibility visibility=Visibility::Visible;r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return visibility;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 ScintillaMission::Runtime core;missionCoreReady(core,r,snapshot);core.updateBeams(r.missionBeams(1000000));
 CHECK(core.snapshot(snapshot.id).owner==0);const auto original=r.missionBeams(1000000)[0];
 visibility=Visibility::Occluded;CHECK(r.route("/spatial/input",feedbackAim(999985,.5,.5,10),1000000).status==200);
 visibility=Visibility::Visible;CHECK(r.route("/spatial/input",feedbackAim(999990,.5,.5,11),1000000).status==200);
 CHECK(r.missionBeams(1000000)[0].interrupted&&!r.missionBeams(1000000)[0].visible);
 CHECK(core.advanceTo(1,true));core.updateBeams(r.missionBeams(1000000));CHECK(core.snapshot(snapshot.id).owner==-1);
 CHECK(r.route("/spatial/input",edgeAt("fresh-release","released",2,999991,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("fresh-press","pressed",3,999992,0,"secondary"),1000000).status==200);
 const auto fresh=r.missionBeams(1000000);CHECK(fresh.size()==1&&fresh[0].visible&&!fresh[0].interrupted);
 CHECK(fresh[0].releaseSerial>original.serial&&fresh[0].releaseSerial<fresh[0].serial);
 CHECK(core.advanceTo(2,true));core.updateBeams(fresh);CHECK(core.snapshot(snapshot.id).owner==0);
 CHECK(core.snapshot(snapshot.id).progress==0);
}
void missionContextMismatchAndRetirementRejectHistoricalProof()
{
 for(unsigned mutation=0;mutation<5;++mutation){
  Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);Frame f=missionFrame(1,snapshot);
  if(mutation==0)f.missionEnabled=false;if(mutation==1)++f.missionRaceEpoch;
  if(mutation==2)++f.missionRevision;if(mutation==3)++f.targets[0].instance;
  r.addFrame(f);if(mutation==4)r.setLiveTargetOracle([](uint64_t,uint64_t){return false;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
  const auto beams=r.missionBeams(1000000);CHECK(beams.size()==1&&!beams[0].visible);
  CHECK(r.events().back().result!="hit");
 }
}
void missionHeldWorldCanObserveOnlyRealCurrentPresentation()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.addFrame(missionFrame(1,snapshot));
 CHECK(r.presentedMissionFrames(1000000).empty());
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 r.tick(1000010,false);auto presented=r.presentedMissionFrames(1000010);
 CHECK(presented.size()==1&&presented[0].frame.missionRaceEpoch==1&&presented[0].high<=1000010);
 CHECK(r.missionBeams(1000010).empty());
 r.setMissionContext(true,3,{snapshot},{});CHECK(r.presentedMissionFrames(1000010).empty());
 r.setMissionContext(true,2,{snapshot},{});CHECK(r.presentedMissionFrames(2000001).empty());
 r.reset("released");CHECK(r.presentedMissionFrames(1000010).empty());
}
void missionMixedPrimaryQueuePreservesBothTypedDeliveries()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot,true);
 Frame f=missionFrame(1,snapshot);f.targets[0].center=Vec3(4,0,10);
 Target rival;rival.id=7;rival.instance=70;rival.radius=.3;rival.center=Vec3(0,0,10);
 rival.combat.target=7;rival.combat.instance=700;rival.combat.eligible=true;f.targets.push_back(rival);
 r.setCombatContext(true,11,{rival.combat},1);r.addFrame(f);
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("rival-release","released",0),1000000).status==200);
 CHECK(r.route("/spatial/input",edge("rival-primary","pressed",1),1000000).status==200);
 auto release=secondSource(edge("capsule-release","released",0));
 auto shot=secondSource(edge("capsule-primary","pressed",1));replace(shot,"\"u\":0.5","\"u\":0.56055947");
 CHECK(r.route("/spatial/input",release,1000000).status==200);CHECK(r.route("/spatial/input",shot,1000000).status==200);
 const auto batch=r.drainCombatShots();CHECK(batch.size()==2&&batch[0].serial<batch[1].serial);
 CHECK(batch[0].kind==TargetKind::Rival&&batch[0].historical.instance==700);
 CHECK(batch[1].kind==TargetKind::MissionCapsule&&batch[1].historicalMission.instance==snapshot.instance);
 CHECK(batch[0].admittedTick==11&&batch[1].admittedTick==11);CHECK(r.drainCombatShots().empty());
}
void missionFeedbackIsEpochCheckedPersistentAndContextSpecific()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.addFrame(missionFrame(1,snapshot));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 const auto beam=r.missionBeams(1000000)[0];ScintillaMission::Feedback feedback;
 feedback.participant="person1";feedback.controlEpoch=beam.controlEpoch;feedback.entity=beam.entity;
 feedback.status=ScintillaMission::Status::TargetBusy;feedback.owner=1;
 r.setMissionContext(true,2,{snapshot},{feedback});
 CHECK(Runtime::missionFeedbackForRendering(r.reticles()[0]).status==ScintillaMission::Status::TargetBusy);
 CHECK(r.stateJSON().find("\"entityId\":\""+std::to_string(snapshot.id)+"\"")!=std::string::npos);
 --feedback.controlEpoch;r.setMissionContext(true,2,{snapshot},{feedback});
 CHECK(Runtime::missionFeedbackForRendering(r.reticles()[0]).status==ScintillaMission::Status::None);
 ++feedback.controlEpoch;r.setMissionContext(true,2,{snapshot},{feedback});
 CHECK(r.route("/spatial/input",feedbackAim(999985,.75,.5,10),1000000).status==200);
 CHECK(Runtime::missionFeedbackForRendering(r.reticles()[0]).status==ScintillaMission::Status::ReleaseRequired);
 CHECK(r.route("/spatial/input",feedbackAim(999990,.5,.5,11),1000000).status==200);
 r.setMissionContext(true,2,{snapshot},{feedback});
 CHECK(Runtime::missionFeedbackForRendering(r.reticles()[0]).status==ScintillaMission::Status::ReleaseRequired);
 r.tick(1000010,false);CHECK(Runtime::missionFeedbackForRendering(r.reticles()[0]).status==ScintillaMission::Status::None);
}
void missionReleaseReceiptDoesNotSurvivePauseOrParticipantRebind()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.addFrame(missionFrame(1,snapshot));
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 CHECK(r.missionBeams(1000000)[0].releaseSerial>0);
 r.tick(1000010,false);r.tick(1000020,true);CHECK(r.missionBeams(1000020).empty());
 CHECK(r.route("/spatial/input",edgeAt("paused-release","released",2,1000015,0,"secondary"),1000030).status==409);
 CHECK(r.missionBeams(1000030).empty());
 CHECK(r.route("/spatial/input",edgeAt("current-release","released",3,1000040,0,"secondary"),1000050).status==200);
 const auto released=r.missionBeams(1000050);CHECK(released.size()==1&&!released[0].held&&released[0].releaseSerial>0);
 r.allowParticipantRebind(true);r.setParticipants({"person2"});
 CHECK(r.rebindParticipants(participantRebind(),"2",1000050).status==200);CHECK(r.inputEpoch()==2);
 r.tick(1000060,true);CHECK(r.missionBeams(1000060).empty());
}
void missionNewerAimCannotReinterpretAnOlderPendingPress()
{
 for(bool differentTarget:{false,true}){
  Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot,true);
  Frame f=missionFrame(1,snapshot);Target other=f.targets[0];
  ++other.id;++other.instance;other.center=Vec3(5,0,10);
  other.mission.id=other.id;other.mission.instance=other.instance;++other.mission.module;
  f.targets.push_back(other);r.setMissionContext(true,2,{snapshot,other.mission},{});r.addFrame(f);
  uint64_t firstQuery=0;bool firstReady=false;
  r.setQueryLeaseOracle([](uint64_t){return std::shared_ptr<void>(std::make_shared<int>(1));});
  r.setVisibilityOracle([&](const Frame& candidate,const Target&,const Vec3&,double){
   CHECK(candidate.queryOwner!=0);if(!firstQuery)firstQuery=candidate.queryOwner;
   return candidate.queryOwner==firstQuery&&!firstReady?Visibility::Pending:Visibility::Visible;
  });
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
  CHECK(r.missionBeams(1000000).empty());CHECK(r.events().empty());
  CHECK(r.route("/spatial/input",feedbackAim(999990,differentTarget?.5737918:.5,.5,10),1000000).status==200);
  firstReady=true;r.tick(1000000,true);const auto beams=r.missionBeams(1000000);
  CHECK(beams.size()==1&&beams[0].entity==snapshot.id);
  CHECK(beams[0].visible==!differentTarget&&beams[0].interrupted==differentTarget);
 }
}
void missionNewDefenseWarningDoesNotInterruptTheSameCapsule()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);
 Frame f=missionFrame(1,snapshot);f.defense.enabled=true;f.defense.threat=9;
 f.layout=ScintillaDefense::Layout::create(2048,1024);r.addFrame(f);r.setDefenseContext(true,{});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 const auto first=r.missionBeams(1000000);CHECK(first.size()==1&&first[0].visible);
 f.id=2;f.defense.threat=10;r.addFrame(f);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000001,1000010),1000020).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(1000005,.5,.5,10),1000020).status==200);
 const auto second=r.missionBeams(1000020);CHECK(second.size()==1&&second[0].visible&&!second[0].interrupted);
 CHECK(first[0].serial==second[0].serial&&first[0].controlEpoch==second[0].controlEpoch);
}

void missionLayoutMasksMatchExactCargoCircles()
{
 using ScintillaDefense::Context;using ScintillaDefense::Layout;
 const unsigned dimensions[][2]={{256,256},{1536,1536},{2048,1024},{1024,2048}};
 for(const auto& size:dimensions){
  const auto l=Layout::create(size[0],size[1],true);const double scale=std::min(size[0],size[1]);
  CHECK(l.valid()&&l.mission&&l.revision()==2&&std::string(l.profile())=="front_rear_cargo_v2");
  for(unsigned i=0;i<3;++i)for(unsigned kind=0;kind<2;++kind){
   double x,y,r;if(kind)l.beacon(i,&x,&y,&r);else l.cargo(i,&x,&y,&r);
   CHECK(std::abs((x*size[0]-size[0]*.5)*2/scale-(-.18+.18*i))<1e-12);
   CHECK(std::abs((y*size[1]-size[1]*.5)*2/scale-(kind?-.27:-.16))<1e-12);
   CHECK(std::abs(r-(kind?.025:.06)*scale*.5)<1e-12);
   CHECK(l.classify(x,y).context==Context::ReservedHUD);
   CHECK(!l.worldRectFree(x,y,x,y));
   // All quadrants share the exact circle. The rectangle test must reject
   // an intersecting footprint even when its centre is outside the HUD.
   for(unsigned angle=0;angle<16;++angle){
    const double a=angle*6.283185307179586/16,dx=std::cos(a)*r/size[0],dy=std::sin(a)*r/size[1];
    CHECK(l.classify(x+.999*dx,y+.999*dy).context==Context::ReservedHUD);
    CHECK(l.classify(x+1.001*dx,y+1.001*dy).context==Context::WorldOffense);
   }
   CHECK(!l.worldRectFree(x+.9*r/size[0],y-.1*r/size[1],x+1.2*r/size[0],y+.1*r/size[1]));
   CHECK(l.worldRectFree(x+1.01*r/size[0],y-.01*r/size[1],x+1.1*r/size[0],y+.01*r/size[1]));
  }
  CHECK(!l.worldRectFree(.6,.4,.5,.5));
  CHECK(!l.worldRectFree(std::numeric_limits<double>::quiet_NaN(),.4,.5,.5));
 }
}
void missionLayoutPreservesLegacySC3AndResizes()
{
 using ScintillaDefense::Context;using ScintillaDefense::Layout;
 for(unsigned width:{256u,1536u,2048u}){
  const unsigned height=width==2048?1024:width;
  const auto legacy=Layout::create(width,height),mission=Layout::create(width,height,true);
  CHECK(!legacy.mission&&legacy.revision()==1&&std::string(legacy.profile())=="front_rear_readonly_v1");
  for(unsigned i=0;i<4;++i)CHECK(legacy.rear[i]==mission.rear[i]);
  for(unsigned i=0;i<5;++i){double x,y,r,a,b,c;legacy.node(i,&x,&y,&r);mission.node(i,&a,&b,&c);CHECK(x==a&&y==b&&r==c);}
  const double scale=std::min(width,height);
  for(unsigned row=0;row<=96;++row)for(unsigned col=0;col<=96;++col){
   const double u=col/96.,v=row/96.,x=(u*width-width*.5)*2/scale,y=(v*height-height*.5)*2/scale;
   bool cargo=false;for(unsigned i=0;i<3;++i)
    cargo=cargo||std::hypot(x-(-.18+.18*i),y+.16)<=.06||std::hypot(x-(-.18+.18*i),y+.27)<=.025;
   const auto before=legacy.classify(u,v),after=mission.classify(u,v);
   CHECK(cargo?(before.context==Context::WorldOffense&&after.context==Context::ReservedHUD):
         (before.context==after.context&&before.sector==after.sector));
  }
 }
 // Resizing recreates circles in the same isotropic space, rather than
 // carrying stale pixel bounds from the previous producer dimensions.
 const auto small=Layout::create(256,256,true),large=Layout::create(2048,1024,true);
 double sx,sy,sr,lx,ly,lr;small.cargo(0,&sx,&sy,&sr);large.cargo(0,&lx,&ly,&lr);
 CHECK(sx!=lx&&sy==ly&&lr==sr*4);CHECK(large.classify(lx,ly).context==Context::ReservedHUD);
 const auto terminal=Layout::create(2048,1024,true,true);
 CHECK(terminal.terminal&&!Layout::create(2048,1024,false,true).terminal);
 const auto uv=[](double x,double y){return std::make_pair(.5+x*.25,.5+y*.5);};
 for(const auto& p:{std::make_pair(-.76,-.08),std::make_pair(.76,.40),std::make_pair(0.,0.)}){
  const auto q=uv(p.first,p.second);CHECK(terminal.classify(q.first,q.second).context==Context::ReservedHUD);
  CHECK(!terminal.worldRectFree(q.first,q.second,q.first,q.second));
 }
 for(const auto& p:{std::make_pair(-.7601,0.),std::make_pair(.7601,0.),std::make_pair(0.,-.0801),std::make_pair(0.,.4001)}){
  const auto q=uv(p.first,p.second);CHECK(terminal.classify(q.first,q.second).context==Context::WorldOffense);
 }
}
std::string defenseLayoutPrepareFixture()
{
 return "{\"contract\":\"tholus.stk-scintilla-defense/1\",\"profile\":\"sc3_sector_rear_readonly_v1\",\"inputSessionID\":\"session\",\"inputEpoch\":\"1\",\"raceEpoch\":\"1\",\"rosterRevision\":\"2\",\"combatProfile\":\"crew_standard_sc2_v1\",\"layoutProfile\":\"front_rear_readonly_v1\",\"layoutRevision\":\"1\",\"routeRevision\":\"route\",\"projectionContextId\":\"projection\",\"visibilityEvidenceID\":\"measured-profile\",\"visibleRegions\":[{\"left\":0,\"top\":0,\"right\":1,\"bottom\":1}]}";
}
void missionLayoutProtocolRequiresTheExactRevisionPair()
{
 Runtime r;ready(r);DefenseAdmission admission;
 const auto legacy=defenseLayoutPrepareFixture();auto mission=legacy;
 replace(mission,"front_rear_readonly_v1","front_rear_cargo_v2");replace(mission,"\"layoutRevision\":\"1\"","\"layoutRevision\":\"2\"");
 CHECK(r.validateDefensePrepare(legacy,"1","2",&admission).status==200);
 CHECK(r.validateDefensePrepare(mission,"1","2",&admission).status==400);
 CHECK(r.validateDefensePrepare(legacy,"1","2",&admission,true).status==400);
 CHECK(r.validateDefensePrepare(mission,"1","2",&admission,true).status==200);
 for(unsigned mutation=0;mutation<3;++mutation){auto bad=mission;
  if(mutation==0)replace(bad,"\"layoutRevision\":\"2\"","\"layoutRevision\":\"1\"");
  if(mutation==1)replace(bad,"front_rear_cargo_v2","front_rear_readonly_v1");
  if(mutation==2)replace(bad,"\"layoutRevision\":\"2\"","\"layoutRevision\":\"3\"");
  CHECK(r.validateDefensePrepare(bad,"1","2",&admission,true).status==400);
 }
 CHECK(r.validateDefensePrepare(mission,"2","2",&admission,true).status==409);
}
void missionLayoutHUDCannotFallThroughBeforeMissionPreparation()
{
 for(unsigned i=0;i<3;++i)for(unsigned kind=0;kind<2;++kind){
  Runtime r;ready(r);r.setMissionLayoutRequired(true);r.setCombatContext(true,0,{},1);
  Frame f=frame(1);f.layout=ScintillaDefense::Layout::create(2048,1024,true);
  double u,v,radius;if(kind)f.layout.beacon(i,&u,&v,&radius);else f.layout.cargo(i,&u,&v,&radius);
  Vec3 direction;CHECK(Runtime::projectionDirection(f.projection,u,v,&direction));
  f.targets[0].center=Vec3(direction.x*10,direction.y*10,direction.z*10);
  r.addFrame(f);unsigned queries=0;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return Visibility::Visible;});
  r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970),1000000).status==200);
  auto press=edgeAt("cargo","pressed",1,999980);
  replace(press,"\"u\":0.5","\"u\":"+std::to_string(u));replace(press,"\"v\":0.5","\"v\":"+std::to_string(v));
  CHECK(r.route("/spatial/input",press,1000000).status==200);
  CHECK(r.events().back().result=="non_targetable_surface"&&queries==0);
  CHECK(r.drainCombatShots().back().result=="non_targetable_surface");
 }
}
void missionLayoutOldAndMixedHistoricalProofsAreRejected()
{
 for(bool mixed:{false,true}){
  Runtime r;ready(r);r.setMissionLayoutRequired(true);r.setCombatContext(true,0,{},1);
  Frame a=frame(1),b=frame(2);a.defense.enabled=b.defense.enabled=true;
  a.layout=ScintillaDefense::Layout::create(2048,1024);b.layout=ScintillaDefense::Layout::create(2048,1024,true);
  r.addFrame(a);if(mixed)r.addFrame(b);unsigned queries=0;
  r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return Visibility::Visible;});
  r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
  CHECK(r.route("/spatial/presentation",presentation(mixed),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("release","released",0,999970),1000000).status==200);
  CHECK(r.route("/spatial/input",edgeAt("old-layout","pressed",1,999990,mixed?1:0),1000000).status==200);
  CHECK(r.events().back().result=="layout_unavailable"&&queries==0);
  CHECK(r.drainCombatShots().back().result=="layout_unavailable");
 }
}
void missionLayoutIntentChangeDisarmsPendingProofWithoutClosingTheGrant()
{
 Runtime r;ready(r);const auto snapshot=missionSnapshot();Frame old=missionFrame(1,snapshot);
 old.defense.enabled=true;r.addFrame(old);r.setCombatContext(true,0,{},1);
 r.setMissionContext(true,2,{snapshot},{});r.setDefenseContext(true,{});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});bool resolved=false;
 r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return resolved?Visibility::Visible:Visibility::Pending;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 CHECK(r.events().empty());r.setMissionLayoutRequired(true);resolved=true;r.tick(1000000,true);
 CHECK(r.prepared()&&r.events().empty()&&r.drainCombatShots().empty());
 CHECK(!r.reticles()[0].toolHeld&&r.presentedDefenseFrames(1000000).empty()&&r.presentedMissionFrames(1000000).empty());
 CHECK(!r.defenseVideoReady(1000000));
 // A new correct producer frame establishes evidence without recreating SI5.
 Frame fresh=missionFrame(2,snapshot);fresh.defense.enabled=true;
 fresh.layout=ScintillaDefense::Layout::create(2048,1024,true);r.addFrame(fresh);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000001,1000010),1000020).status==200);
 CHECK(r.presentedDefenseFrames(1000020).size()==1&&r.presentedMissionFrames(1000020).size()==1&&r.defenseVideoReady(1000020));
 CHECK(r.route("/spatial/input",edgeAt("must-release","pressed",2,1000004,0,"secondary"),1000020).status==409);
 CHECK(r.route("/spatial/input",edgeAt("fresh-release","released",3,1000005,0,"secondary"),1000020).status==200);
 CHECK(r.route("/spatial/input",edgeAt("fresh-press","pressed",4,1000006,0,"secondary"),1000020).status==200);
 CHECK(r.missionBeams(1000020).size()==1&&r.missionBeams(1000020)[0].visible);
}
void missionLayoutPresentationRejectsWrongRevisionAndResizedMetadata()
{
 for(unsigned mutation=0;mutation<3;++mutation){
  Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.setMissionLayoutRequired(true);
  Frame f=missionFrame(1,snapshot);f.defense.enabled=true;
  f.layout=ScintillaDefense::Layout::create(2048,1024,true);
  if(mutation==0)f.layout.mission=false;if(mutation==1)f.layout.width=1024;if(mutation==2)f.layout.height=2048;
  r.addFrame(f);r.setDefenseContext(true,{});
  CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
  CHECK(r.presentedDefenseFrames(1000000).empty()&&r.presentedMissionFrames(1000000).empty());
  CHECK(!r.defenseVideoReady(1000000));
  r.tick(1000010,false);CHECK(r.presentedMissionFrames(1000010).empty());
 }
}
void missionLayoutReadOnlyCargoDepartureStaysInterruptedAfterCoalescing()
{
 Runtime r;const auto snapshot=missionSnapshot();missionReady(r,snapshot);r.setMissionLayoutRequired(true);
 Frame f=missionFrame(1,snapshot);f.layout=ScintillaDefense::Layout::create(2048,1024,true);r.addFrame(f);
 bool readyDepth=true;r.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){return readyDepth?Visibility::Visible:Visibility::Pending;});
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);missionToolPress(r);
 CHECK(r.missionBeams(1000000).size()==1&&r.missionBeams(1000000)[0].visible);
 readyDepth=false;
 CHECK(r.route("/spatial/input",feedbackAim(999985,.5,.5,10),1000000).status==200);
 // Cargo is readonly even before defenseEnabled becomes true. Its resolved
 // surface is a negative fact, not a synthetic tool release or a world hit.
 CHECK(r.route("/spatial/input",feedbackAim(999986,.5,.42,11),1000000).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(999987,.5,.5,12),1000000).status==200);
 readyDepth=true;r.tick(1000000,true);
 const auto beams=r.missionBeams(1000000);
 CHECK(beams.size()==1&&beams[0].held&&!beams[0].visible&&beams[0].interrupted);
}
void missionPreparationCanObserveRealDefenseLayoutWhileHeld()
{
 Runtime r;ready(r);r.setMissionLayoutRequired(true);Frame f=frame(1);
 f.defense.enabled=true;f.layout=ScintillaDefense::Layout::create(2048,1024,true);r.addFrame(f);
 CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);r.tick(1000010,false);
 CHECK(r.presentedDefenseFrames(1000010).empty());
 const auto held=r.presentedDefenseFrames(1000010,true);CHECK(held.size()==1&&held[0].frame.layout.mission);
 CHECK(!r.defenseVideoReady(1000010)&&r.defenseVideoReady(1000010,true));
 CHECK(!held[0].frame.missionEnabled); // SC4 itself need not already be prepared.
 CHECK(r.presentedDefenseFrames(1200000,true).empty());r.reset("released");
 CHECK(r.presentedDefenseFrames(1000010,true).empty());
}


// SC4 maintenance retains the physical grant through explicit slow preflight.
// Pure consumer fixtures: engine held/profile/producer/race guards are separate.
const std::string maintenanceID="00000000-0000-4000-8000-000000000010";
std::string maintenanceRequest(const std::string& epoch="1",const std::string& operationID=maintenanceID,const std::string& extra="")
{return "{\"contract\":\"tholus.stk-scintilla-input-maintenance/1\",\"schemaVersion\":1,\"producerUUID\":\"00000000-0000-4000-8000-000000000002\",\"raceEpoch\":\"1\",\"inputSessionID\":\"session\",\"routingGeneration\":\"generation\",\"expectedInputEpoch\":\""+epoch+"\",\"operationID\":\""+operationID+"\""+extra+"}";}
std::string maintenanceResume(int64_t now=8000000,int64_t expires=12000000,const std::string& epoch="2",const std::string& operationID=maintenanceID)
{
 std::string samples;for(int i=3;i>0;--i){if(!samples.empty())samples+=',';const auto t=std::to_string(now-i*100);samples+="{\"hostSentUs\":"+t+",\"serverReceivedUs\":"+t+",\"serverSentUs\":"+t+",\"hostReceivedUs\":"+t+"}";}
 return maintenanceRequest(epoch,operationID,",\"expiresAtHostUs\":"+std::to_string(expires)+",\"clockSamples\":["+samples+"]");
}
void maintenanceReady(Runtime& r)
{ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});r.tick(1000000,false);}
void missionInputMaintenanceRetainsGrantAndReplayAcrossSlowPreflight()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});r.addFrame(frame(1));
 CHECK(r.route("/spatial/input",edgeAt("maintenance-release","released",10,999980),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("maintenance-press","pressed",11,999990),1000000).status==200);
 r.tick(1000000,false);CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);
 CHECK(r.inputEpoch()==2&&r.inputMaintenanceActive()&&r.hasPreparedGrant()&&!r.prepared());CHECK(!r.reticles()[0].primaryHeld&&!r.reticles()[0].toolHeld);
 r.tick(8000000,true);CHECK(r.inputMaintenanceActive()&&r.hasPreparedGrant());CHECK(r.stateJSON().find("\"active\":false")!=std::string::npos);
 CHECK(r.inputMaintenance("resume",maintenanceResume(),8000000).status==200);CHECK(!r.inputMaintenanceActive()&&r.hasPreparedGrant()&&!r.prepared());CHECK(r.inputEpoch()==2);
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",8000000).status==200);CHECK(r.inputEpoch()==3&&r.prepared());
 CHECK(r.stateJSON().find("\"historyReady\":false")!=std::string::npos);CHECK(r.presentedDefenseFrames(8000000,true).empty());
 r.tick(8000010,true);const auto replay=r.route("/spatial/input",edgeAt("maintenance-press","pressed",11,8000020),8000030);CHECK(replay.status!=200);CHECK(!r.reticles()[0].primaryHeld);
 r.reset("test");CHECK(r.route("/spatial/prepare",prepare(),8000040).status==409); // usedSessions survives.
}
void missionInputMaintenanceRejectsAllOrdinaryReactivationPaths()
{
 Runtime r;maintenanceReady(r);CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);
 for(const auto& path:{"/spatial/prepare","/spatial/update","/spatial/renew","/spatial/presentation","/spatial/input","/spatial/release"}){
  const auto reply=r.route(path,"{}",8000000);CHECK(reply.status==409&&reply.json.find("input_maintenance_suspended")!=std::string::npos);CHECK(r.inputMaintenanceActive()&&r.hasPreparedGrant()&&r.inputEpoch()==2);
 }
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",8000000).status==409);
 CHECK(r.route("/spatial/state","",8000000).status==200);CHECK(r.inputMaintenanceActive());
}
void missionInputMaintenanceTokensAreBoundedAndIdempotent()
{
 Runtime r;maintenanceReady(r);const auto begin=r.inputMaintenance("begin",maintenanceRequest(),1000000);CHECK(begin.status==200&&begin.json.find("\"inputEpoch\":\"2\"")!=std::string::npos);
 const auto heldEpoch=r.reticles()[0].toolEpoch;
 CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000010).status==200);CHECK(r.inputEpoch()==2&&r.reticles()[0].toolEpoch==heldEpoch);
 const std::string other="00000000-0000-4000-8000-000000000011";
 CHECK(r.inputMaintenance("begin",maintenanceRequest("2",other),1000020).status==409);CHECK(r.inputMaintenance("resume",maintenanceResume(1000030,6000000,"2",other),1000030).status==409);
 CHECK(r.inputMaintenance("resume",maintenanceResume(1000040,6000000),1000040).status==200);const auto resumedEpoch=r.reticles()[0].toolEpoch;
 CHECK(r.inputMaintenance("resume",maintenanceResume(1000050,10000000),1000050).status==200);CHECK(r.reticles()[0].toolEpoch==resumedEpoch&&r.inputEpoch()==2);
 const auto retry=r.inputMaintenance("begin",maintenanceRequest(),1000060);CHECK(retry.status==200&&retry.json.find("\"suspended\":false")!=std::string::npos);CHECK(!r.inputMaintenanceActive());
 CHECK(r.inputMaintenance("begin",maintenanceRequest("2",other),1000070).status==200);CHECK(r.inputEpoch()==3);
 CHECK(r.inputMaintenance("resume",maintenanceResume(1000080,6000000),1000080).status==409); // Old nonce cannot unlock new operation.
}
void missionInputMaintenanceRequiresFreshClockAndAuthoritativeExpiry()
{
 for(int mutation=0;mutation<7;++mutation){Runtime r;maintenanceReady(r);CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);r.tick(8000000,false);
  auto request=maintenanceResume();
  if(mutation==0)replace(request,"\"expiresAtHostUs\":12000000","\"expiresAtHostUs\":7000000");
  else if(mutation==1)replace(request,"\"expiresAtHostUs\":12000000","\"expiresAtHostUs\":19000000");
  else if(mutation==2)replace(request,"\"hostSentUs\":7999700","\"hostSentUs\":1");
  else if(mutation==3)replace(request,"\"expectedInputEpoch\":\"2\"","\"expectedInputEpoch\":\"1\"");
  else if(mutation==4)replace(request,"\"routingGeneration\":\"generation\"","\"routingGeneration\":\"other\"");
  else if(mutation==5)replace(request,"\"inputSessionID\":\"session\"","\"inputSessionID\":\"other\"");
  else request=maintenanceResume(6000000,12000000);
  CHECK(r.inputMaintenance("resume",request,8000000).status!=200);CHECK(r.inputMaintenanceActive()&&r.inputEpoch()==2);
  CHECK(r.inputMaintenance("resume",maintenanceResume(),8000000).status==200);
 }
}
void missionInputMaintenanceDuplicateResumeDoesNotExtendLease()
{
 Runtime r;maintenanceReady(r);CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);
 CHECK(r.inputMaintenance("resume",maintenanceResume(1000100,2000000),1000100).status==200);
 CHECK(r.inputMaintenance("resume",maintenanceResume(1000200,9000000),1000200).status==200);
 r.tick(2000001,false);CHECK(!r.hasPreparedGrant()&&!r.inputMaintenanceActive());
 CHECK(r.inputMaintenance("resume",maintenanceResume(2000010,9000000),2000010).status==409);
}
void missionInputMaintenanceSurvivesSameGrantRaceResetButNotInvalidation()
{
 Runtime r;maintenanceReady(r);CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);r.tick(8000000,false);
 CHECK(r.resetForRace());CHECK(r.inputMaintenanceActive()&&r.hasPreparedGrant()&&r.inputEpoch()==2);
 auto request=maintenanceResume();replace(request,"\"raceEpoch\":\"1\"","\"raceEpoch\":\"2\"");CHECK(r.inputMaintenance("resume",request,8000000).status==200);
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",8000000).status==200);
 Runtime q;maintenanceReady(q);CHECK(q.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);q.reset("producer_projection_changed");
 CHECK(!q.inputMaintenanceActive()&&!q.hasPreparedGrant());CHECK(q.inputMaintenance("resume",maintenanceResume(),8000000).status==409);
}
void missionInputMaintenanceRestoresOnlyFreshReleaseAndProofs()
{
 Runtime r;maintenanceReady(r);r.addFrame(frame(1));CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.inputMaintenance("begin",maintenanceRequest(),1000000).status==200);
 CHECK(r.inputMaintenance("resume",maintenanceResume(2000000,6000000),2000000).status==200);
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",2000000).status==200);r.tick(2000010,true);
 CHECK(r.presentedDefenseFrames(2000010,true).empty());
 Frame fresh=frame(2);fresh.captureUs=2000020;r.addFrame(fresh);
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 CHECK(r.route("/spatial/presentation",racePresentation(2,2000030,2001000),2001100).status==200);
 const auto blocked=r.route("/spatial/input",edgeAt("maintenance-held","pressed",0,2000500,0,"secondary"),2001100);CHECK(blocked.status==409&&blocked.json.find("release_required")!=std::string::npos);
 CHECK(r.route("/spatial/input",edgeAt("maintenance-fresh-release","released",1,2000600,0,"secondary"),2001100).status==200);
 CHECK(r.route("/spatial/input",edgeAt("maintenance-fresh-press","pressed",2,2000700,0,"secondary"),2001100).status==200);
 CHECK(r.reticles()[0].toolHeld&&r.hasPreparedGrant()&&r.prepared());CHECK(!r.events().empty()&&r.events().back().frameId==2);
}
void missionInputMaintenanceClosedProtocolAndInitialLeaseRemainRequired()
{
 for(int mutation=0;mutation<5;++mutation){Runtime r;maintenanceReady(r);auto request=maintenanceRequest();
  if(mutation==0)replace(request,"\"expectedInputEpoch\":\"1\"","\"expectedInputEpoch\":\"01\"");
  else if(mutation==1)request.insert(1,"\"unexpected\":true,");
  else if(mutation==2)replace(request,"\"schemaVersion\":1","\"schemaVersion\":2");
  else if(mutation==3)replace(request,maintenanceID,"not-a-uuid");
  else replace(request,"\"expectedInputEpoch\":\"1\"","\"expectedInputEpoch\":\"2\"");
  CHECK(r.inputMaintenance("begin",request,1000000).status!=200);CHECK(r.inputEpoch()==1&&!r.inputMaintenanceActive());
 }
 Runtime r;maintenanceReady(r);CHECK(r.inputMaintenance("begin",maintenanceRequest(),2000001).status==409);CHECK(!r.inputMaintenanceActive());
}

void crewBindingObservationUsesNativePhysicalIdentityAndLossHistory()
{
 Runtime r;ready(r);auto bindings=r.bindingStates();CHECK(bindings.size()==1);
 const auto initial=bindings[0];CHECK(initial.participantId=="person1"&&initial.sourceId=="s1");
 CHECK(initial.deviceBindingRevision=="b1"&&initial.trackingSpaceEpoch=="e1"&&initial.poseContinuityId=="c1");
 CHECK(initial.roomCalibrationRevision=="r1"&&initial.gripCalibrationRevision=="g1"&&initial.tracked&&!initial.neutral&&initial.trackingLossRevision==0);
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),1000000).status==200);
 bindings=r.bindingStates();CHECK(!bindings[0].tracked&&bindings[0].trackingLossRevision==1);
 CHECK(bindings[0].deviceBindingRevision==initial.deviceBindingRevision);
}
void crewLogicalRebindPreservesPhysicalBindingAndRequiresNewEpoch()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person2"});
 const auto original=r.bindingStates()[0];CHECK(r.rebindParticipants(participantRebind("1","person2"),"2",1000000).status==200);
 const auto rebound=r.bindingStates()[0];CHECK(r.inputEpoch()==2&&rebound.participantId=="person2");
 CHECK(rebound.sourceId==original.sourceId&&rebound.deviceBindingRevision==original.deviceBindingRevision&&rebound.poseContinuityId==original.poseContinuityId);
 CHECK(!rebound.neutral&&rebound.trackingLossRevision==original.trackingLossRevision&&r.hasPreparedGrant());
}
void crewPossessionContinuityCannotRecoverWithoutNewInputEpoch()
{
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});
 CHECK(r.bindingStates()[0].continuityIntact);const auto epoch=r.inputEpoch();
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost_after_challenge\""),1000010).status==200);
 CHECK(!r.bindingStates()[0].tracked&&!r.bindingStates()[0].continuityIntact);
 auto recovered=message("\"type\":\"sourceState\",\"sequence\":1,\"state\":\"tracked\",\"reason\":\"recovered_during_prepare\"");
 replace(recovered,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(r.route("/spatial/input",recovered,1000020).status==200);
 for(unsigned control=0;control<2;++control){
  auto up=edgeAt("recovered-up"+std::to_string(control),"released",control,1000030+control,0,control?"secondary":"primary");
  replace(up,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
  CHECK(r.route("/spatial/input",up,1000040).status==200);
 }
 const auto recoveredBinding=r.bindingStates()[0];
 CHECK(recoveredBinding.tracked&&recoveredBinding.neutral&&!recoveredBinding.continuityIntact);
 CHECK(r.inputEpoch()==epoch&&r.prepared()&&r.hasPreparedGrant());
 // A fresh native epoch is required even if SC2--4 await long enough to see
 // tracked+neutral again. Rebinding keeps the healthy physical grant alive.
 CHECK(r.rebindParticipants(participantRebind(std::to_string(epoch),"person1"),"2",1000050).status==200);
 const auto rebound=r.bindingStates()[0];
 CHECK(r.inputEpoch()==epoch+1&&rebound.tracked&&rebound.continuityIntact&&!rebound.neutral);
 CHECK(rebound.trackingLossRevision==recoveredBinding.trackingLossRevision&&rebound.poseContinuityId=="c2");
 CHECK(r.prepared()&&r.hasPreparedGrant());
}
void crewPossessionBaselinePrecedesFirstReadAndIdempotentRebindCannotHealLoss()
{
 Runtime initial;ready(initial);
 CHECK(initial.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"initial_loss_before_get\""),1000010).status==200);
 auto firstRecovery=message("\"type\":\"sourceState\",\"sequence\":1,\"state\":\"tracked\",\"reason\":\"initial_recovery\"");
 replace(firstRecovery,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(initial.route("/spatial/input",firstRecovery,1000020).status==200);
 CHECK(initial.inputEpoch()==1&&initial.bindingStates()[0].tracked&&!initial.bindingStates()[0].continuityIntact);
 Runtime r;ready(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});
 const auto request=participantRebind("1","person1");CHECK(r.rebindParticipants(request,"2",1000010).status==200);
 // No bindingStates observation occurs before this loss: the baseline belongs
 // to the rebind transaction, rather than the first HTTP/GET observation.
 CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost_before_first_binding_get\""),1000020).status==200);
 auto recovered=message("\"type\":\"sourceState\",\"sequence\":1,\"state\":\"tracked\",\"reason\":\"recovered\"");
 replace(recovered,"\"poseContinuityId\":\"c1\"","\"poseContinuityId\":\"c2\"");
 CHECK(r.route("/spatial/input",recovered,1000030).status==200);
 CHECK(r.bindingStates()[0].tracked&&!r.bindingStates()[0].continuityIntact);
 CHECK(r.rebindParticipants(request,"2",1000040).status==200);CHECK(r.inputEpoch()==2);
 CHECK(!r.bindingStates()[0].continuityIntact); // Exact retry is observational.
 CHECK(r.rebindParticipants(participantRebind("2","person1"),"2",1000050).status==200);
 CHECK(r.inputEpoch()==3&&r.bindingStates()[0].continuityIntact&&!r.bindingStates()[0].neutral);
}
void crewNeutralityIncludesOptionalGripControl()
{
 Runtime r;auto grant=prepare();replace(grant,"\"secondaryAction\"","\"secondaryAction\",\"gripAction\"");
 CHECK(r.route("/spatial/prepare",grant,1000000).status==200);r.tick(1000000,true);
 CHECK(r.route("/spatial/input",message("\"type\":\"sourceState\",\"sequence\":0,\"state\":\"tracked\",\"reason\":\"prepared\""),1000000).status==200);
 r.addFrame(frame(1));CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(!r.bindingStates()[0].neutral);
 CHECK(r.route("/spatial/input",edgeAt("primary-neutral","released",0,999990),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("secondary-neutral","released",1,999990,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("grip-neutral","released",2,999990,0,"grip"),1000000).status==200);
 CHECK(r.bindingStates()[0].neutral);
 CHECK(r.route("/spatial/input",edgeAt("grip-held","pressed",3,999990,0,"grip"),1000000).status==200);
 CHECK(!r.bindingStates()[0].neutral);
 CHECK(r.route("/spatial/input",edgeAt("grip-released","released",4,999990,0,"grip"),1000000).status==200);
 CHECK(r.bindingStates()[0].neutral);
}
void crewNeutralityRequiresFreshObservedUpAfterAuthorityBoundaries()
{
 Runtime r;ready(r);r.addFrame(frame(1));CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("p-up","released",0,999990),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("s-up","released",1,999990,0,"secondary"),1000000).status==200);
 CHECK(r.bindingStates()[0].neutral);r.tick(1000010,false);
 CHECK(!r.bindingStates()[0].neutral);
 CHECK(r.route("/spatial/input",edgeAt("held-p-up","released",2,1000020),1000020).status==200);
 CHECK(r.route("/spatial/input",edgeAt("held-s-up","released",3,1000020,0,"secondary"),1000020).status==200);
 CHECK(r.bindingStates()[0].neutral);
 // A valid physical DOWN remains held even when gameplay rejects the press.
 CHECK(r.route("/spatial/input",edgeAt("held-down","pressed",4,1000030),1000030).status==409);
 CHECK(!r.bindingStates()[0].neutral);r.tick(1000030,false);CHECK(!r.bindingStates()[0].neutral);
 CHECK(r.route("/spatial/input",edgeAt("held-release","released",5,1000040),1000040).status==200);
 CHECK(r.bindingStates()[0].neutral);
 r.allowParticipantRebind(true);r.setParticipants({"person2"});
 CHECK(r.rebindParticipants(participantRebind("1","person2"),"2",1000050).status==200);
 CHECK(!r.bindingStates()[0].neutral);
}
Frame crewFrame(const Runtime& input,uint64_t id=1,bool party=true,uint64_t turn=0,uint64_t intent=1,uint64_t roster=1)
{
 Frame f=frame(id);f.captureUs=1000010;f.defense.enabled=true;f.defense.threat=9;
 f.layout=ScintillaDefense::Layout::create(2048,1024,true,false,true);
 f.crewEnabled=true;f.partyEnabled=party;f.crewRaceEpoch=1;f.crewDriverTurnId=turn;
 f.crewIntentRevision=intent;f.crewInputEpoch=input.inputEpoch();f.crewRosterRevision=roster;
 f.targets[0].combat.instance=70;f.targets[0].combat.eligible=true;return f;
}
void crewInputReady(Runtime& r,bool party=true,bool present=true)
{
 ready(r);r.setMissionLayoutRequired(true);r.setCrewContext(true,party,1,0,1,0,1);
 r.setCombatContext(true,7,{},1);r.setDefenseContext(true,{});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});
 r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 if(present){r.addFrame(crewFrame(r,1,party));CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000210).status==200);}
}
std::string crewEdge(const std::string& id,const std::string& kind,unsigned seq,int64_t time=1000100,const std::string& control="secondary")
{
 auto value=edgeAt(id,kind,seq,time,0,control);
 if(kind=="pressed"){replace(value,"\"u\":0.5","\"u\":0.555");replace(value,"\"v\":0.5","\"v\":0.925");}
 return value;
}
void crewToolPress(Runtime& r,const std::string& tag="party",int64_t now=1000210)
{
 CHECK(r.route("/spatial/input",crewEdge(tag+"-up","released",0,1000050),now).status==200);
 CHECK(r.route("/spatial/input",crewEdge(tag+"-down","pressed",1),now).status==200);
}
void crewLayoutV3ReservesExactPartyCircleAndPreservesLegacyMasks()
{
 using ScintillaDefense::Context;using ScintillaDefense::Layout;
 CHECK(int(Context::ReservedHUD)==5&&int(Context::PartyBoost)==6);
 for(const auto size:{std::make_pair(256u,256u),std::make_pair(768u,768u),std::make_pair(1536u,1536u),std::make_pair(2048u,1024u),std::make_pair(8192u,8192u)}){
  const auto l=Layout::create(size.first,size.second,true,false,true),old=Layout::create(size.first,size.second,true);
  double u,v,r;l.party(&u,&v,&r);const double scale=std::min(size.first,size.second);
  CHECK(l.crew&&l.revision()==3&&std::string(l.profile())=="front_rear_crew_v3");
  CHECK(std::abs((u*size.first-size.first*.5)*2/scale-.22)<1e-12);
  CHECK(std::abs((v*size.second-size.second*.5)*2/scale-.85)<1e-12&&std::abs(r-.03*scale)<1e-12);
  CHECK(l.classify(u,v).context==Context::PartyBoost&&!l.worldRectFree(u,v,u,v));
  CHECK(old.classify(u,v).context==Context::WorldOffense&&old.worldRectFree(u,v,u,v));
  for(unsigned angle=0;angle<32;++angle){const double a=angle*6.283185307179586/32;
   const double dx=std::cos(a)*r/size.first,dy=std::sin(a)*r/size.second;
   CHECK(l.classify(u+.999*dx,v+.999*dy).context==Context::PartyBoost);
   CHECK(l.classify(u+1.001*dx,v+1.001*dy).context==Context::WorldOffense);
  }
  for(unsigned node=0;node<5;++node){double a,b,c,x,y,z;l.node(node,&a,&b,&c);old.node(node,&x,&y,&z);CHECK(a==x&&b==y&&c==z);}
  CHECK(!l.worldRectFree(u+.9*r/size.first,v-.1*r/size.second,u+1.2*r/size.first,v+.1*r/size.second));
  CHECK(l.worldRectFree(u+1.01*r/size.first,v-.01*r/size.second,u+1.1*r/size.first,v+.01*r/size.second));
 }
 const auto square=Layout::create(1536,1536,true,false,true);double u,v,r;square.party(&u,&v,&r);
 CHECK(std::abs(u-.61)<1e-12&&std::abs(v-.925)<1e-12&&std::abs(r-46.08)<1e-12);
}
void crewDisabledContextCannotDisarmLegacyInput()
{
 Runtime r;ready(r);auto f=frame(1);f.targets[0].combat.instance=70;f.targets[0].combat.eligible=true;r.addFrame(f);
 r.setCombatContext(true,7,{},1);r.setVisibilityOracle([](const Frame&,const Target&,const Vec3&,double){return Visibility::Visible;});
 r.setLiveTargetOracle([](uint64_t,uint64_t){return true;});CHECK(r.route("/spatial/presentation",presentation(),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("legacy-up","released",0,999970,0,"secondary"),1000000).status==200);
 CHECK(r.route("/spatial/input",edgeAt("legacy-down","pressed",1,999980,0,"secondary"),1000000).status==200);
 const auto before=r.beamProofs(1000000)[0];CHECK(before.visible);
 r.setCrewContext(false,false,99,100,101,102,103);
 const auto after=r.beamProofs(1000000)[0];CHECK(after.visible&&after.controlEpoch==before.controlEpoch&&r.partyBeams(1000000).empty());
}
void crewPartyPrimaryAndDisabledToolNeverReachWorldGameplay()
{
 for(bool enabled:{false,true})for(const std::string control:{"primary","secondary"}){
  Runtime input;crewInputReady(input,enabled,false);auto f=crewFrame(input,1,enabled);Vec3 d;
  CHECK(Runtime::projectionDirection(f.projection,.555,.925,&d));f.targets[0].center=Vec3(10*d.x,10*d.y,10*d.z);
  input.addFrame(f);unsigned queries=0;input.setVisibilityOracle([&](const Frame&,const Target&,const Vec3&,double){++queries;return Visibility::Visible;});
  CHECK(input.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000210).status==200);
  CHECK(input.route("/spatial/input",crewEdge("up","released",0,1000050,control),1000210).status==200);
  CHECK(input.route("/spatial/input",crewEdge("down","pressed",1,1000100,control),1000210).status==200);
  CHECK(input.events().back().result==(enabled&&control=="secondary"?"hud":"non_targetable_surface"));
  CHECK(queries==0&&input.drainCombatShots().empty()&&!input.beamProofs(1000210)[0].held);
  const auto b=input.partyBeams(1000210);CHECK(b.size()==1&&b[0].visible==(enabled&&control=="secondary"));
 }
}
void crewPartyPressAuthorityAndAdmissionRemainImmutableAcrossAimsAndProposal()
{
 Runtime r;crewInputReady(r);crewToolPress(r);const auto before=r.partyBeams(1000210)[0];
 CHECK(before.held&&before.visible&&!before.pending&&before.admittedTick==7&&before.serial>before.releaseSerial);
 CHECK(before.authority.race==1&&before.authority.inputEpoch==1&&before.authority.driverTurnId==0&&before.authority.rosterRevision==1);
 CHECK(before.authority.routeRevision=="route"&&before.authority.projectionContextId=="projection"&&before.capturedProposalId==0);
 r.setCrewContext(true,true,1,0,1,17,1);r.setCombatContext(true,99,{},1);
 auto f=crewFrame(r,2);f.crewProposalId=17;f.defense.threat=99;r.addFrame(f);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000300,1000500),1000510).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(1000400,.555,.925,10),1000510).status==200);r.tick(1000510,true);
 const auto after=r.partyBeams(1000510)[0];
 CHECK(after.visible&&after.serial==before.serial&&after.controlEpoch==before.controlEpoch&&after.admittedTick==7);
 CHECK(after.authority==before.authority&&after.capturedProposalId==0);
}
void crewPartyInitialPendingIsOnlyTheActualReliablePressQuery()
{
 Runtime r;crewInputReady(r,true,false);r.addFrame(crewFrame(r));crewToolPress(r);
 auto before=r.partyBeams(1000210)[0];CHECK(before.held&&before.pending&&!before.visible&&!before.interrupted&&before.admittedTick==7);
 r.setCombatContext(true,9,{},1);r.tick(1000300,true);CHECK(r.partyBeams(1000300)[0].pending);
 CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000310).status==200);
 auto after=r.partyBeams(1000310)[0];CHECK(after.visible&&!after.pending&&after.serial==before.serial&&after.controlEpoch==before.controlEpoch&&after.admittedTick==7);
}
void crewHistoricalPartyProposalIsCapturedAndRetiredIdentityIsRejected()
{
 for(uint64_t historical:{0u,16u,17u}){
  Runtime r;crewInputReady(r,true,false);r.setCrewContext(true,true,1,0,1,17,1);
  auto f=crewFrame(r);f.crewProposalId=historical;r.addFrame(f);
  CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000210).status==200);crewToolPress(r);
  const auto b=r.partyBeams(1000210)[0];
  if(historical==16)CHECK(!b.visible&&!b.pending&&r.events().back().result=="party_proposal_changed");
  else CHECK(b.visible&&b.capturedProposalId==historical);
 }
}
void crewPartyPendingNegativeCannotBecomePendingOrPositiveForTheSamePress()
{
 Runtime r;crewInputReady(r,true,false);auto old=crewFrame(r);old.crewIntentRevision=2;r.addFrame(old);crewToolPress(r);
 CHECK(r.partyBeams(1000210)[0].pending);
 CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000310).status==200);
 auto failed=r.partyBeams(1000310)[0];CHECK(!failed.pending&&!failed.visible&&failed.interrupted);
 auto current=crewFrame(r,2);r.addFrame(current);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000400,1000600),1000610).status==200);
 CHECK(r.route("/spatial/input",feedbackAim(1000500,.555,.925,10),1000610).status==200);r.tick(1000610,true);
 const auto repaired=r.partyBeams(1000610)[0];CHECK(!repaired.pending&&!repaired.visible&&repaired.interrupted&&repaired.serial==failed.serial);
}
void crewPartyCoalescedDepartureAndReturnStayInterruptedBeforeAndAfterProof()
{
 for(bool pending:{false,true}){
  Runtime r;crewInputReady(r,true,!pending);if(pending)r.addFrame(crewFrame(r));crewToolPress(r);
  const auto epoch=r.partyBeams(1000210)[0].controlEpoch;
  ScintillaCrew::PartyFeedback view;view.participantId="person1";view.controlEpoch=epoch;view.progress=1;view.consenting=true;r.setPartyFeedback({view});
  CHECK(r.route("/spatial/input",feedbackAim(1000120,.5,.5,10),1000220).status==200);
  CHECK(r.route("/spatial/input",feedbackAim(1000130,.555,.925,11),1000220).status==200);
  if(pending)CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000230).status==200);
  r.tick(1000230,true);const auto b=r.partyBeams(1000230)[0];
  CHECK(b.interrupted&&!b.pending&&!b.visible&&b.controlEpoch==epoch);
  r.setPartyFeedback({view});CHECK(r.reticles()[0].partyFeedback.reason==ScintillaCrew::PartyReason::ReleaseRequired&&!r.reticles()[0].partyFeedback.consenting);
 }
}
void crewOldAndMixedFramesRejectEveryAuthorityDimensionWithoutRevokingGrant()
{
 for(unsigned mutation=0;mutation<8;++mutation){
  Runtime r;crewInputReady(r,true,false);auto bad=crewFrame(r);
  if(mutation==0)bad.layout=ScintillaDefense::Layout::create(2048,1024,true);
  if(mutation==1)bad.crewEnabled=false;
  if(mutation==2)bad.partyEnabled=false;
  if(mutation==3)++bad.crewRaceEpoch;
  if(mutation==4)++bad.crewDriverTurnId;
  if(mutation==5)++bad.crewIntentRevision;
  if(mutation==6)++bad.crewInputEpoch;
  if(mutation==7)++bad.crewRosterRevision;
  r.addFrame(bad);CHECK(r.route("/spatial/presentation",racePresentation(1,1000020,1000200),1000210).status==200);crewToolPress(r);
  CHECK(r.events().back().result=="layout_unavailable"&&r.prepared()&&!r.partyBeams(1000210)[0].visible);
 }
 Runtime r;crewInputReady(r,true,false);r.addFrame(crewFrame(r));auto bad=crewFrame(r,2);++bad.crewDriverTurnId;r.addFrame(bad);
 auto proof=racePresentation(1,1000020,1000200);replace(proof,"\"presentedUntilUs\":1000200}","\"presentedUntilUs\":1000100},{\"frameId\":2,\"presentedFromUs\":1000100,\"presentedUntilUs\":1000200}");
 CHECK(r.route("/spatial/presentation",proof,1000210).status==200);crewToolPress(r);
 CHECK(r.events().back().result=="layout_unavailable"&&!r.partyBeams(1000210)[0].visible&&r.prepared());
}
void crewSameTurnRebindCannotUseAnOldInputEpochFrame()
{
 Runtime r;crewInputReady(r);r.allowParticipantRebind(true);r.setParticipants({"person1"});
 CHECK(r.rebindParticipants(participantRebind("1","person1"),"2",1000500).status==200);
 r.setCrewContext(true,true,1,0,1,0,2);r.tick(1000600,true);
 CHECK(r.route("/spatial/presentation",racePresentation(1,1000620,1000900),1000910).status==200);
 CHECK(r.route("/spatial/input",crewEdge("up2","released",0,1000650),1000910).status==200);
 CHECK(r.route("/spatial/input",crewEdge("down2","pressed",1,1000700),1000910).status==200);
 CHECK(!r.partyBeams(1000910)[0].visible&&r.events().back().result=="layout_unavailable");
 auto fresh=crewFrame(r,2,true,0,1,2);fresh.captureUs=1000920;r.addFrame(fresh);
 CHECK(r.route("/spatial/presentation",racePresentation(2,1000950,1001200),1001210).status==200);
 CHECK(r.route("/spatial/input",crewEdge("up3","released",2,1001000),1001210).status==200);
 CHECK(r.route("/spatial/input",crewEdge("down3","pressed",3,1001100),1001210).status==200);
 CHECK(r.partyBeams(1001210)[0].visible&&r.partyBeams(1001210)[0].authority.inputEpoch==2);
}
void crewRealUpSerialSurvivesControlClearingButNotAuthorityChange()
{
 Runtime r;crewInputReady(r);crewToolPress(r);const auto press=r.partyBeams(1000210)[0];
 r.tick(1061000,true);auto cleared=r.partyBeams(1061000)[0];
 CHECK(!cleared.held&&!cleared.neutral&&!cleared.pending&&cleared.releaseSerial==press.releaseSerial);
 CHECK(r.route("/spatial/input",crewEdge("real-up","released",2,1061090),1061100).status==200);
 const auto released=r.partyBeams(1061100)[0];CHECK(released.neutral&&released.releaseSerial>press.serial);
 r.setCrewContext(true,true,1,1,1,0,1);auto boundary=r.partyBeams(1061100)[0];CHECK(!boundary.neutral&&boundary.releaseSerial==0);
 CHECK(r.route("/spatial/input",crewEdge("old-up","released",3,1061095),1061110).status==409);
 CHECK(!r.partyBeams(1061110)[0].neutral);
}
ScintillaJSON::J crewPartyFeedbackJSON(const Runtime& input)
{
 const auto text=input.stateJSON();const auto root=ScintillaJSON::Parser(text).parse();
 const auto& feedback=ScintillaJSON::at(root,"feedback").array;CHECK(feedback.size()==1);
 return ScintillaJSON::at(feedback[0],"crewPartyTool");
}
void crewPartySourceJSONUsesOnlyCurrentFiniteFeedbackAndClearsConsent()
{
 using namespace ScintillaJSON;
 Runtime legacy;ready(legacy);const auto legacyText=legacy.stateJSON();const auto old=Parser(legacyText).parse();
 CHECK(!at(old,"feedback").array[0].object.count("crewPartyTool"));
 for(bool lost:{false,true}){
  Runtime r;crewInputReady(r);crewToolPress(r);ScintillaCrew::PartyFeedback view;
  view.participantId="person1";view.controlEpoch=r.reticles()[0].toolEpoch;
  view.reason=ScintillaCrew::PartyReason::Ready;view.progress=.625;r.setPartyFeedback({view});
  auto feedback=crewPartyFeedbackJSON(r);CHECK(feedback.object.size()==4);
  CHECK(at(feedback,"reason").string=="ready"&&at(feedback,"controlEpoch").kind==J::String);
  CHECK(at(feedback,"controlEpoch").string==std::to_string(view.controlEpoch));
  CHECK(at(feedback,"progress").kind==J::Number&&at(feedback,"progress").number==.625&&!at(feedback,"consenting").boolean);
  view.progress=1;view.consenting=true;r.setPartyFeedback({view});feedback=crewPartyFeedbackJSON(r);
  CHECK(at(feedback,"progress").number==1&&at(feedback,"consenting").boolean);
  if(lost)CHECK(r.route("/spatial/input",message("\"type\":\"cancel\",\"sequence\":1,\"reason\":\"lost\""),1000220).status==200);
  else CHECK(r.route("/spatial/input",crewEdge("feedback-up","released",2,1000150),1000220).status==200);
  r.setPartyFeedback({view});feedback=crewPartyFeedbackJSON(r);
  CHECK(at(feedback,"progress").number==0&&!at(feedback,"consenting").boolean&&at(feedback,"controlEpoch").string=="0");
  CHECK(r.prepared()); // A normal UP/tracking rejection never revokes the grant.
 }
 Runtime r;crewInputReady(r);crewToolPress(r);ScintillaCrew::PartyFeedback view;
 view.participantId="person1";view.controlEpoch=r.reticles()[0].toolEpoch;view.progress=1;view.consenting=true;
 for(unsigned fault=0;fault<5;++fault){auto invalid=view;
  if(fault==0)invalid.participantId="other";
  if(fault==1)++invalid.controlEpoch;
  if(fault==2)invalid.progress=std::numeric_limits<double>::quiet_NaN();
  if(fault==3)invalid.progress=-.01;
  if(fault==4)invalid.progress=1.01;
  r.setPartyFeedback({invalid});const auto feedback=crewPartyFeedbackJSON(r);
  CHECK(at(feedback,"progress").number==0&&!at(feedback,"consenting").boolean);
 }
 r.setPartyFeedback({view});auto copy=r.reticles()[0];copy.valid=false;
 CHECK(!Runtime::partyFeedbackForRendering(copy).consenting&&Runtime::partyFeedbackForRendering(copy).progress==0);
 copy=r.reticles()[0];copy.intent.context=ScintillaDefense::Context::WorldOffense;
 CHECK(!Runtime::partyFeedbackForRendering(copy).consenting);
 copy.partyFeedback.reason=ScintillaCrew::PartyReason::ReleaseRequired;
 const auto release=Runtime::partyFeedbackForRendering(copy);
 CHECK(release.reason==ScintillaCrew::PartyReason::ReleaseRequired&&!release.consenting&&release.progress==0);
 copy=r.reticles()[0];copy.u=std::numeric_limits<double>::infinity();CHECK(!Runtime::partyFeedbackForRendering(copy).consenting);
 view.progress=.5;view.consenting=true;r.setPartyFeedback({view});CHECK(!at(crewPartyFeedbackJSON(r),"consenting").boolean);
}
struct TestCase {const char* name;void (*run)();};
const TestCase cases[]={
#define SCINTILLA_CASE(name) {#name,name},
#include "scintilla_input_cases.inc"
#undef SCINTILLA_CASE
};
}
int main(int argc,char** argv)
{
 if(argc==2&&std::string(argv[1])=="--list")
 {for(const auto& test:cases)std::cout<<test.name<<'\n';return EXIT_SUCCESS;}
 const bool selected=argc==3&&std::string(argv[1])=="--case";
 if(argc!=1&&!selected)
 {std::cerr<<"Usage: scintilla_input_regression [--list | --case NAME]\n";return 2;}
 bool found=false;
 for(const auto& test:cases)
 {
  if(selected&&test.name!=std::string(argv[2]))continue;
  found=true;std::cout<<"[ RUN ] "<<test.name<<std::endl;
  test.run();std::cout<<"[ OK  ] "<<test.name<<std::endl;
 }
 if(!found){std::cerr<<"Unknown regression case: "<<argv[2]<<'\n';return 2;}
 return EXIT_SUCCESS;
}
