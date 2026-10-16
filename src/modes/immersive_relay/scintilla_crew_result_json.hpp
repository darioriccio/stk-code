// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_RESULT_JSON_HPP
#define HEADER_SCINTILLA_CREW_RESULT_JSON_HPP
#include "modes/immersive_relay/scintilla_crew_result.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <ostream>
namespace ScintillaCrew {
inline void writeReportParticipantJSON(std::ostream& out,const ReportParticipant& person) {
    using ScintillaJSON::quote;const auto& a=person.assignment;
    out<<"{\"participantId\":"<<quote(a.participantId)<<",\"name\":"<<quote(a.name)<<",\"isPilot\":"<<(a.isPilot?"true":"false")
       <<",\"competence\":"<<(a.isPilot?"null":quote(Runtime::competenceName(a.competence)))<<",\"contributions\":{";
    for(unsigned c=0;c<9;++c){if(c)out<<',';out<<quote(Runtime::contributionName(ContributionKind(c)))<<':'<<quote(std::to_string(person.contributions[c]));}
    out<<"},\"opportunityTicks\":{";const char* metrics[]={"noOpportunity","waiting","engaged","notReady","paused"};
    for(unsigned c=0;c<5;++c){if(c)out<<',';out<<quote(metrics[c])<<':'<<quote(std::to_string(person.opportunityTicks[c]));}out<<"}}";
}
// Every surface serializes this exact immutable typed envelope.
inline void writeResultJSON(std::ostream& out,const Result& result) {
    if(!result.valid){out<<"null";return;}using ScintillaJSON::quote;
    out<<"{\"schemaVersion\":1,\"producerUUID\":"<<quote(result.producerUUID)<<",\"raceEpoch\":"<<quote(std::to_string(result.raceEpoch))
       <<",\"missionRevision\":"<<quote(std::to_string(result.missionRevision))<<",\"finishSequence\":"<<quote(std::to_string(result.finishSequence))
       <<",\"finishTick\":"<<quote(std::to_string(result.finishTick))<<",\"profileId\":"<<quote(result.profileId)<<",\"crewRulesRevision\":"<<quote(result.crewRulesRevision)
       <<",\"partyEnabled\":"<<(result.partyEnabled?"true":"false")<<",\"bucket\":"<<quote(result.bucket)
       <<",\"completedHandoffs\":"<<quote(std::to_string(result.completedHandoffs))<<",\"partyActivations\":"<<quote(std::to_string(result.partyActivations))
       <<",\"rotationAnomalies\":"<<quote(std::to_string(result.rotationAnomalies))<<",\"comparable\":"<<(result.comparable?"true":"false")
       <<",\"physicsFPS\":"<<result.physicsFPS<<",\"countersSaturated\":"<<(result.countersSaturated?"true":"false")<<",\"parameters\":{";
    const auto& p=result.parameters;
    const char* names[]={"suggestionIntervalTicks","suggestionHoldTicks","urgentTetherTicks","maximumWaitTicks","partyHoldTicks","partyWindowTicks","partyCooldownTicks","partyBurstTicks","partyFadeTicks"};
    const Tick values[]={p.suggestionIntervalTicks,p.suggestionHoldTicks,p.urgentTetherTicks,p.maximumWaitTicks,p.partyHoldTicks,p.partyWindowTicks,p.partyCooldownTicks,p.partyBurstTicks,p.partyFadeTicks};
    for(unsigned i=0;i<9;++i){if(i)out<<',';out<<quote(names[i])<<':'<<quote(std::to_string(values[i]));}
    out<<",\"partyCost\":"<<p.partyCost<<",\"partySpeedDeltaMps\":"<<p.partySpeedDeltaMps<<",\"partyRequiredCount\":"<<p.partyRequiredCount
       <<",\"partyZoneManifestSHA256\":"<<quote(p.partyZoneManifestSHA256)<<"},\"participants\":[";
    for(unsigned i=0;i<result.participants.size();++i){if(i)out<<',';writeReportParticipantJSON(out,result.participants[i]);}out<<"]}";
}
}
#endif
