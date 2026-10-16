// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_MISSION_RESULT_JSON_HPP
#define HEADER_SCINTILLA_MISSION_RESULT_JSON_HPP

#include "modes/immersive_relay/scintilla_mission.hpp"
#include <iomanip>
#include <ostream>

namespace ScintillaMission
{
// One serializer for the API and persisted report. Wide identities never pass
// through floating point; absent official race facts remain JSON null.
inline void writeResultString(std::ostream& out,const std::string& value)
{
    out << '"';
    for(unsigned char c:value)
    {
        if(c=='"' || c=='\\') out << '\\' << c;
        else if(c<32 || c==127) out << '?';
        else out << c;
    }
    out << '"';
}
inline void writeResultJSON(std::ostream& out,const Result& result)
{
    if(!result.valid) {out << "null";return;}
    const std::streamsize previousPrecision=out.precision();
    out << "{\"schemaVersion\":1,\"raceEpoch\":\"" << result.raceEpoch
        << "\",\"missionRevision\":\"" << result.missionRevision
        << "\",\"finishSequence\":\"" << result.finishSequence
        << "\",\"finishTick\":\"" << result.finishTick
        << "\",\"naturalFinish\":" << (result.natural?"true":"false")
        << ",\"nativeFinishTime\":";
    if(result.natural) out << std::setprecision(17) << result.nativeFinishTime;
    else out << "null";
    out << ",\"officialTeamRacePosition\":";
    if(result.natural) out << result.officialTeamRacePosition;
    else out << "null";
    out << ",\"securedCount\":" << result.securedCount
        << ",\"cargoCompleteAtFinish\":" << (result.cargoCompleteAtFinish?"true":"false")
        << ",\"slotsAtFinish\":[";
    for(size_t i=0;i<result.slots.size();++i)
    {
        if(i) out << ',';
        const Slot& slot=result.slots[i];
        const char* state=slot.state==SlotState::Secured?"secured":
                          slot.state==SlotState::Tethered?"tethered":"empty";
        out << "{\"state\":\"" << state << "\",\"moduleId\":";
        if(slot.module) out << '"' << slot.module << '"';else out << "null";
        out << ",\"everAcquired\":" << (slot.everAcquired?"true":"false")
            << ",\"beaconLit\":" << (slot.beaconLit?"true":"false") << '}';
    }
    out << "],\"beaconsAtFinish\":[";
    for(size_t i=0;i<result.slots.size();++i)
    {if(i) out << ',';out << (result.slots[i].beaconLit?"true":"false");}
    out << "],\"terminationReason\":";writeResultString(out,result.terminationReason);
    out << ",\"outcome\":";writeResultString(out,result.outcome);out << '}';
    out.precision(previousPrecision);
}
}
#endif
