// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_PROTOCOL_HPP
#define HEADER_SCINTILLA_CREW_PROTOCOL_HPP

#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

// Wire validation only. Admission compares this identity with live native
// authority; neither a binding attestation nor a cached response opens a gate.
namespace ScintillaCrewProtocol {
using namespace ScintillaJSON;
static const size_t MaxRequestBytes=16384;
static const size_t MaxReceipts=64;
inline const char* contract(){return "tholus.stk-scintilla-crew/1";}
inline const char* standardProfile(){return "sc5_crew_standard_v1";}
inline const char* partyProfile(){return "sc5_crew_party_v1";}
inline const char* layoutProfile(){return "front_rear_crew_v3";}
inline const char* resultBucket(bool party){return party?"scintilla_sc5_party_v1":"scintilla_sc5_standard_v1";}
inline bool validProfile(const std::string& value){return value==standardProfile()||value==partyProfile();}
inline uint64_t unsignedValue(const std::string& value) {
    // Validate through the shared canonical-decimal helper before conversion.
    J j;j.kind=J::String;j.string=value;
    const auto canonical=ScintillaMissionProtocol::decimal(j);
    uint64_t result=0;for(char c:canonical)result=result*10+uint64_t(c-'0');return result;
}
inline std::string decimal(const J& value){return ScintillaMissionProtocol::decimal(value);}
inline std::string optionalToken(const J& value){return ScintillaMissionProtocol::optionalToken(value);}
inline bool uuid(const std::string& value){return ScintillaMissionProtocol::uuid(value);}
inline bool hash(const std::string& value){return ScintillaMissionProtocol::hash(value);}

struct Identity {
    std::string producerUUID,raceEpoch="0",driverTurnId="0",crewIntentRevision="0",crewRulesRevision="1";
    std::string rosterRevision="0",inputSessionID,inputEpoch="0",missionRevision="1";
    std::string routeRevision,projectionContextId,layoutProfile="front_rear_crew_v3",layoutRevision="3";
    std::string key()const {
        return producerUUID+"|"+raceEpoch+"|"+driverTurnId+"|"+crewIntentRevision+"|"+crewRulesRevision+
            "|"+rosterRevision+"|"+inputSessionID+"|"+inputEpoch+"|"+missionRevision+"|"+routeRevision+
            "|"+projectionContextId+"|"+layoutProfile+"|"+layoutRevision;
    }
    bool operator==(const Identity& other)const{return key()==other.key();}
    bool operator!=(const Identity& other)const{return !(*this==other);}
};
inline Identity identity(const J& j,bool allowIncomplete=false) {
    closed(j,{"producerUUID","raceEpoch","driverTurnId","crewIntentRevision","crewRulesRevision",
        "rosterRevision","inputSessionID","inputEpoch","missionRevision","routeRevision",
        "projectionContextId","layoutProfile","layoutRevision"});
    Identity i;i.producerUUID=text(j,"producerUUID");if(!uuid(i.producerUUID))throw Invalid("invalid_producer_uuid");
    i.raceEpoch=decimal(at(j,"raceEpoch"));i.driverTurnId=decimal(at(j,"driverTurnId"));
    i.crewIntentRevision=decimal(at(j,"crewIntentRevision"));i.crewRulesRevision=decimal(at(j,"crewRulesRevision"));
    i.rosterRevision=decimal(at(j,"rosterRevision"));i.inputSessionID=optionalToken(at(j,"inputSessionID"));
    i.inputEpoch=decimal(at(j,"inputEpoch"));i.missionRevision=decimal(at(j,"missionRevision"));
    i.routeRevision=optionalToken(at(j,"routeRevision"));i.projectionContextId=optionalToken(at(j,"projectionContextId"));
    i.layoutProfile=text(j,"layoutProfile");i.layoutRevision=decimal(at(j,"layoutRevision"));
    const bool legacy=(i.layoutProfile=="front_rear_readonly_v1"&&i.layoutRevision=="1")||
        (i.layoutProfile=="front_rear_cargo_v2"&&i.layoutRevision=="2");
    const bool crew=i.layoutProfile==layoutProfile()&&i.layoutRevision=="3";
    const bool unset=i.layoutProfile.empty()&&i.layoutRevision=="0"&&i.inputSessionID.empty()&&i.inputEpoch=="0";
    if(!crew&&!(allowIncomplete&&(legacy||unset)))throw Invalid("invalid_crew_layout");
    if(i.crewRulesRevision!="1"&&!(allowIncomplete&&i.crewRulesRevision=="0"))throw Invalid("invalid_crew_rules_revision");
    if(i.missionRevision!="1"&&!(allowIncomplete&&i.missionRevision=="0"))throw Invalid("invalid_mission_revision");
    if(!allowIncomplete&&(i.raceEpoch=="0"||i.crewIntentRevision=="0"||i.rosterRevision=="0"||
       i.inputEpoch=="0"||i.inputSessionID.empty()||i.routeRevision.empty()||i.projectionContextId.empty()))
        throw Invalid("incomplete_crew_identity");
    return i;
}
inline std::string identityJSON(const Identity& i) {
    return "{\"producerUUID\":"+quote(i.producerUUID)+",\"raceEpoch\":"+quote(i.raceEpoch)+
        ",\"driverTurnId\":"+quote(i.driverTurnId)+",\"crewIntentRevision\":"+quote(i.crewIntentRevision)+
        ",\"crewRulesRevision\":"+quote(i.crewRulesRevision)+",\"rosterRevision\":"+quote(i.rosterRevision)+
        ",\"inputSessionID\":"+quote(i.inputSessionID)+",\"inputEpoch\":"+quote(i.inputEpoch)+
        ",\"missionRevision\":"+quote(i.missionRevision)+",\"routeRevision\":"+quote(i.routeRevision)+
        ",\"projectionContextId\":"+quote(i.projectionContextId)+",\"layoutProfile\":"+quote(i.layoutProfile)+
        ",\"layoutRevision\":"+quote(i.layoutRevision)+"}";
}

// Physical revisions are opaque SI5 tokens, distinct from the numeric logical
// binding revision. The delimiter is excluded by the existing token grammar.
inline std::string physicalSourceCanonical(const std::string& source,const std::string& binding,
    const std::string& tracking,const std::string& room,const std::string& grip) {
    std::string bytes;
    for(const auto* value:{&source,&binding,&tracking,&room,&grip}) {
        J j;j.kind=J::String;j.string=*value;token(j);
        if(!bytes.empty())bytes+='|';bytes+=*value;
    }
    return bytes;
}

struct BindingProof {
    std::string participantId,sourceId,bindingRevision,physicalFingerprint,verification,challengeSequence;
};
inline BindingProof bindingProof(const J& j) {
    closed(j,{"participantId","sourceId","bindingRevision","physicalFingerprint","verification","challengeSequence"});
    BindingProof p;p.participantId=tok(j,"participantId");p.sourceId=tok(j,"sourceId");
    p.bindingRevision=decimal(at(j,"bindingRevision"));p.physicalFingerprint=text(j,"physicalFingerprint");
    p.verification=text(j,"verification");p.challengeSequence=decimal(at(j,"challengeSequence"));
    if(!hash(p.physicalFingerprint)||(p.verification!="gesture"&&p.verification!="retained"))throw Invalid("invalid_binding_proof");
    return p;
}
struct Request {
    std::string requestId,fingerprint,operation,profileId,challengeId;
    Identity expected;bool required=false;
    std::vector<BindingProof> bindings;
};
inline Request request(const std::string& body,const std::string& operation) {
    if(body.size()>MaxRequestBytes)throw Invalid("crew_request_size");
    const J j=Parser(body).parse();
    if(operation=="intent")closed(j,{"contract","schemaVersion","requestId","expected","required","profileId"});
    else if(operation=="prepare")closed(j,{"contract","schemaVersion","requestId","expected"});
    else if(operation=="bindings-confirm")closed(j,{"contract","schemaVersion","requestId","expected","challengeId","bindings"});
    else throw Invalid("unknown_crew_operation");
    if(text(j,"contract")!=contract()||num(j,"schemaVersion")!=1)throw Invalid("invalid_crew_contract");
    Request r;r.requestId=text(j,"requestId");if(!uuid(r.requestId))throw Invalid("invalid_request_id");
    r.operation=operation;r.expected=identity(at(j,"expected"),operation=="intent");
    r.fingerprint=operation+":"+serialize(j);
    if(operation=="intent") {
        if(at(j,"required").kind!=J::Bool)throw Invalid("invalid_required");
        r.required=at(j,"required").boolean;r.profileId=text(j,"profileId");
        if(!validProfile(r.profileId))throw Invalid("invalid_crew_profile");
    }
    if(operation=="bindings-confirm") {
        r.challengeId=text(j,"challengeId");if(!uuid(r.challengeId))throw Invalid("invalid_challenge_id");
        std::set<std::string> people,sources;
        for(const auto& jproof:arr(at(j,"bindings"),1,5)) {
            const auto p=bindingProof(jproof);
            if(!people.insert(p.participantId).second||!sources.insert(p.sourceId).second)
                throw Invalid("non_bijective_bindings");
            r.bindings.push_back(p);
        }
    }
    return r;
}

class Receipts {
public:
    enum Match {New,Duplicate,Conflict};
    struct Reply {unsigned httpStatus=200;std::string body;};
private:
    struct Entry {std::string fingerprint;Reply reply;};
    std::map<std::string,Entry> m_entries;
    std::deque<std::string> m_order;
public:
    Match check(const Request& r)const {
        const auto entry=m_entries.find(r.requestId);if(entry==m_entries.end())return New;
        return entry->second.fingerprint==r.fingerprint?Duplicate:Conflict;
    }
    const Reply* response(const Request& r)const {
        const auto entry=m_entries.find(r.requestId);
        return entry!=m_entries.end()&&entry->second.fingerprint==r.fingerprint?&entry->second.reply:nullptr;
    }
    // The caller resolves Duplicate/Conflict before comparing live authority.
    // Returning a historical successful prepare never changes the live gate;
    // admission must still consult current state. Entries expire by count only.
    bool commit(const Request& r,const std::string& response,unsigned httpStatus=200) {
        if(check(r)!=New)return false;
        if(m_entries.size()==MaxReceipts){m_entries.erase(m_order.front());m_order.pop_front();}
        Entry entry;entry.fingerprint=r.fingerprint;entry.reply.body=response;entry.reply.httpStatus=httpStatus;
        m_entries.emplace(r.requestId,std::move(entry));m_order.push_back(r.requestId);return true;
    }
    void reset(){m_entries.clear();m_order.clear();}
    size_t size()const{return m_entries.size();}
};
}
#endif
