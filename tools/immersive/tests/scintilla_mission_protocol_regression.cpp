// SPDX-License-Identifier: GPL-3.0-or-later
// Pure production protocol/receipt regressions; no engine, GPU, assets or I/O.
#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace ScintillaMissionProtocol;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
const std::string id="00000000-0000-4000-8000-000000000001";
std::string identityJSON(){return "{\"producerUUID\":\""+id+"\",\"raceEpoch\":\"18446744073709551615\",\"intentRevision\":\"0\",\"missionRevision\":\"1\",\"rosterRevision\":\"9007199254740993\",\"inputSessionID\":\"\",\"inputEpoch\":\"0\",\"routeRevision\":\"\",\"projectionContextId\":\"\",\"anchorManifestHash\":\"\",\"introManifestHash\":\"\",\"layoutProfile\":\"front_rear_cargo_v2\",\"layoutRevision\":\"2\"}";}
std::string body(const std::string& op){return "{\"contract\":\"tholus.stk-scintilla-mission/1\",\"schemaVersion\":1,\"requestId\":\""+id+"\",\"profileId\":\"sc4_cargo_three_slots_v1\",\"profileRevision\":\"1\",\"expected\":"+identityJSON()+(op=="intent"?",\"required\":true":op=="intro-complete"?",\"variantId\":\"C1\",\"completion\":\"played\",\"playbackID\":\""+id+"\"":"")+"}";}
void replace(std::string& s,const std::string& old,const std::string& value){const auto at=s.find(old);CHECK(at!=std::string::npos);s.replace(at,old.size(),value);}
template<class F>void rejected(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
void canonicalIdentitiesPreserveWideIntegers(){
 auto r=request(body("prepare"),"prepare");CHECK(r.expected.raceEpoch=="18446744073709551615"&&r.expected.rosterRevision=="9007199254740993");
 for(const auto& bad:{"\"00\"","\"01\"","\"-1\"","\"1e0\"","\"18446744073709551616\"","1","\" 1\""}){
  auto b=body("prepare");replace(b,"\"raceEpoch\":\"18446744073709551615\"","\"raceEpoch\":"+std::string(bad));rejected([&]{request(b,"prepare");});
 }
 auto b=body("prepare");replace(b,"front_rear_cargo_v2","front_rear_readonly_v1");rejected([&]{request(b,"prepare");});
 b=body("prepare");replace(b,"\"layoutRevision\":\"2\"","\"layoutRevision\":\"1\"");rejected([&]{request(b,"prepare");});
}
void requestsAreClosedAndOperationTyped(){
 CHECK(request(body("intent"),"intent").required);CHECK(request(body("intro-complete"),"intro-complete").completion=="played");
 for(const auto& op:{"intent","prepare","intro-complete"}){
  auto b=body(op);b.insert(1,"\"unknown\":1,");rejected([&]{request(b,op);});
  b=body(op);b.insert(1,"\"schemaVersio\\u006e\":1,");rejected([&]{request(b,op);});
 }
 auto b=body("intent");replace(b,"\"required\":true","\"required\":\"true\"");rejected([&]{request(b,"intent");});
 b=body("intro-complete");replace(b,"played","auto_skipped");rejected([&]{request(b,"intro-complete");});
 b=body("intro-complete");replace(b,"\"playbackID\":\""+id+"\"","\"playbackID\":\"\"");rejected([&]{request(b,"intro-complete");});
 rejected([&]{request(std::string(16385,' '),"prepare");});
}
void recentRequestRetriesPrecedeIdentityAndNeverExhaustSession(){
 Receipts receipts;auto r=request(body("intent"),"intent");CHECK(receipts.check(r)==Receipts::New);CHECK(receipts.commit(r));CHECK(receipts.check(r)==Receipts::Duplicate);
 auto modified=r;modified.fingerprint+="different";CHECK(receipts.check(modified)==Receipts::Conflict);
 // Intent may have moved its revision. The receipt still resolves first.
 Identity current=r.expected;current.intentRevision="1";CHECK(!(current==r.expected));CHECK(receipts.check(r)==Receipts::Duplicate);
 for(unsigned i=0;i<4096;++i){Request next=r;std::ostringstream serial;serial<<std::hex<<std::setfill('0')<<std::setw(12)<<i+2;
  next.requestId="00000000-0000-4000-8000-"+serial.str();next.fingerprint=next.requestId;CHECK(receipts.check(next)==Receipts::New);CHECK(receipts.commit(next));CHECK(receipts.size()<=64);
 }
 CHECK(receipts.size()==64&&receipts.check(r)==Receipts::New);CHECK(!(current==r.expected));
 receipts.reset();CHECK(receipts.size()==0);
}
void everyBindingDimensionParticipatesInAdmission(){
 auto a=request(body("prepare"),"prepare").expected;
 const std::array<std::string Identity::*,13> fields={{&Identity::producerUUID,&Identity::raceEpoch,&Identity::intentRevision,&Identity::missionRevision,&Identity::rosterRevision,&Identity::inputSessionID,&Identity::inputEpoch,&Identity::routeRevision,&Identity::projectionContextId,&Identity::anchorManifestHash,&Identity::introManifestHash,&Identity::layoutProfile,&Identity::layoutRevision}};
 for(auto field:fields){auto b=a;b.*field+="x";CHECK(!(a==b));}
}
void introReceiptIsImmutableAcrossRebindAndRejectsOtherRace(){
 IntroReceipt receipt;const std::string key=id+"|1|"+std::string(64,'a')+"|C1";
 CHECK(receipt.accept(key,"played",id));CHECK(receipt.accept(key,"played",id));
 CHECK(!receipt.accept(key,"operator_skipped",id));CHECK(!receipt.accept(key+"new-race","played",id));
 CHECK(receipt.matches(key));CHECK(!receipt.matches(id+"|2|"+std::string(64,'a')+"|C1"));
 auto identity=request(body("prepare"),"prepare").expected;identity.inputEpoch="2";identity.routeRevision="new-route";identity.rosterRevision="new-roster";
 CHECK(receipt.matches(key)); // Input/route/crew bindings never enter intro key.
}
std::string manifest(){
 std::string variants="";for(unsigned i=0;i<3;++i){if(i)variants+=',';const std::string v=i==0?"C1":i==1?"C2":"C3plus";
  variants+="{\"variantId\":\""+v+"\",\"minCrew\":"+std::to_string(i+1)+",\"maxCrew\":"+std::to_string(i==2?5:i+1)+",\"videoPath\":\""+v+".mp4\",\"videoSHA256\":\""+std::string(64,'a')+"\",\"subtitlesPath\":\""+v+".vtt\",\"subtitlesSHA256\":\""+std::string(64,'b')+"\"}";
 }
 return "{\"contract\":\"tholus.stk-scintilla-intro/1\",\"schemaVersion\":1,\"profileId\":\"sc4_cargo_three_slots_v1\",\"profileRevision\":\"1\",\"locale\":\"it\",\"durationSeconds\":40,\"frameRateNumerator\":30,\"frameRateDenominator\":1,\"frameCount\":1200,\"width\":1920,\"height\":1080,\"license\":\"CC-BY-4.0\",\"author\":\"Tholus\",\"variants\":["+variants+"]}";
}
void introManifestRejectsTimingTraversalAndIntegerWrap(){
 const auto m=introManifest(manifest());CHECK(m.variants[0].minCrew==1&&m.variants[2].maxCrew==5);
 for(unsigned mutation=0;mutation<8;++mutation){auto b=manifest();
  if(mutation==0)replace(b,"\"minCrew\":1","\"minCrew\":4294967297");if(mutation==1)replace(b,"\"maxCrew\":1","\"maxCrew\":4294967297");
  if(mutation==2)replace(b,"C1.mp4","../C1.mp4");if(mutation==3)replace(b,"C1.vtt","/C1.vtt");
  if(mutation==4)replace(b,"\"frameCount\":1200","\"frameCount\":1201");if(mutation==5)replace(b,"\"durationSeconds\":40","\"durationSeconds\":39");
  if(mutation==6)replace(b,"C2","C1");if(mutation==7)replace(b,std::string(64,'a'),std::string(64,'A'));
  rejected([&]{introManifest(b);});
 }
 CHECK(std::string(variantForCrew(1))=="C1"&&std::string(variantForCrew(2))=="C2"&&std::string(variantForCrew(5))=="C3plus");
}
void crewIntroRequiresProfileRulesLayoutAndSixPanels(){
 for(const auto* profile:{"sc5_crew_standard_v1","sc5_crew_party_v1"}){
  J value=Parser(manifest()).parse();value.object["contract"]=Parser("\"tholus.stk-scintilla-intro/2\"").parse();value.object["schemaVersion"]=Parser("2").parse();
  value.object["profileId"]=Parser(quote(profile)).parse();value.object["crewRulesRevision"]=Parser("\"1\"").parse();value.object["layoutProfile"]=Parser("\"front_rear_crew_v3\"").parse();value.object["layoutRevision"]=Parser("\"3\"").parse();
  for(auto& variant:value.object["variants"].array){J panels;panels.kind=J::Array;for(unsigned i=0;i<6;++i)panels.array.push_back(Parser("{\"path\":\"panel_"+std::to_string(i)+".png\",\"sha256\":"+quote(std::string(64,'c'))+"}").parse());variant.object["panels"]=panels;}
  CHECK(introManifest(serialize(value),profile).variants[2].maxCrew==5);
  rejected([&]{introManifest(serialize(value));});
  rejected([&]{introManifest(serialize(value),std::string(profile)=="sc5_crew_party_v1"?"sc5_crew_standard_v1":"sc5_crew_party_v1");});
  for(const auto* field:{"crewRulesRevision","layoutProfile","layoutRevision"}){auto changed=value;changed.object.erase(field);rejected([&]{introManifest(serialize(changed),profile);});}
  auto changed=value;changed.object["variants"].array[0].object.erase("panels");rejected([&]{introManifest(serialize(changed),profile);});
 }
 auto v3=Parser(identityJSON()).parse();v3.object["layoutProfile"]=Parser("\"front_rear_crew_v3\"").parse();v3.object["layoutRevision"]=Parser("\"3\"").parse();CHECK(identity(v3).layoutRevision=="3");
 v3.object["layoutRevision"]=Parser("\"2\"").parse();rejected([&]{identity(v3);});
}
struct Case{const char* name;void(*run)();};const Case cases[]={
 {"canonicalIdentitiesPreserveWideIntegers",canonicalIdentitiesPreserveWideIntegers},{"requestsAreClosedAndOperationTyped",requestsAreClosedAndOperationTyped},
 {"recentRequestRetriesPrecedeIdentityAndNeverExhaustSession",recentRequestRetriesPrecedeIdentityAndNeverExhaustSession},{"everyBindingDimensionParticipatesInAdmission",everyBindingDimensionParticipatesInAdmission},
 {"introReceiptIsImmutableAcrossRebindAndRejectsOtherRace",introReceiptIsImmutableAcrossRebindAndRejectsOtherRace},{"introManifestRejectsTimingTraversalAndIntegerWrap",introManifestRejectsTimingTraversalAndIntegerWrap},
 {"crewIntroRequiresProfileRulesLayoutAndSixPanels",crewIntroRequiresProfileRulesLayoutAndSixPanels}};
}
int main(int argc,char** argv){bool found=false;for(const auto& c:cases){if(argc==3&&std::string(argv[1])=="--case"&&argv[2]!=std::string(c.name))continue;c.run();found=true;}return found?0:2;}
