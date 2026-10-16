// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_crew_zones.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
namespace ScintillaCrewZones {
namespace {
using namespace ScintillaJSON;
bool hash(const std::string& s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool boolean(const J& j){if(j.kind!=J::Bool)throw Invalid("party_zone_boolean");return j.boolean;}
bool fail(std::string* reason,const char* s){if(reason)*reason=s;return false;}
bool uuid(const std::string& s) {
    if(s.size()!=36)return false;
    for(size_t i=0;i<s.size();++i)if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
}
bool load(const std::string& bytes,Manifest* out,std::string* reason) {
    try {
        if(!out||bytes.size()>16384)return fail(reason,"party_zone_size");
        const J j=Parser(bytes).parse();closed(j,{"schemaVersion","profileId","crewRulesRevision","trackId","trackHash","reverse","authoringStatus","author","license","speedBoundMetersPerSecond","marginSeconds","positionErrorMeters","finalExclusionMeters","candidateNodes","zones"});
        if(num(j,"schemaVersion")!=1||text(j,"profileId")!="sc5_crew_party_v1"||text(j,"crewRulesRevision")!="1")return fail(reason,"party_zone_profile");
        Manifest m;m.trackId=tok(j,"trackId");m.trackHash=text(j,"trackHash");m.reverse=boolean(at(j,"reverse"));
        m.status=text(j,"authoringStatus");m.author=text(j,"author");m.license=text(j,"license");
        if(!hash(m.trackHash)||(m.status!="development_unqualified"&&m.status!="authored")||m.author.empty()||m.author.size()>256||m.license.empty()||m.license.size()>128)return fail(reason,"party_zone_provenance");
        m.speedBound=real(at(j,"speedBoundMetersPerSecond"),1,100);
        m.margin=real(at(j,"marginSeconds"),0,10);m.positionError=real(at(j,"positionErrorMeters"),0,5);
        m.finalExclusion=real(at(j,"finalExclusionMeters"),1,500);
        std::set<unsigned> nodes;for(const auto& n:arr(at(j,"candidateNodes"),0,6)){const auto v=integer(n);if(v>4095||!nodes.insert(unsigned(v)).second)return fail(reason,"party_zone_candidates");m.candidates.push_back(unsigned(v));}
        std::set<std::string> ids;
        for(const auto& z:arr(at(j,"zones"),0,6)) {
            closed(z,{"id","startNode","endNode","startMeters","endMeters","minimumWidthMeters"});
            Zone v;v.id=tok(z,"id");const auto a=num(z,"startNode"),b=num(z,"endNode");
            if(a>4095||b>4095||a==b||!ids.insert(v.id).second)return fail(reason,"party_zone_identity");
            v.startNode=unsigned(a);v.endNode=unsigned(b);v.start=real(at(z,"startMeters"),0,100000);
            v.end=real(at(z,"endMeters"),0,100000);v.width=real(at(z,"minimumWidthMeters"),6,100);
            if(v.start>=v.end)return fail(reason,"party_zone_interval");m.zones.push_back(v);
        }
        std::sort(m.zones.begin(),m.zones.end(),[](const Zone& a,const Zone& b){return a.start<b.start;});
        for(size_t i=1;i<m.zones.size();++i)if(m.zones[i].start<m.zones[i-1].end)return fail(reason,"party_zone_overlap");
        if(m.status=="authored"&&m.zones.empty())return fail(reason,"party_zone_empty");
        *out=m;if(reason)reason->clear();return true;
    }catch(const std::exception&){return fail(reason,"party_zone_schema");}
}
double guardDistance(const Manifest& m,double extent) {
    if(!std::isfinite(extent)||extent<=0||extent>20)return -1;
    const double d=m.speedBound*(0.5+0.25+m.margin)+m.positionError+extent;
    return std::isfinite(d)&&d>0?d:-1;
}
bool validateNative(const Manifest& m,const std::string& id,const std::string& h,bool reverse,double lap,const std::vector<double>& nodes,std::string* reason) {
    if(id!=m.trackId||h!=m.trackHash||reverse!=m.reverse||!std::isfinite(lap)||lap<=0||m.finalExclusion>=lap)return fail(reason,"party_zone_native_track");
    for(unsigned n:m.candidates)if(n>=nodes.size())return fail(reason,"party_zone_native_node");
    for(const auto& z:m.zones) {
        if(z.startNode>=nodes.size()||z.endNode>=nodes.size()||!std::isfinite(nodes[z.startNode])||!std::isfinite(nodes[z.endNode])||
           std::abs(nodes[z.startNode]-z.start)>0.001||std::abs(nodes[z.endNode]-z.end)>0.001||z.end>lap-m.finalExclusion)
            return fail(reason,"party_zone_native_interval");
    }
    if(reason)reason->clear();return true;
}
bool qualify(const Manifest& m,const std::string& bytes,const std::string& manifestHash,const std::string& evidenceHash,const std::string& buildHash,const std::string& assetHash,int fps,std::string* reason) {
    try {
        if(bytes.size()>16384||m.status!="authored"||m.zones.empty()||!hash(manifestHash)||!hash(evidenceHash)||!hash(buildHash)||!hash(assetHash)||fps<60||fps>1000)return fail(reason,"party_zones_unqualified");
        const J j=Parser(bytes).parse();closed(j,{"schemaVersion","contract","profileId","crewRulesRevision","manifestSHA256","trackHash","nativeBuildSHA256","trackAssetsSHA256","evidenceSHA256","zones"});
        if(num(j,"schemaVersion")!=1||text(j,"contract")!="tholus.stk-scintilla-party-zones/1"||text(j,"profileId")!="sc5_crew_party_v1"||text(j,"crewRulesRevision")!="1"||text(j,"manifestSHA256")!=manifestHash||text(j,"trackHash")!=m.trackHash||text(j,"nativeBuildSHA256")!=buildHash||text(j,"trackAssetsSHA256")!=assetHash||text(j,"evidenceSHA256")!=evidenceHash)return fail(reason,"party_zone_qualification_identity");
        const auto& zones=arr(at(j,"zones"),m.zones.size(),m.zones.size());std::set<std::string> seen;
        for(const auto& row:zones) {
            closed(row,{"zoneId","geometryClear","noRampOrJump","brakeBefore","brakeDuring","brakeAccelOrders","analogBrake","laterNativePad","observedSpeedBoundMetersPerSecond","observedPositionErrorMeters","physicsFPS"});
            const auto id=tok(row,"zoneId");if(!seen.insert(id).second||std::none_of(m.zones.begin(),m.zones.end(),[&](const Zone& z){return z.id==id;}))return fail(reason,"party_zone_qualification_set");
            for(auto k:{"geometryClear","noRampOrJump","brakeBefore","brakeDuring","brakeAccelOrders","analogBrake","laterNativePad"})if(!boolean(at(row,k)))return fail(reason,"party_zone_qualification_incomplete");
            if(real(at(row,"observedSpeedBoundMetersPerSecond"),0,100)<m.speedBound||real(at(row,"observedPositionErrorMeters"),0,5)>m.positionError||num(row,"physicsFPS")!=fps)return fail(reason,"party_zone_qualification_envelope");
        }
        if(reason)reason->clear();return true;
    }catch(const std::exception&){return fail(reason,"party_zone_qualification_schema");}
}
bool nativeEvidence(const Manifest& m,const std::string& bytes,const std::string& manifestHash,const std::string& buildHash,const std::string& assetHash,int fps,std::vector<std::string>* artifacts,std::string* reason) {
    try {
        if(!artifacts||bytes.size()>65536||m.status!="authored"||m.zones.empty()||fps<60||fps>1000||
           !hash(manifestHash)||!hash(buildHash)||!hash(assetHash))return fail(reason,"party_zone_evidence_required");
        const J j=Parser(bytes).parse();closed(j,{"schemaVersion","contract","sourceKind","synthetic","producerUUID","operatorId","manifestSHA256","trackHash","nativeBuildSHA256","trackAssetsSHA256","physicsFPS","zones"});
        if(num(j,"schemaVersion")!=1||text(j,"contract")!="tholus.stk-scintilla-party-zones-evidence/1"||
           text(j,"sourceKind")!="native_runtime"||boolean(at(j,"synthetic"))||!uuid(text(j,"producerUUID"))||
           tok(j,"operatorId").empty()||text(j,"manifestSHA256")!=manifestHash||text(j,"trackHash")!=m.trackHash||
           text(j,"nativeBuildSHA256")!=buildHash||text(j,"trackAssetsSHA256")!=assetHash||num(j,"physicsFPS")!=fps)
            return fail(reason,"party_zone_evidence_identity");
        std::set<std::string> seen,proofs;
        for(const auto& row:arr(at(j,"zones"),m.zones.size(),m.zones.size())) {
            closed(row,{"zoneId","geometryClear","noRampOrJump","observedSpeedBoundMetersPerSecond","observedPositionErrorMeters","checks"});
            const auto id=tok(row,"zoneId");
            if(!seen.insert(id).second||std::none_of(m.zones.begin(),m.zones.end(),[&](const Zone& z){return z.id==id;})||
               !boolean(at(row,"geometryClear"))||!boolean(at(row,"noRampOrJump"))||
               real(at(row,"observedSpeedBoundMetersPerSecond"),0,100)<m.speedBound||
               real(at(row,"observedPositionErrorMeters"),0,5)>m.positionError)return fail(reason,"party_zone_evidence_envelope");
            const auto& checks=at(row,"checks");closed(checks,{"brakeBefore","brakeDuring","brakeAccelOrders","analogBrake","laterNativePad"});
            for(auto name:{"brakeBefore","brakeDuring","brakeAccelOrders","analogBrake","laterNativePad"}) {
                const auto& proof=at(checks,name);closed(proof,{"passed","nativeRunId","artifactSHA256"});
                const auto artifact=text(proof,"artifactSHA256");
                if(!boolean(at(proof,"passed"))||!uuid(text(proof,"nativeRunId"))||!hash(artifact))return fail(reason,"party_zone_evidence_check");
                proofs.insert(artifact);
            }
        }
        std::vector<std::string> result(proofs.begin(),proofs.end());*artifacts=std::move(result);
        if(reason)reason->clear();return true;
    }catch(const std::exception&){return fail(reason,"party_zone_evidence_schema");}
}
bool Tracker::update(const Sample& s,int fps,double lap,double bound) {
    const bool valid=s.tick>=0&&s.lap>=0&&s.lap<12&&fps>=60&&fps<=1000&&std::isfinite(lap)&&lap>0&&
        std::isfinite(bound)&&bound>0&&std::isfinite(s.distance)&&s.distance>=0&&s.distance<lap&&
        std::isfinite(s.x)&&std::isfinite(s.y)&&std::isfinite(s.z)&&std::isfinite(s.speed)&&s.speed>0&&s.speed<=bound+0.001&&s.onRoad&&!s.animated;
    if(!valid){reset();return false;}
    if(m_known&&s.tick==m_previous.tick) {
        // Idempotence is useful for the final same-thread physical recheck;
        // changed coordinates on the same tick are a discontinuity.
        if(s.lap!=m_previous.lap||s.distance!=m_previous.distance||s.x!=m_previous.x||s.y!=m_previous.y||s.z!=m_previous.z){reset();return false;}
        return m_continuous;
    }
    const double step=bound/fps+0.05;
    const double dx=s.x-m_previous.x,dy=s.y-m_previous.y,dz=s.z-m_previous.z;
    const double advance=(s.lap-m_previous.lap)*lap+s.distance-m_previous.distance;
    m_continuous=m_known&&s.tick==m_previous.tick+1&&advance>=0&&advance<=step*2&&dx*dx+dy*dy+dz*dz<=step*step;
    m_known=true;m_previous=s;return m_continuous;
}
bool Tracker::available(const Manifest& m,double lap,unsigned totalLaps,double extent)const {
    const double guard=guardDistance(m,extent);
    if(!m_known||!m_continuous||m.status!="authored"||guard<=0||totalLaps<1||totalLaps>12||unsigned(m_previous.lap)>=totalLaps||
       m_previous.distance>=lap-m.finalExclusion||m.finalExclusion<guard)return false;
    for(const auto& zone:m.zones)if(m_previous.distance>=zone.start&&zone.end-m_previous.distance>=guard)return true;
    return false;
}
}
