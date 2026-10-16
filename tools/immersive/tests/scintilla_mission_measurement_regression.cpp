// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic pure verifier facts. These tests NEVER emit a qualification report.
#include "modes/immersive_relay/scintilla_mission_measurement.hpp"
#include <iostream>
#include <cstdlib>
#include <map>
using namespace ScintillaMissionMeasurement;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
template<class F>void rejected(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
void capsuleWindowRequiresStrictlyMoreThanTwoSeconds(){
 Window w;for(int i=0;i<=60;++i)w.observe(1000000+int64_t(i)*100000/3,120+i*4,15,120,false);
 CHECK(w.best==0);w.observe(3033333,364,15,120,false);CHECK(w.best==15);
}
void tetherWindowAcceptsExactThreeSeconds(){
 Window w;for(int i=0;i<=90;++i)w.observe(1000000+int64_t(i)*100000/3,120+i*4,17,120,true);CHECK(w.best==17);
}
void shortFastSpikeCannotRaiseASustainedEnvelope(){
 Window w;for(int i=0;i<150;++i)w.observe(1000000+int64_t(i)*100000/3,120+i*4,i==80?40:15,120,false);
 CHECK(w.best==15);
}
void laterSustainedSpeedCanRaiseItsOwnBound(){
 Window w;for(int i=0;i<150;++i)w.observe(1000000+int64_t(i)*100000/3,120+i*4,i<70?15:25,120,false);CHECK(w.best==25);
}
void gapsAndClockNonAdvancementCannotAccumulateTime(){
 Window w;for(int i=0;i<200;++i)w.observe(1000000+int64_t(i)*51000,120+i*6,15,120,false);CHECK(w.best==0);
 w=Window();for(int i=0;i<200;++i)w.observe(1000000+int64_t(i)*33000,120,15,120,false);CHECK(w.best==0);
}
void clockAndIdentityDomainsStaySeparate(){
 J id;id.kind=J::String;id.string="18446744073709551615";CHECK(wide(id)==UINT64_MAX);rejected([&]{clock(id);});
 id.string="9007199254740993";CHECK(wide(id)==9007199254740993ULL);CHECK(clock(id)==9007199254740993LL);
 for(const auto& s:{"00","01","-1","1e0","18446744073709551616"}){id.string=s;rejected([&]{wide(id);});}
}
void fullFootprintExcludesReservedHUDAndProjectionEdges(){
 CHECK(!footprint("equirectangular",0,2048,1024,Vec(0,12.83883,50),2)); // cargo circles near the equator
 CHECK(!footprint("equirectangular",0,2048,1024,Vec(0,50,0),2)); // pole footprint cannot be bounded
 CHECK(!footprint("fisheye_domemaster",0,1536,1536,Vec(50,0,0),2)); // clips front hemisphere
 CHECK(!footprint("unknown",0,1536,1536,Vec(0,20,50),2));
}
void resetDoesNotFabricateANewQualifiedWindow(){
 Window w;for(int i=0;i<40;++i)w.observe(1000000+int64_t(i)*33000,120+i*4,15,120,false);w.reset();
 for(int i=0;i<40;++i)w.observe(3000000+int64_t(i)*33000,360+i*4,15,120,false);CHECK(w.best==0);
}

// Native admission parser fixtures contain invented facts and never write files
// or a qualification descriptor. They exercise Evidence, not only its helpers.
struct AdmissionFixture {
 ScintillaMissionAnchors::Manifest manifest;
 std::string fingerprint=R"({"anchorHash":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","trackHash":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","trackAssetsHash":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","shaderHash":"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc","executableHash":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","profileId":"sc4_cargo_three_slots_v1","profileRevision":"1","C":1,"P":2,"laps":1,"F":120,"width":2048,"height":1024,"cubemapSize":1024,"projection":"equirectangular","output":"syphon","near":0.2,"far":1000,"faceFovDeg":90,"domeTiltDeg":0,"cameraMode":"stabilized_chase","camera":[0,35,0,-0.25,0.4,0,0.1,0],"horizonStabilization":true,"capsuleRadius":2,"tetherRadius":0.5,"tetherOffsets":[[-3,5.5,6],[3,5.5,6]],"evaluatedLayoutRevision":2,"nativeBaseSpeed":15})";
 AdmissionFixture(){for(unsigned i=0;i<4;++i){ScintillaMissionAnchors::Group g;g.id="synthetic_"+std::to_string(i);g.activationFraction=.1+.2*i;g.expiryFraction=g.activationFraction+.19;g.laps.push_back(0);ScintillaMissionAnchors::Capsule c;c.center.x=30;c.center.z=10;g.capsules.push_back(c);manifest.groups.push_back(g);}}
 J header(const std::string& phase)const {
  std::ostringstream o;o<<std::setprecision(17)<<R"({"type":"header","contract":"tholus.stk-scintilla-measurement/1","schemaVersion":1,"recordingID":"00000000-0000-4000-8000-000000000001","producerUUID":"00000000-0000-4000-8000-000000000002","raceEpoch":"1","provenance":"native_render_depth","fingerprint":)"<<fingerprint<<",\"fingerprintJSON\":"<<quote(fingerprint)<<R"(,"fingerprintHash":"unused_by_pure_fixture","groups":[)";
  for(unsigned i=0;i<4;++i){if(i)o<<',';o<<"{\"id\":"<<quote(manifest.groups[i].id)<<",\"activationFraction\":"<<manifest.groups[i].activationFraction<<",\"expiryFraction\":"<<manifest.groups[i].expiryFraction<<R"(,"laps":[0],"capsules":[{"center":[30,0,10],"radius":2}]})";}
  o<<R"(],"limits":{"maxFrames":18000,"maxPending":8,"maxGapUs":50000,"liveProofMaxAgeUs":50000},"phasePolicy":"diagnostic_constant_per_trace","phase":)"<<quote(phase)<<R"(,"actualLayoutRevision":0})";return Parser(o.str()).parse();
 }
 J frame(unsigned sequence,unsigned group,unsigned local,const std::string& phase,int64_t delay)const {
  const int64_t capture=1000000+int64_t(sequence)*33333,tick=sequence*4;
  const double fraction=std::min(manifest.groups[group].expiryFraction-.001,manifest.groups[group].activationFraction-.003+local*.001);
  std::ostringstream o;o<<std::setprecision(17)<<R"({"type":"frame","index":")"<<sequence<<R"(","frameID":")"<<sequence<<R"(","captureUs":")"<<capture<<R"(","readbackUs":")"<<capture+delay<<R"(","readbackTick":")"<<tick+(delay*120+999999)/1000000<<R"(","tick":")"<<tick<<R"(","officialLap":0,"fraction":)"<<fraction<<R"(,"speed":15,"minimumSpeed":15,"maxSpeed":25,"scenario":)"<<quote(group==1?"zipper":"normal")<<R"(,"speedIncreaseTicks":[)"<<(group==1?10:0)<<R"(,0,0,0,0,0],"active":true,"continuous":true,"published":true,"camera":{"position":[0,0,0],"right":[1,0,0],"up":[0,1,0],"forward":[0,0,1]},"kartPosition":[)"<<sequence*.4<<R"(,0,0],"tetherCandidates":[[8,0,3],[14,0,3]],"objects":[{"kind":"capsule","group":)"<<group<<R"(,"capsule":0,"side":-1,"phase":)"<<quote(phase)<<R"(,"center":[30,0,10],"radius":2,"range":true,"rectangleFree":true,"activation":)"<<(local==3?"true":"false")<<R"(,"activationUs":")"<<(local==3?capture-10000:0)<<R"(","inWindow":)"<<(local>=3?"true":"false")<<R"(,"idealPixels":100,"admissiblePixels":100,"visiblePixels":100,"status":"ready"})";
  for(unsigned side=0;side<2;++side)o<<R"(,{"kind":"tether","group":-1,"capsule":-1,"side":)"<<side<<R"(,"phase":)"<<quote(phase=="shell"?"awaiting_presentation":"recoverable")<<R"(,"center":[)"<<(side?14:8)<<R"(,0,3],"radius":0.5,"range":true,"rectangleFree":true,"activation":false,"activationUs":"0","inWindow":true,"idealPixels":20,"admissiblePixels":20,"visiblePixels":20,"status":"ready"})";
  o<<"]}";return Parser(o.str()).parse();
 }
 bool qualifies(int64_t delay,std::string* reason)const {
  Evidence e(manifest,fingerprint);for(const auto& phase:{"shell","open"}){e.beginTrace();e.header(header(phase));unsigned sequence=0;for(unsigned g=0;g<4;++g)for(unsigned i=0;i<140;++i)e.frame(frame(++sequence,g,i,phase,delay));e.footer(Parser(R"({"type":"footer","frameCount":"560","complete":true,"reason":"","naturalFinish":true,"officialLap":1})").parse());}
  return e.qualified(nullptr,reason);
 }
};
void availabilityIntervalsHaveInclusiveExpiryAndRequireReadback(){
 CHECK(capsuleProofAvailable(100000,115000,150000));CHECK(!capsuleProofAvailable(100000,115000,150001));CHECK(!capsuleProofAvailable(100000,115000,114999));
 CHECK(capsuleProofsOverlap(100000,150000));CHECK(!capsuleProofsOverlap(100000,150001));
}
void capsuleDurationStartsAtReadbackButTetherRetainsCaptureContract(){
 Window cap,tether;for(int i=0;i<=60;++i){const int64_t us=1000000+int64_t(i)*100000/3;cap.observe(us,120+i*4,15,120,false,us+33333,124+i*4);}
 cap.observe(3033333,364,15,120,false,3066666,368);CHECK(cap.best==0);cap.observe(3066666,368,15,120,false,3099999,372);CHECK(cap.best==15);
 for(int i=0;i<=90;++i){const int64_t us=1000000+int64_t(i)*100000/3;tether.observe(us,120+i*4,15,120,true,us+90000,132+i*4);}CHECK(tether.best==15);
}
void actualEvidenceRejectsSlowAndIndividuallyFreshButGappedQueries(){
 AdmissionFixture f;std::string reason;CHECK(f.qualifies(15000,&reason));CHECK(reason.empty());CHECK(f.qualifies(16666,&reason));
 CHECK(!f.qualifies(20000,&reason));CHECK(reason=="measurement_capsule_window_missing");CHECK(!f.qualifies(90000,&reason));CHECK(reason=="measurement_capsule_window_missing");
}
void actualEvidenceRejectsLegacyAndImpossibleAvailabilityFields(){
 AdmissionFixture f;auto missing=[](J&j,const std::string&key){j.object.erase(key);};
 for(const auto&key:{"readbackUs","readbackTick"}){Evidence e(f.manifest,f.fingerprint);e.header(f.header("shell"));auto row=f.frame(1,0,0,"shell",5000);missing(row,key);rejected([&]{e.frame(row);});}
 {Evidence e(f.manifest,f.fingerprint);e.header(f.header("shell"));auto row=f.frame(1,0,0,"shell",5000);J text;text.kind=J::String;text.string="1";row.object["readbackUs"]=text;rejected([&]{e.frame(row);});}
 {Evidence e(f.manifest,f.fingerprint);e.header(f.header("shell"));auto row=f.frame(1,0,0,"shell",5000);row.object["objects"].array.front().object.erase("activationUs");rejected([&]{e.frame(row);});}
}
void crewHeaderRequiresDistinctClosedV3Contract(){
 for(const auto* profile:{"sc5_crew_standard_v1","sc5_crew_party_v1"}){
  AdmissionFixture f;J config=Parser(f.fingerprint).parse();
  config.object["profileId"]=Parser(quote(profile)).parse();config.object["evaluatedLayoutRevision"]=Parser("3").parse();
  config.object["crewRulesRevision"]=Parser("\"1\"").parse();config.object["zoneManifestHash"]=Parser(quote(std::string(64,'e'))).parse();
  config.object["partyEnabled"]=Parser(std::string(profile)=="sc5_crew_party_v1"?"true":"false").parse();
  for(const auto& pair:std::map<std::string,int>{{"partyCost",15},{"partyGain",1},{"partyHoldTicks",90},{"partyWindowTicks",360},{"partyDurationTicks",60},{"partyFadeTicks",30},{"partyCooldownTicks",720}})config.object[pair.first]=Parser(std::to_string(pair.second)).parse();
  f.fingerprint=serialize(config);auto header=f.header("shell");header.object["contract"]=Parser("\"tholus.stk-scintilla-measurement/2\"").parse();header.object["schemaVersion"]=Parser("2").parse();header.object["actualLayoutRevision"]=Parser("3").parse();
  {Evidence e(f.manifest,f.fingerprint);e.header(header);}
  for(const auto& pair:std::map<std::string,std::string>{{"contract","\"tholus.stk-scintilla-measurement/1\""},{"schemaVersion","1"},{"actualLayoutRevision","0"}}){auto changed=header;changed.object[pair.first]=Parser(pair.second).parse();Evidence e(f.manifest,f.fingerprint);rejected([&]{e.header(changed);});}
  auto changed=header;changed.object["fingerprint"].object["unknown"]=Parser("1").parse();const auto raw=serialize(changed.object["fingerprint"]);changed.object["fingerprintJSON"]=Parser(quote(raw)).parse();Evidence e(f.manifest,raw);rejected([&]{e.header(changed);});
  auto badTilt=header;badTilt.object["fingerprint"].object["domeTiltDeg"]=Parser("120").parse();const auto tilted=serialize(badTilt.object["fingerprint"]);badTilt.object["fingerprintJSON"]=Parser(quote(tilted)).parse();Evidence tiltEvidence(f.manifest,tilted);rejected([&]{tiltEvidence.header(badTilt);});
 }
}

}
int main(int argc,char**argv){const std::map<std::string,void(*)()> cases={
 {"capsuleWindowRequiresStrictlyMoreThanTwoSeconds",capsuleWindowRequiresStrictlyMoreThanTwoSeconds},
 {"tetherWindowAcceptsExactThreeSeconds",tetherWindowAcceptsExactThreeSeconds},
 {"shortFastSpikeCannotRaiseASustainedEnvelope",shortFastSpikeCannotRaiseASustainedEnvelope},
 {"laterSustainedSpeedCanRaiseItsOwnBound",laterSustainedSpeedCanRaiseItsOwnBound},
 {"gapsAndClockNonAdvancementCannotAccumulateTime",gapsAndClockNonAdvancementCannotAccumulateTime},
 {"clockAndIdentityDomainsStaySeparate",clockAndIdentityDomainsStaySeparate},
 {"fullFootprintExcludesReservedHUDAndProjectionEdges",fullFootprintExcludesReservedHUDAndProjectionEdges},
 {"resetDoesNotFabricateANewQualifiedWindow",resetDoesNotFabricateANewQualifiedWindow},
{"availabilityIntervalsHaveInclusiveExpiryAndRequireReadback",availabilityIntervalsHaveInclusiveExpiryAndRequireReadback},
{"capsuleDurationStartsAtReadbackButTetherRetainsCaptureContract",capsuleDurationStartsAtReadbackButTetherRetainsCaptureContract},
{"actualEvidenceRejectsSlowAndIndividuallyFreshButGappedQueries",actualEvidenceRejectsSlowAndIndividuallyFreshButGappedQueries},
{"actualEvidenceRejectsLegacyAndImpossibleAvailabilityFields",actualEvidenceRejectsLegacyAndImpossibleAvailabilityFields},
{"crewHeaderRequiresDistinctClosedV3Contract",crewHeaderRequiresDistinctClosedV3Contract}};
 if(argc!=3||std::string(argv[1])!="--case"||!cases.count(argv[2]))return 2;cases.find(argv[2])->second();return 0;}
