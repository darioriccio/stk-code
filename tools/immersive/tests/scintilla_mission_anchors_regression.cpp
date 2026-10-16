// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_mission_anchors.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
using namespace ScintillaMissionAnchors;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);}}while(false)
namespace
{
void replace(std::string& value,const std::string& before,const std::string& after)
{const auto p=value.find(before);CHECK(p!=std::string::npos);value.replace(p,before.size(),after);}
std::string json()
{
 std::string value=R"({"schemaVersion":1,"profileId":"sc4_cargo_three_slots_v1","profileRevision":1,
 "trackId":"snowtuxpeak","direction":"forward","trackHash":"HASH","authoringStatus":"development_unqualified",
 "author":"Scintilla regression fixture","license":"GPL-3.0-or-later","groups":[
 {"id":"one","activationNode":1,"expiryNode":2,"activationFraction":0.1,"expiryFraction":0.2,
  "laps":[0,1,2,3,4,5,6,7,8,9,10,11],"capsules":[{"center":[0,3.5,0],"radius":2},{"center":[5,3.5,0],"radius":2}]},
 {"id":"two","activationNode":3,"expiryNode":4,"activationFraction":0.3,"expiryFraction":0.4,
  "laps":[0,1,2,3,4,5,6,7,8,9,10,11],"capsules":[{"center":[20,3.5,0],"radius":2},{"center":[25,3.5,0],"radius":2}]},
 {"id":"three","activationNode":5,"expiryNode":6,"activationFraction":0.5,"expiryFraction":0.6,
  "laps":[0,1,2,3,4,5,6,7,8,9,10,11],"capsules":[{"center":[40,3.5,0],"radius":2},{"center":[45,3.5,0],"radius":2}]}],
 "confirmations":[{"slot":0,"node":2,"fraction":0.25,"laps":[0,1,2,3,4,5,6,7,8,9,10,11]},
 {"slot":1,"node":4,"fraction":0.45,"laps":[0,1,2,3,4,5,6,7,8,9,10,11]},
 {"slot":2,"node":6,"fraction":0.65,"laps":[0,1,2,3,4,5,6,7,8,9,10,11]}]})";
 replace(value,"HASH",std::string(64,'a'));return value;
}
Manifest manifest()
{Manifest m;std::string reason;CHECK(load(json(),&m,&reason));CHECK(reason.empty());return m;}
NativeTrack native()
{
 NativeTrack n;n.trackId="snowtuxpeak";n.trackHash=std::string(64,'a');n.lapLength=1000;
 for(unsigned i=0;i<10;++i)n.nodeFractions.push_back(i/10.0);return n;
}
void loaderAcceptsBoundedDevelopmentDataWithoutClaimingQualification()
{
 const Manifest m=manifest();CHECK(m.groups.size()==3&&m.groups[0].capsules.size()==2);
 CHECK(m.groups[0].laps.size()==12&&m.confirmations[2].slot==2);
 CHECK(m.authoringStatus=="development_unqualified"&&!m.reverse&&m.profileRevision==1);
 std::string reason;CHECK(validateNative(m,native(),&reason));CHECK(reason.empty());
 // There is deliberately no field that certifies optics or playability.
 CHECK(m.groups[0].capsules[0].radius==2);
}
void loaderRejectsDuplicateUnknownAndInexactFieldsAtomically()
{
 for(unsigned mutation=0;mutation<7;++mutation){
  std::string bad=json();
  if(mutation==0)bad.insert(1,"\"schema\\u0056ersion\":1,");
  if(mutation==1)bad.insert(1,"\"qualified\":true,");
  if(mutation==2)replace(bad,"\"schemaVersion\":1","\"schemaVersion\":1.00000001");
  if(mutation==3)replace(bad,"\"profileRevision\":1","\"profileRevision\":9007199254740991.00000001");
  if(mutation==4)bad=std::string(65537,'x');
  if(mutation==5)replace(bad,"\"radius\":2","\"radius\":1e309");
  if(mutation==6)replace(bad,"development_unqualified","locally_qualified");
  Manifest unchanged;unchanged.author="preserved";std::string reason;
  CHECK(!load(bad,&unchanged,&reason));CHECK(!reason.empty());CHECK(unchanged.author=="preserved");
 }
 CHECK(!load(json(),nullptr,nullptr));
}
void loaderRejectsInvalidGeometryAndUnsafeAssetReferences()
{
 for(unsigned mutation=0;mutation<6;++mutation){
  std::string bad=json();
  if(mutation==0)replace(bad,"[5,3.5,0]","[3.99,3.5,0]");
  if(mutation==1)replace(bad,"\"radius\":2","\"radius\":0");
  if(mutation==2)replace(bad,"[0,3.5,0]","[1000001,3.5,0]");
  if(mutation==3)replace(bad,"\"radius\":2","\"radius\":2,\"asset\":\"../escape.png\"");
  if(mutation==4)replace(bad,"\"radius\":2","\"radius\":2,\"asset\":\"/absolute.png\"");
  if(mutation==5)replace(bad,"[0,3.5,0]","[0,3.5]");
  Manifest m;std::string reason;CHECK(!load(bad,&m,&reason));
 }
 auto tangent=json();replace(tangent,"[5,3.5,0]","[4,3.5,0]");Manifest m;CHECK(load(tangent,&m,nullptr));
}
void loaderRejectsOverlappingWindowsAndInvalidLapAllowlists()
{
 for(unsigned mutation=0;mutation<7;++mutation){
  std::string bad=json();
  if(mutation==0)replace(bad,"\"expiryFraction\":0.2","\"expiryFraction\":0.05");
  if(mutation==1)replace(bad,"\"activationFraction\":0.3","\"activationFraction\":0.15");
  if(mutation==2)replace(bad,"\"laps\":[0,1,2,3,4,5,6,7,8,9,10,11]","\"laps\":[0,0]");
  if(mutation==3)replace(bad,"\"laps\":[0,1,2,3,4,5,6,7,8,9,10,11]","\"laps\":[12]");
  if(mutation==4)replace(bad,"\"slot\":1","\"slot\":0");
  if(mutation==5)replace(bad,"\"id\":\"two\"","\"id\":\"one\"");
  if(mutation==6)replace(bad,"\"direction\":\"forward\"","\"direction\":\"reverse\"");
  Manifest m;CHECK(!load(bad,&m,nullptr));
 }
}
void nativeValidationRequiresExactTrackIdentityAndNodeContainment()
{
 const Manifest m=manifest();
 for(unsigned mutation=0;mutation<7;++mutation){
  NativeTrack n=native();
  if(mutation==0)n.trackHash=std::string(64,'b');if(mutation==1)n.reverse=true;
  if(mutation==2)n.trackId="another_track";if(mutation==3)n.lapLength=0;
  if(mutation==4)n.nodeFractions[3]=n.nodeFractions[2];
  if(mutation==5)n.nodeFractions[3]=std::numeric_limits<double>::quiet_NaN();
  if(mutation==6)n.nodeFractions.resize(3);
  CHECK(!validateNative(m,n,nullptr));
 }
 Manifest shifted=m;shifted.groups[0].activationNode=7;
 CHECK(!validateNative(shifted,native(),nullptr));
}
void nativeFloatBoundaryToleranceDoesNotAdmitAnUnrelatedNode()
{
 Manifest m=manifest();m.groups[0].activationFraction=0.1-1e-6;
 CHECK(validateNative(m,native(),nullptr));
 m.groups[0].activationFraction=0.1-2e-5;CHECK(!validateNative(m,native(),nullptr));
}
void beforeNativeStartAndFirstObservationNeverRetroactivelyReward()
{
 Traversal t;CHECK(t.prepare(manifest(),2,2));CHECK(t.update(-1,.99,true).empty());CHECK(t.lastUpdateValid());
 CHECK(t.opportunityReceiptCount()==0);auto first=t.update(0,.35,true);CHECK(!first.empty());
 for(const Crossing& c:first)CHECK(c.skipped);
 CHECK(t.opportunityReceiptCount()==4&&t.confirmationReceiptCount()==1);
 CHECK(t.update(0,.35,true).empty());
}
void backwardAndRepeatedCrossingsCannotFarm()
{
 Traversal t;CHECK(t.prepare(manifest(),1,2));CHECK(t.update(0,0,true).empty());
 auto first=t.update(0,.1,true);CHECK(first.size()==2&&!first[0].skipped&&!first[1].skipped);
 CHECK(t.update(0,.05,true).empty());CHECK(t.update(0,.1,true).empty());
 const auto expiry=t.update(0,.2,true);CHECK(expiry.size()==2&&expiry[0].kind==Kind::Expiry);
 CHECK(t.opportunityReceiptCount()==2);CHECK(t.update(0,.1,true).empty());CHECK(t.update(0,.2,true).empty());
}
void discontinuousForwardSegmentsConsumeSkippedOccurrences()
{
 Traversal t;CHECK(t.prepare(manifest(),2,2));CHECK(t.update(0,0,true).empty());
 const auto jumped=t.update(0,.35,false);CHECK(!jumped.empty());
 for(const Crossing& c:jumped)CHECK(c.skipped);
 CHECK(t.opportunityReceiptCount()==4&&t.confirmationReceiptCount()==1);
 CHECK(t.update(0,.05,true).empty());CHECK(t.update(0,.35,true).empty());
 const auto expiry=t.update(0,.4,true);CHECK(expiry.size()==2&&expiry[0].kind==Kind::Expiry);
}
void multipleCrossingsAreOrderedAndExpiryPrecedesNewActivation()
{
 Manifest m=manifest();m.groups[1].activationNode=2;m.groups[1].activationFraction=.2;
 Traversal t;CHECK(t.prepare(m,1,2));CHECK(t.update(0,0,true).empty());
 const auto crossings=t.update(0,.45,true);CHECK(crossings.size()==10);
 for(std::size_t i=1;i<crossings.size();++i)CHECK(crossings[i-1].fraction<=crossings[i].fraction);
 CHECK(crossings[2].kind==Kind::Expiry&&crossings[3].kind==Kind::Expiry);
 CHECK(crossings[4].kind==Kind::Activation&&crossings[5].kind==Kind::Activation);
 CHECK(crossings.back().kind==Kind::Confirmation&&crossings.back().slot==1);
}
void onlyOfficialLapTransitionsCreateNewOccurrences()
{
 Traversal t;CHECK(t.prepare(manifest(),2,2));t.update(0,.9,true);
 CHECK(t.update(0,.01,true).empty());CHECK(t.update(0,.12,true).empty());
 CHECK(t.opportunityReceiptCount()==6);
 CHECK(t.update(1,.05,true).empty());const auto next=t.update(1,.1,true);
 CHECK(next.size()==2&&next[0].ordinal==24&&next[1].ordinal==25&&next[0].lap==1);
 CHECK(!next[0].skipped);CHECK(t.opportunityReceiptCount()==8);
}
void singleSupporterGetsOneCapsuleAndTheOtherOccurrenceIsSkipped()
{
 for(unsigned crew:{1,2,5}){
  Traversal t;CHECK(t.prepare(manifest(),1,crew));t.update(0,0,true);
  const auto group=t.update(0,.1,true);CHECK(group.size()==2&&!group[0].skipped);
  CHECK(group[1].skipped==(crew==1));CHECK(t.opportunityReceiptCount()==2);
 }
}
void exactRepreparePreservesReceiptsButSemanticChangesConflict()
{
 const Manifest m=manifest();Traversal t;CHECK(t.prepare(m,1,2));t.update(0,0,true);t.update(0,.1,true);
 CHECK(t.prepare(m,1,2));CHECK(t.update(0,.1,true).empty());CHECK(t.opportunityReceiptCount()==2);
 for(unsigned mutation=0;mutation<5;++mutation){
  Manifest changed=m;
  if(mutation==0)changed.groups[0].capsules[1].center.x=6;
  if(mutation==1)changed.groups[0].capsules[0].asset="other.png";
  if(mutation==2)changed.groups[0].laps.pop_back();
  if(mutation==3)changed.confirmations[0].fraction=.26;
  if(mutation==4)changed.author="new provenance";
  CHECK(!t.prepare(changed,1,2));CHECK(t.opportunityReceiptCount()==2);
 }
 CHECK(!t.prepare(m,2,2));CHECK(!t.prepare(m,1,1));
 t.reset();CHECK(t.prepare(m,1,2));CHECK(t.opportunityReceiptCount()==0);
}
void invalidSamplesPreserveReceiptsAndTheLastValidOrigin()
{
 Traversal t;CHECK(t.prepare(manifest(),1,2));t.update(0,0,true);
 CHECK(t.update(0,std::numeric_limits<double>::quiet_NaN(),true).empty());CHECK(!t.lastUpdateValid());
 CHECK(t.update(1,.1,true).empty());CHECK(!t.lastUpdateValid());
 CHECK(t.update(0,1.01,true).empty());CHECK(!t.lastUpdateValid());
 CHECK(t.opportunityReceiptCount()==0);
 const auto actual=t.update(0,.1,true);CHECK(t.lastUpdateValid()&&actual.size()==2&&!actual[0].skipped);
 CHECK(t.update(-1,.99,true).empty());CHECK(!t.lastUpdateValid());CHECK(t.opportunityReceiptCount()==2);
}
void maximumManifestSessionKeepsAllReceiptsBounded()
{
 Manifest m=manifest();const Group base=m.groups[0];m.groups.clear();
 for(unsigned i=0;i<12;++i){
  Group g=base;g.id="group"+std::to_string(i);g.activationFraction=.01+.08*i;g.expiryFraction=g.activationFraction+.04;
  g.activationNode=i;g.expiryNode=i;g.capsules[0].center.x=100*i;g.capsules[1].center.x=100*i+5;m.groups.push_back(g);
 }
 Traversal t;CHECK(t.prepare(m,12,5));CHECK(t.update(0,0,true).empty());
 const auto all=t.update(11,1,false);CHECK(all.size()==612);
 CHECK(t.opportunityReceiptCount()==288&&t.confirmationReceiptCount()==36);
 for(const Crossing& c:all)CHECK(c.skipped);
 CHECK(t.update(11,1,false).empty());CHECK(t.update(11,.1,true).empty());CHECK(t.update(11,.99,true).empty());
}
void scheduleMustContainThreeObtainableOccurrences()
{
 Manifest m=manifest();m.groups.resize(1);Traversal t;
 CHECK(!t.prepare(m,1,1));CHECK(!t.prepare(m,1,2));CHECK(t.prepare(m,3,1));
 t.reset();m.groups.resize(13);CHECK(!t.prepare(m,12,5));
}
}
int main(int argc,char** argv)
{
 struct Case{const char* name;void(*run)();};const Case cases[]={
#define ANCHORS_CASE(name) {#name,name},
#include "scintilla_mission_anchors_cases.inc"
#undef ANCHORS_CASE
 };
 const bool selected=argc==3&&std::string(argv[1])=="--case";if(argc!=1&&!selected)return 2;
 bool found=false;for(const Case& c:cases)if(!selected||c.name==std::string(argv[2]))
 {found=true;std::cout<<c.name<<std::endl;c.run();}return found?0:2;
}
