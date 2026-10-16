// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_RESULT_HPP
#define HEADER_SCINTILLA_CREW_RESULT_HPP
#include "modes/immersive_relay/scintilla_crew.hpp"
#include "modes/immersive_relay/scintilla_mission.hpp"
#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include <algorithm>

namespace ScintillaCrew {
struct ReportParameters {
    Tick suggestionIntervalTicks=0,suggestionHoldTicks=0,urgentTetherTicks=0,maximumWaitTicks=0;
    Tick partyHoldTicks=0,partyWindowTicks=0,partyCooldownTicks=0,partyBurstTicks=0,partyFadeTicks=0;
    unsigned partyCost=15,partySpeedDeltaMps=1,partyRequiredCount=1;
    std::string partyZoneManifestSHA256;
};
struct ReportParticipant {
    Assignment assignment;
    std::array<uint64_t,9> contributions={{}};
    std::array<uint64_t,5> opportunityTicks={{}};
};
struct Result {
    bool valid=false,partyEnabled=false,comparable=false,countersSaturated=false;
    std::string producerUUID,profileId,crewRulesRevision="1",bucket;
    uint64_t raceEpoch=0,missionRevision=0,finishSequence=0,completedHandoffs=0,partyActivations=0,rotationAnomalies=0;
    Tick finishTick=0;int physicsFPS=0;
    ReportParameters parameters;
    std::vector<ReportParticipant> participants;
};
// Copies enter the session controller from its game-thread adapter. No engine
// callback, live Runtime reference or source lookup is needed under its mutex.
struct Observation {
    bool available=false,prepared=false,test=false;
    Result snapshot;
};
inline Observation observeNativeTurn(const Observation& observation,unsigned driver,uint64_t completed,uint64_t administrative) {
    if(!observation.available||observation.snapshot.participants.size()<2||observation.snapshot.participants.size()>6||
       driver>=observation.snapshot.participants.size()||completed<observation.snapshot.completedHandoffs)return Observation();
    Observation current=observation;auto& result=current.snapshot;
    if(completed!=result.completedHandoffs){current.prepared=false;result.valid=false;result.comparable=false;}
    result.completedHandoffs=completed;result.rotationAnomalies=std::max(result.rotationAnomalies,administrative);
    if(administrative)result.comparable=false;
    for(unsigned i=0;i<result.participants.size();++i){auto& assignment=result.participants[i].assignment;
        assignment.isPilot=i==driver;assignment.competence=competenceAtTurn(i,result.participants.size(),driver,completed);}
    return current;
}
inline bool reportHash(const std::string& hash) {
    if(hash.size()!=64)return false;
    for(char c:hash)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline Observation observeResult(const Runtime& runtime,const std::string& producer,
    uint64_t race,uint64_t revision,Tick tick,bool test,const std::string& zoneHash) {
    Observation observation;
    if(!runtime.required()||!runtime.configured()||runtime.race()!=race||!revision||tick<0||
       !ScintillaMissionProtocol::uuid(producer)||(!zoneHash.empty()&&!reportHash(zoneHash)))return observation;
    const auto assignments=runtime.assignments();const auto& records=runtime.records();
    if(assignments.size()<2||assignments.size()>Profile::MaxParticipants||assignments.size()!=records.size())return observation;
    Result& result=observation.snapshot;const auto& profile=runtime.profile();
    if(profile.fps<60||profile.fps>1000||profile.crew!=assignments.size()-1)return observation;
    result.producerUUID=producer;result.raceEpoch=race;result.missionRevision=revision;result.finishTick=tick;
    result.profileId=runtime.profileId();result.partyEnabled=runtime.partyEnabled();
    result.bucket=result.partyEnabled?"scintilla_sc5_party_v1":"scintilla_sc5_standard_v1";
    result.completedHandoffs=runtime.driverTurnId();result.partyActivations=runtime.partyActivations();
    result.rotationAnomalies=runtime.rotationAnomalies();result.countersSaturated=runtime.countersSaturated();result.physicsFPS=profile.fps;
    auto& p=result.parameters;p.suggestionIntervalTicks=profile.suggestionInterval;p.suggestionHoldTicks=profile.suggestionHold;
    p.urgentTetherTicks=profile.urgentTether;p.maximumWaitTicks=profile.maxWait;p.partyHoldTicks=profile.partyHold;p.partyWindowTicks=profile.partyWindow;
    p.partyCooldownTicks=6*Tick(profile.fps);p.partyBurstTicks=profile.fps/2;p.partyFadeTicks=profile.fps/4;
    p.partyRequiredCount=profile.crew==1?1:2;p.partyZoneManifestSHA256=zoneHash;
    for(unsigned i=0;i<assignments.size();++i){ReportParticipant person;person.assignment=assignments[i];
        person.contributions=records[i].contributions;person.opportunityTicks=records[i].opportunityTicks;result.participants.push_back(person);}
    observation.available=true;observation.prepared=runtime.prepared();observation.test=test;return observation;
}
inline bool matchesMissionResult(const Result& crew,const ScintillaMission::Result& mission) {
    return crew.valid&&mission.valid&&crew.raceEpoch==mission.raceEpoch&&crew.missionRevision==mission.missionRevision&&
        crew.finishSequence==mission.finishSequence&&crew.finishTick==mission.finishTick;
}
inline bool boundObservation(const Observation& observation,const ScintillaMission::Result& mission) {
    if(!observation.available)return false;const auto& r=observation.snapshot;
    if(r.participants.size()<2||r.participants.size()>Profile::MaxParticipants||r.physicsFPS<60||r.physicsFPS>1000||
       !ScintillaMissionProtocol::uuid(r.producerUUID)||r.raceEpoch!=mission.raceEpoch||r.missionRevision!=mission.missionRevision||
       r.finishTick!=mission.finishTick||r.crewRulesRevision!="1"||
       (r.profileId!="sc5_crew_standard_v1"&&r.profileId!="sc5_crew_party_v1")||
       r.partyEnabled!=(r.profileId=="sc5_crew_party_v1")||
       r.bucket!=(r.partyEnabled?"scintilla_sc5_party_v1":"scintilla_sc5_standard_v1")||
       (!r.parameters.partyZoneManifestSHA256.empty()&&!reportHash(r.parameters.partyZoneManifestSHA256)))return false;
    const Tick f=r.physicsFPS;const auto& p=r.parameters;
    return p.suggestionIntervalTicks==(f+3)/4&&p.suggestionHoldTicks==(3*f+4)/5&&p.urgentTetherTicks==(6*f+4)/5&&p.maximumWaitTicks==30*f&&
        p.partyHoldTicks==(3*f+3)/4&&p.partyWindowTicks==3*f&&p.partyCooldownTicks==6*f&&p.partyBurstTicks==f/2&&p.partyFadeTicks==f/4&&
        p.partyCost==15&&p.partySpeedDeltaMps==1&&p.partyRequiredCount==(r.participants.size()==2?1u:2u);
}
inline Result freezeResult(const Observation& observation,const ScintillaMission::Result& mission) {
    Result result;
    if(!mission.valid||!mission.raceEpoch||!mission.missionRevision||!mission.finishSequence||mission.finishTick<0||
       !boundObservation(observation,mission))return result;
    // An already terminal observation is a receipt of the original record.
    if(observation.snapshot.valid)return matchesMissionResult(observation.snapshot,mission)?observation.snapshot:result;
    result=observation.snapshot;result.valid=true;result.finishSequence=mission.finishSequence;
    result.comparable=mission.natural&&observation.prepared&&!observation.test&&!result.rotationAnomalies&&!result.countersSaturated;
    if(result.partyEnabled&&!reportHash(result.parameters.partyZoneManifestSHA256))result.comparable=false;
    return result;
}
}
#endif
