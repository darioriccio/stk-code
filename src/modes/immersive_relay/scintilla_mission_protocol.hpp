// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_MISSION_PROTOCOL_HPP
#define HEADER_SCINTILLA_MISSION_PROTOCOL_HPP
#include "modes/immersive_relay/scintilla_json.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
namespace ScintillaMissionProtocol {
using namespace ScintillaJSON;
inline bool uuid(const std::string& s) {
    if(s.size()!=36)return false;
    for(size_t i=0;i<s.size();++i)if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
inline std::string decimal(const J& j) {
    const auto s=str(j);if(s.empty()||s.size()>20||(s.size()>1&&s[0]=='0'))throw Invalid("invalid_canonical_decimal");
    uint64_t n=0;for(char c:s){if(c<'0'||c>'9'||n>(UINT64_MAX-uint64_t(c-'0'))/10)throw Invalid("invalid_canonical_decimal");n=n*10+uint64_t(c-'0');}return s;
}
inline std::string optionalToken(const J& j){return str(j).empty()?"":token(j);}
inline bool hash(const std::string& s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline std::string optionalHash(const J& j){const auto s=str(j);if(!s.empty()&&!hash(s))throw Invalid("invalid_hash");return s;}
inline std::string path(const J& j){const auto s=str(j);if(s.empty()||s.size()>240||s[0]=='/'||s.find("..")!=std::string::npos||s.find('\\')!=std::string::npos)throw Invalid("invalid_asset_path");for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='_'||c=='.'||c=='-'))throw Invalid("invalid_asset_path");return s;}
struct Identity {
    std::string producerUUID,raceEpoch,intentRevision,missionRevision,rosterRevision,inputSessionID,inputEpoch,routeRevision,projectionContextId,anchorManifestHash,introManifestHash;
    std::string layoutProfile="front_rear_cargo_v2",layoutRevision="2";
    std::string key()const{return producerUUID+"|"+raceEpoch+"|"+intentRevision+"|"+missionRevision+"|"+rosterRevision+"|"+inputSessionID+"|"+inputEpoch+"|"+routeRevision+"|"+projectionContextId+"|"+anchorManifestHash+"|"+introManifestHash+"|"+layoutProfile+"|"+layoutRevision;}
    bool operator==(const Identity& b)const{return key()==b.key();}
};
inline Identity identity(const J& j){
    closed(j,{"producerUUID","raceEpoch","intentRevision","missionRevision","rosterRevision","inputSessionID","inputEpoch","routeRevision","projectionContextId","anchorManifestHash","introManifestHash","layoutProfile","layoutRevision"});
    Identity i;i.producerUUID=text(j,"producerUUID");if(!uuid(i.producerUUID))throw Invalid("invalid_producer_uuid");
    i.raceEpoch=decimal(at(j,"raceEpoch"));i.intentRevision=decimal(at(j,"intentRevision"));i.missionRevision=decimal(at(j,"missionRevision"));i.rosterRevision=decimal(at(j,"rosterRevision"));i.inputEpoch=decimal(at(j,"inputEpoch"));
    i.inputSessionID=optionalToken(at(j,"inputSessionID"));i.routeRevision=optionalToken(at(j,"routeRevision"));i.projectionContextId=optionalToken(at(j,"projectionContextId"));
    i.anchorManifestHash=optionalHash(at(j,"anchorManifestHash"));i.introManifestHash=optionalHash(at(j,"introManifestHash"));
    i.layoutProfile=text(j,"layoutProfile");i.layoutRevision=decimal(at(j,"layoutRevision"));
    if(!((i.layoutProfile=="front_rear_cargo_v2"&&i.layoutRevision=="2")||(i.layoutProfile=="front_rear_crew_v3"&&i.layoutRevision=="3"))||i.raceEpoch=="0"||i.missionRevision!="1")throw Invalid("invalid_mission_identity");return i;
}
struct Request {std::string requestId,fingerprint,playbackID,variantId,completion;Identity expected;bool required=false;};
inline Request request(const std::string& body,const std::string& operation){
    if(body.size()>16384)throw Invalid("mission_request_size");const J j=Parser(body).parse();
    if(operation=="intent")closed(j,{"contract","schemaVersion","requestId","profileId","profileRevision","expected","required"});
    else if(operation=="prepare")closed(j,{"contract","schemaVersion","requestId","profileId","profileRevision","expected"});
    else if(operation=="intro-complete")closed(j,{"contract","schemaVersion","requestId","profileId","profileRevision","expected","variantId","completion","playbackID"});
    else throw Invalid("unknown_mission_operation");
    if(text(j,"contract")!="tholus.stk-scintilla-mission/1"||num(j,"schemaVersion")!=1||text(j,"profileId")!="sc4_cargo_three_slots_v1"||decimal(at(j,"profileRevision"))!="1")throw Invalid("invalid_mission_profile");
    Request r;r.requestId=text(j,"requestId");if(!uuid(r.requestId))throw Invalid("invalid_request_id");r.expected=identity(at(j,"expected"));r.fingerprint=operation+":"+serialize(j);
    if(operation=="intent"){if(at(j,"required").kind!=J::Bool)throw Invalid("invalid_required");r.required=at(j,"required").boolean;}
    if(operation=="intro-complete"){
        r.variantId=text(j,"variantId");r.completion=text(j,"completion");r.playbackID=text(j,"playbackID");
        if((r.variantId!="C1"&&r.variantId!="C2"&&r.variantId!="C3plus")||(r.completion!="played"&&r.completion!="operator_skipped")||!uuid(r.playbackID))throw Invalid("invalid_intro_receipt");
    }return r;
}
class Receipts {
    std::map<std::string,std::string> m_entries;
    std::deque<std::string> m_order;
public:
    enum Match {New,Duplicate,Conflict};
    // Recent retries retain exact request-ID semantics. After this bounded
    // window, current identity + intrinsic intent/prepare/intro idempotency are
    // authoritative; long races never exhaust a grant merely through reprepare.
    Match check(const Request& r)const{auto i=m_entries.find(r.requestId);if(i!=m_entries.end())return i->second==r.fingerprint?Duplicate:Conflict;return New;}
    bool commit(const Request& r){if(check(r)!=New)return false;if(m_entries.size()==64){m_entries.erase(m_order.front());m_order.pop_front();}m_entries.emplace(r.requestId,r.fingerprint);m_order.push_back(r.requestId);return true;}
    void reset(){m_entries.clear();m_order.clear();}size_t size()const{return m_entries.size();}
};
struct IntroVariant {std::string id,videoPath,videoHash,subtitlesPath,subtitlesHash;unsigned minCrew=0,maxCrew=0;};
struct IntroManifest {std::array<IntroVariant,3> variants;};
inline IntroManifest introManifest(const std::string& body,const std::string& expectedProfile="sc4_cargo_three_slots_v1"){
    const bool crew=expectedProfile=="sc5_crew_standard_v1"||expectedProfile=="sc5_crew_party_v1";
    if(!crew&&expectedProfile!="sc4_cargo_three_slots_v1")throw Invalid("invalid_intro_profile");
    const auto j=Parser(body).parse();
    if(crew)closed(j,{"contract","schemaVersion","profileId","profileRevision","locale","durationSeconds","frameRateNumerator","frameRateDenominator","frameCount","width","height","license","author","variants","crewRulesRevision","layoutProfile","layoutRevision"});
    else closed(j,{"contract","schemaVersion","profileId","profileRevision","locale","durationSeconds","frameRateNumerator","frameRateDenominator","frameCount","width","height","license","author","variants"});
    if(text(j,"contract")!=(crew?"tholus.stk-scintilla-intro/2":"tholus.stk-scintilla-intro/1")||num(j,"schemaVersion")!=(crew?2:1)||text(j,"profileId")!=expectedProfile||decimal(at(j,"profileRevision"))!="1"||text(j,"locale")!="it"||num(j,"durationSeconds")!=40||num(j,"frameRateNumerator")!=30||num(j,"frameRateDenominator")!=1||num(j,"frameCount")!=1200||num(j,"width")!=1920||num(j,"height")!=1080||text(j,"license").empty()||text(j,"license").size()>128||text(j,"author").empty()||text(j,"author").size()>256)throw Invalid("invalid_intro_manifest");
    if(crew&&(decimal(at(j,"crewRulesRevision"))!="1"||text(j,"layoutProfile")!="front_rear_crew_v3"||decimal(at(j,"layoutRevision"))!="3"))throw Invalid("invalid_intro_crew_identity");
    IntroManifest m;std::set<std::string> seen;
    for(const auto& v:arr(at(j,"variants"),3,3)){
        closed(v,{"variantId","minCrew","maxCrew","videoPath","videoSHA256","subtitlesPath","subtitlesSHA256"},{"panels"});IntroVariant a;a.id=text(v,"variantId");
        unsigned slot=a.id=="C1"?0:a.id=="C2"?1:a.id=="C3plus"?2:3;if(slot>=3||!seen.insert(a.id).second)throw Invalid("invalid_intro_variant");
        const auto minimum=num(v,"minCrew"),maximum=num(v,"maxCrew");
        if(minimum!=(slot==2?3:slot+1)||maximum!=(slot==2?5:slot+1))throw Invalid("invalid_intro_crew");
        a.minCrew=unsigned(minimum);a.maxCrew=unsigned(maximum);
        a.videoPath=path(at(v,"videoPath"));a.subtitlesPath=path(at(v,"subtitlesPath"));a.videoHash=text(v,"videoSHA256");a.subtitlesHash=text(v,"subtitlesSHA256");if(!hash(a.videoHash)||!hash(a.subtitlesHash))throw Invalid("invalid_intro_hash");
        if(crew&&!has(v,"panels"))throw Invalid("invalid_intro_panels");
        if(has(v,"panels"))for(const auto& panel:arr(at(v,"panels"),6,6)){closed(panel,{"path","sha256"});path(at(panel,"path"));if(!hash(text(panel,"sha256")))throw Invalid("invalid_panel_hash");}
        m.variants[slot]=a;
    }return m;
}
inline const char* variantForCrew(unsigned crew){return crew==1?"C1":crew==2?"C2":"C3plus";}
struct IntroReceipt {
    std::string key,completion,playbackID;
    bool accept(const std::string& identity,const std::string& value,const std::string& playback){
        if(!key.empty())return key==identity&&completion==value&&playbackID==playback;
        key=identity;completion=value;playbackID=playback;return true;
    }
    bool matches(const std::string& identity)const{return !key.empty()&&key==identity;}
};
}
#endif
