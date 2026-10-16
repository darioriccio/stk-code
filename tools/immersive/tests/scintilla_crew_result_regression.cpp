// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Production SC5 core/result/serializer/file semantics. No game or hardware.
#include "modes/immersive_relay/scintilla_crew_result_json.hpp"
#include "modes/immersive_relay/scintilla_crew_report_file.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(#x)+":"+std::to_string(__LINE__));}while(false)
using namespace ScintillaCrew;
namespace {
const std::string producer="01234567-89ab-4cde-8fab-0123456789ab";
std::vector<Participant> people(unsigned count) {std::vector<Participant> p;for(unsigned i=0;i<count;++i){Participant a;
    a.id="person-"+std::to_string(i);a.name="Person \""+std::to_string(i)+"\"";p.push_back(a);}return p;}
Runtime ready(unsigned count,bool party,int fps=120,uint64_t race=1) {Runtime r;
    CHECK(r.setIntent(true,party?"sc5_crew_party_v1":"sc5_crew_standard_v1"));CHECK(r.prepare(race,1,1,people(count),0,0,fps));return r;}
ScintillaMission::Result finish(uint64_t race=1,Tick tick=9,bool natural=true) {ScintillaMission::Result r;
    r.valid=true;r.natural=natural;r.raceEpoch=race;r.missionRevision=1;r.finishSequence=1;r.finishTick=tick;return r;}
std::string json(const Result& r) {std::ostringstream out;writeResultJSON(out,r);return out.str();}
void naturalSnapshotPreservesEveryParticipantAndProfileBucket() {
    for(unsigned count=2;count<=6;++count)for(bool party:{false,true}) {
        auto runtime=ready(count,party);uint64_t serial=0;
        for(unsigned person=1;person<count;++person)for(unsigned category=0;category<9;++category)
            CHECK(runtime.credit(people(count)[person].id,ContributionKind(category),++serial));
        const auto observation=observeResult(runtime,producer,1,1,9,false,party?std::string(64,'a'):"");
        const auto result=freezeResult(observation,finish());CHECK(result.valid&&result.comparable);
        CHECK(result.participants.size()==count&&result.partyEnabled==party&&result.partyActivations==0);
        CHECK(result.bucket==(party?"scintilla_sc5_party_v1":"scintilla_sc5_standard_v1"));
        for(unsigned i=0;i<count;++i){CHECK(result.participants[i].assignment.participantId==people(count)[i].id);
            CHECK(result.participants[i].assignment.isPilot==(i==0));for(auto n:result.participants[i].contributions)CHECK(n==(i?1u:0u));}
        const auto parsed=ScintillaJSON::Parser(json(result)).parse();CHECK(parsed.object.size()==18);
        CHECK(ScintillaJSON::at(parsed,"parameters").object.size()==13);
        CHECK(ScintillaJSON::at(parsed,"participants").array[1].object.size()==6);
    }
    const uint64_t race=UINT64_C(9007199254740993);auto runtime=ready(2,false,120,race);
    const auto result=freezeResult(observeResult(runtime,producer,race,1,9,false,""),finish(race));
    const auto parsed=ScintillaJSON::Parser(json(result)).parse();CHECK(ScintillaJSON::at(parsed,"raceEpoch").string=="9007199254740993");
}
void reportParametersMatchExactIntegerRulesAtEveryFPS() {
    for(int f=60;f<=1000;++f)for(unsigned count=2;count<=6;++count) {
        const auto runtime=ready(count,true,f);const auto result=freezeResult(observeResult(runtime,producer,1,1,9,false,std::string(64,'b')),finish());
        CHECK(result.valid&&result.physicsFPS==f);const auto& p=result.parameters;
        CHECK(4*p.suggestionIntervalTicks>=f&&4*(p.suggestionIntervalTicks-1)<f);
        CHECK(5*p.suggestionHoldTicks>=3*f&&5*(p.suggestionHoldTicks-1)<3*f);
        CHECK(5*p.urgentTetherTicks>=6*f&&5*(p.urgentTetherTicks-1)<6*f);
        CHECK(p.maximumWaitTicks==30*f&&p.partyWindowTicks==3*f&&p.partyCooldownTicks==6*f);
        CHECK(4*p.partyHoldTicks>=3*f&&4*(p.partyHoldTicks-1)<3*f);
        CHECK(2*p.partyBurstTicks<=f&&2*(p.partyBurstTicks+1)>f);
        CHECK(4*p.partyFadeTicks<=f&&4*(p.partyFadeTicks+1)>f);
        CHECK(p.partyCost==15&&p.partySpeedDeltaMps==1&&p.partyRequiredCount==(count==2?1u:2u));
    }
}
void identityMismatchAndMalformedObservationCannotFreeze() {
    const auto runtime=ready(3,true);const auto observation=observeResult(runtime,producer,1,1,9,false,std::string(64,'c'));
    for(unsigned fault=0;fault<12;++fault){auto o=observation;auto m=finish();
        if(fault==0)++m.raceEpoch;if(fault==1)++m.missionRevision;if(fault==2)++m.finishTick;
        if(fault==3)m.finishSequence=0;if(fault==4)m.valid=false;if(fault==5)o.available=false;
        if(fault==6)o.snapshot.participants.resize(7);if(fault==7)o.snapshot.physicsFPS=59;
        if(fault==8)o.snapshot.profileId="unknown";if(fault==9)o.snapshot.parameters.partyZoneManifestSHA256="bad";
        if(fault==10)++o.snapshot.parameters.partyHoldTicks;if(fault==11)o.snapshot.parameters.partyRequiredCount=1;
        CHECK(!freezeResult(o,m).valid);
    }
    auto frozen=observation;frozen.snapshot=freezeResult(observation,finish());
    CHECK(json(freezeResult(frozen,finish()))==json(frozen.snapshot));auto later=finish();++later.finishSequence;
    CHECK(!freezeResult(frozen,later).valid);CHECK(json(Result())=="null");
}
void frozenRecordOutlivesLateCreditsPauseResetAndProfileChange() {
    auto runtime=ready(4,true);CHECK(runtime.credit("person-1",ContributionKind::Opening,1));
    const auto observation=observeResult(runtime,producer,1,1,9,false,std::string(64,'d'));
    const auto result=freezeResult(observation,finish());const auto bytes=json(result);
    CHECK(runtime.credit("person-1",ContributionKind::Opening,2));runtime.suspend();CHECK(runtime.accountPaused(999));
    runtime.reset(false);CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));CHECK(runtime.prepare(2,1,1,people(2),0,0,60));
    CHECK(json(result)==bytes&&json(freezeResult(observation,finish()))==bytes);
    CHECK(result.participants[1].contributions[0]==1&&result.participants[1].opportunityTicks[4]==0);
    CHECK(result.partyEnabled&&result.physicsFPS==120&&runtime.profile().fps==60);
}
void artificialAdminTestSaturationCannotBeComparable() {
    for(unsigned reason=0;reason<5;++reason){auto runtime=ready(3,true);
        if(reason==1){CHECK(runtime.observeTurn(1,1,1,true));CHECK(runtime.prepare(1,2,2,people(3),1,1,120));}
        if(reason==2)for(unsigned i=0;i<3;++i)CHECK(runtime.accountPaused(std::numeric_limits<Tick>::max()));
        const auto observation=observeResult(runtime,producer,1,1,9,reason==3,reason==4?"":std::string(64,'e'));
        const auto result=freezeResult(observation,finish(1,9,reason!=0));CHECK(result.valid&&!result.comparable&&result.partyEnabled);
        CHECK(result.bucket=="scintilla_sc5_party_v1");if(reason==1)CHECK(result.rotationAnomalies==1);
        if(reason==2)CHECK(result.countersSaturated&&result.participants[1].opportunityTicks[4]==UINT64_MAX);
    }
}
void exclusiveArchiveNeverOverwritesAndKeysBoundNames() {
    const char* directory=std::getenv("SCINTILLA_TEST_OUTPUT_DIR");CHECK(directory&&directory[0]);
    auto runtime=ready(2,true);auto result=freezeResult(observeResult(runtime,producer,1,1,9,false,std::string(64,'f')),finish());
    result.raceEpoch=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto first=reportFileName(std::string(300,'x')+"/../../one",result);const auto secondSession=reportFileName(std::string(300,'x')+"/../../two",result);
    CHECK(first.size()<220&&first.find('/')==std::string::npos&&first!=secondSession);
    const std::string path=std::string(directory)+"/"+first;auto next=result;++next.raceEpoch;
    const std::string nextPath=std::string(directory)+"/"+reportFileName(std::string(300,'x')+"/../../one",next);
    struct Cleanup {std::string a,b;~Cleanup(){std::remove(a.c_str());std::remove(b.c_str());}} cleanup{path,nextPath};
    CHECK(writeReportExclusive(path,"original"));CHECK(!writeReportExclusive(path,"overwrite"));
    CHECK(writeReportExclusive(nextPath,"next race"));std::ifstream file(path.c_str());
    CHECK(std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>())=="original");
}
}
int main(int argc,char** argv) {
    const std::map<std::string,std::function<void()> > cases={
        {"naturalSnapshotPreservesEveryParticipantAndProfileBucket",naturalSnapshotPreservesEveryParticipantAndProfileBucket},
        {"reportParametersMatchExactIntegerRulesAtEveryFPS",reportParametersMatchExactIntegerRulesAtEveryFPS},
        {"identityMismatchAndMalformedObservationCannotFreeze",identityMismatchAndMalformedObservationCannotFreeze},
        {"frozenRecordOutlivesLateCreditsPauseResetAndProfileChange",frozenRecordOutlivesLateCreditsPauseResetAndProfileChange},
        {"artificialAdminTestSaturationCannotBeComparable",artificialAdminTestSaturationCannotBeComparable},
        {"exclusiveArchiveNeverOverwritesAndKeysBoundNames",exclusiveArchiveNeverOverwritesAndKeysBoundNames}};
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto& c:cases)std::cout<<c.first<<'\n';return 0;}
    if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;
    try{bool found=false;for(const auto& c:cases){if(argc==3&&c.first!=argv[2])continue;found=true;c.second();}return found?0:2;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
