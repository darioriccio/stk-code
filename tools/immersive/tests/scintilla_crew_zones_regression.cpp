// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_crew_zones.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>
using namespace ScintillaCrewZones;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
std::string fixture(bool qualified=false) {
    return std::string(R"({"schemaVersion":1,"profileId":"sc5_crew_party_v1","crewRulesRevision":"1","trackId":"snowtuxpeak","trackHash":")")+std::string(64,'a')+
        R"(","reverse":false,"authoringStatus":")"+(qualified?"authored":"development_unqualified")+
        R"(","author":"native regression fixture","license":"GPL-3.0-or-later","speedBoundMetersPerSecond":30,"marginSeconds":0.5,"positionErrorMeters":0.5,"finalExclusionMeters":60,"candidateNodes":[1,2],"zones":[{"id":"one","startNode":1,"endNode":2,"startMeters":100,"endMeters":200,"minimumWidthMeters":6}]})";
}
Manifest manifest(bool qualified=true){Manifest m;std::string reason;CHECK(load(fixture(qualified),&m,&reason));return m;}
Sample sample(int64_t tick,double distance=110){Sample s;s.tick=tick;s.lap=0;s.distance=distance;s.x=distance;s.speed=20;s.onRoad=true;return s;}
void authoringNeverQualifiesAndLoadingIsAtomic() {
    auto m=manifest(false);std::string reason;CHECK(!qualify(m,"{}",std::string(64,'b'),std::string(64,'c'),std::string(64,'d'),std::string(64,'f'),120,&reason));
    Tracker t;CHECK(!t.update(sample(0),120,1000,30));CHECK(t.update(sample(1,110.1),120,1000,30));CHECK(!t.available(m,1000,6,2));
    for(const std::string& bad:{std::string(16385,'x'),std::string("{\"schemaVersion\":1,\"schemaVersion\":1}"),fixture()+"x"}) {
        m.author="preserved";CHECK(!load(bad,&m,&reason)&&m.author=="preserved");
    }
}
void exactNativeTrackNodesAndDirectionAreRequired() {
    auto m=manifest();std::string reason;std::vector<double> nodes={0,100,200};
    CHECK(validateNative(m,"snowtuxpeak",std::string(64,'a'),false,1000,nodes,&reason));
    CHECK(!validateNative(m,"snowtuxpeak",std::string(64,'b'),false,1000,nodes,&reason));
    CHECK(!validateNative(m,"snowtuxpeak",std::string(64,'a'),true,1000,nodes,&reason));
    nodes[2]+=0.01;CHECK(!validateNative(m,"snowtuxpeak",std::string(64,'a'),false,1000,nodes,&reason));
    nodes[2]=200;CHECK(!validateNative(m,"snowtuxpeak",std::string(64,'a'),false,250,nodes,&reason));
}
void qualificationBindsEveryZoneBuildAndEvidenceHash() {
    auto m=manifest();std::string reason;
    std::string report=R"({"schemaVersion":1,"contract":"tholus.stk-scintilla-party-zones/1","profileId":"sc5_crew_party_v1","crewRulesRevision":"1","manifestSHA256":")"+std::string(64,'b')+R"(","trackHash":")"+std::string(64,'a')+R"(","nativeBuildSHA256":")"+std::string(64,'d')+R"(","trackAssetsSHA256":")"+std::string(64,'f')+R"(","evidenceSHA256":")"+std::string(64,'c')+R"(","zones":[{"zoneId":"one","geometryClear":true,"noRampOrJump":true,"brakeBefore":true,"brakeDuring":true,"brakeAccelOrders":true,"analogBrake":true,"laterNativePad":true,"observedSpeedBoundMetersPerSecond":30,"observedPositionErrorMeters":0.5,"physicsFPS":120}]})";
    CHECK(qualify(m,report,std::string(64,'b'),std::string(64,'c'),std::string(64,'d'),std::string(64,'f'),120,&reason));
    CHECK(!qualify(m,report,std::string(64,'b'),std::string(64,'c'),std::string(64,'e'),std::string(64,'f'),120,&reason));
    CHECK(!qualify(m,report,std::string(64,'b'),std::string(64,'e'),std::string(64,'d'),std::string(64,'f'),120,&reason));
    CHECK(!qualify(m,report,std::string(64,'b'),std::string(64,'c'),std::string(64,'d'),std::string(64,'f'),61,&reason));
    const auto at=report.find("\"brakeDuring\":true");CHECK(at!=std::string::npos);report.replace(at,18,"\"brakeDuring\":false");
    CHECK(!qualify(m,report,std::string(64,'b'),std::string(64,'c'),std::string(64,'d'),std::string(64,'f'),120,&reason));
}
// A parser fixture describing the native format; never installed as proof.
std::string nativeFormatFixture() {
    std::string out=R"({"schemaVersion":1,"contract":"tholus.stk-scintilla-party-zones-evidence/1","sourceKind":"native_runtime","synthetic":false,"producerUUID":"12345678-1234-4234-8234-123456789abc","operatorId":"synthetic.operator","manifestSHA256":")"+std::string(64,'b')+R"(","trackHash":")"+std::string(64,'a')+R"(","nativeBuildSHA256":")"+std::string(64,'d')+R"(","trackAssetsSHA256":")"+std::string(64,'f')+R"(","physicsFPS":120,"zones":[{"zoneId":"one","geometryClear":true,"noRampOrJump":true,"observedSpeedBoundMetersPerSecond":30,"observedPositionErrorMeters":0.5,"checks":{)";
    bool first=true;for(auto name:{"brakeBefore","brakeDuring","brakeAccelOrders","analogBrake","laterNativePad"}){
        if(!first)out+=',';first=false;out+='"';out+=name;out+=R"(":{"passed":true,"nativeRunId":"12345678-1234-4234-8234-123456789abc","artifactSHA256":")"+std::string(64,'e')+"\"}";
    }return out+"}}]}";
}
void nativeEvidenceRequiresClosedProvenanceAndReturnsBoundedProofHashes() {
    auto m=manifest();std::string reason;std::vector<std::string> artifacts;
    const auto body=nativeFormatFixture();
    CHECK(nativeEvidence(m,body,std::string(64,'b'),std::string(64,'d'),std::string(64,'f'),120,&artifacts,&reason));
    CHECK(artifacts==std::vector<std::string>({std::string(64,'e')}));
    m.status="development_unqualified";artifacts={"sentinel"};
    CHECK(!nativeEvidence(m,body,std::string(64,'b'),std::string(64,'d'),std::string(64,'f'),120,&artifacts,&reason));
    CHECK(artifacts==std::vector<std::string>({"sentinel"}));
}
void nativeEvidenceRejectsSyntheticStaleFailedAndIncompleteObservations() {
    const std::vector<std::pair<std::string,std::string>> mutations={
        {"\"synthetic\":false","\"synthetic\":true"},{"native_runtime","offline_math"},
        {"\"physicsFPS\":120","\"physicsFPS\":61"},{"\"passed\":true","\"passed\":false"},
        {"\"geometryClear\":true","\"geometryClear\":false"},{"\"noRampOrJump\":true","\"noRampOrJump\":false"},
        {"\"observedSpeedBoundMetersPerSecond\":30","\"observedSpeedBoundMetersPerSecond\":29.99"},
        {"\"observedPositionErrorMeters\":0.5","\"observedPositionErrorMeters\":0.501"},
        {"\"brakeDuring\"","\"unknownCheck\""},{"\"operatorId\":\"synthetic.operator\"","\"operatorId\":\"\""},
        {std::string(64,'b'),std::string(64,'c')},{std::string(64,'d'),std::string(64,'c')},
        {std::string(64,'f'),std::string(64,'c')},{"\"schemaVersion\":1","\"schemaVersion\":1,\"schemaVersion\":1"}};
    for(const auto& mutation:mutations){auto body=nativeFormatFixture();const auto at=body.find(mutation.first);CHECK(at!=std::string::npos);body.replace(at,mutation.first.size(),mutation.second);
        std::string reason;std::vector<std::string> artifacts={"sentinel"};
        CHECK(!nativeEvidence(manifest(),body,std::string(64,'b'),std::string(64,'d'),std::string(64,'f'),120,&artifacts,&reason));
        CHECK(artifacts==std::vector<std::string>({"sentinel"}));
    }
}
void remainingDistanceIsCheckedAtTheLastConsent() {
    auto m=manifest();Tracker t;CHECK(!t.update(sample(0),120,1000,30));CHECK(t.update(sample(1,110.1),120,1000,30));
    CHECK(guardDistance(m,2)==40);CHECK(t.available(m,1000,6,2));
    t.reset();CHECK(!t.update(sample(0,159.9),120,1000,30));CHECK(t.update(sample(1,160),120,1000,30));CHECK(t.available(m,1000,6,2));
    CHECK(t.update(sample(2,160.1),120,1000,30));CHECK(!t.available(m,1000,6,2));
    CHECK(!t.available(m,1000,6,std::numeric_limits<double>::infinity()));
}
void rescueTeleportBackwardAndClockGapsBreakContinuity() {
    for(unsigned cause=0;cause<6;++cause){Tracker t;auto a=sample(0);CHECK(!t.update(a,120,1000,30));auto b=sample(1,110.1);
        if(cause==0)b.animated=true;if(cause==1)b.distance=109;if(cause==2)b.x=120;if(cause==3)b.tick=2;if(cause==4)b.onRoad=false;if(cause==5)b.speed=31;
        CHECK(!t.update(b,120,1000,30));CHECK(!t.available(manifest(),1000,6,2));
        // Re-entry requires a new continuous observation, never stale coordinates.
        t.update(sample(3,110.2),120,1000,30);CHECK(t.update(sample(4,110.3),120,1000,30));CHECK(t.available(manifest(),1000,6,2));
    }
}
void sameTickRecheckCannotAcceptMovedKartOrFinalSegment() {
    auto m=manifest();Tracker t;auto a=sample(1,110.1);CHECK(!t.update(sample(0),120,1000,30));CHECK(t.update(a,120,1000,30));CHECK(t.update(a,120,1000,30));
    a.x+=0.001;CHECK(!t.update(a,120,1000,30));CHECK(!t.available(m,1000,6,2));
    m.finalExclusion=900;t.reset();CHECK(!t.update(sample(2),120,1000,30));CHECK(t.update(sample(3,110.1),120,1000,30));CHECK(!t.available(m,1000,6,2));
    m=manifest();m.finalExclusion=20;CHECK(!t.available(m,1000,6,2));
}
struct Case{const char* name;void(*run)();};const Case cases[]={
    {"authoringNeverQualifiesAndLoadingIsAtomic",authoringNeverQualifiesAndLoadingIsAtomic},
    {"exactNativeTrackNodesAndDirectionAreRequired",exactNativeTrackNodesAndDirectionAreRequired},
    {"qualificationBindsEveryZoneBuildAndEvidenceHash",qualificationBindsEveryZoneBuildAndEvidenceHash},
    {"nativeEvidenceRequiresClosedProvenanceAndReturnsBoundedProofHashes",nativeEvidenceRequiresClosedProvenanceAndReturnsBoundedProofHashes},
    {"nativeEvidenceRejectsSyntheticStaleFailedAndIncompleteObservations",nativeEvidenceRejectsSyntheticStaleFailedAndIncompleteObservations},
    {"remainingDistanceIsCheckedAtTheLastConsent",remainingDistanceIsCheckedAtTheLastConsent},
    {"rescueTeleportBackwardAndClockGapsBreakContinuity",rescueTeleportBackwardAndClockGapsBreakContinuity},
    {"sameTickRecheckCannotAcceptMovedKartOrFinalSegment",sameTickRecheckCannotAcceptMovedKartOrFinalSegment}};
}
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--list"){for(const auto& c:cases)std::cout<<c.name<<'\n';return 0;}
if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;bool found=false;for(const auto& c:cases)if(argc==1||argv[2]==std::string(c.name)){c.run();found=true;}return found?0:2;}
