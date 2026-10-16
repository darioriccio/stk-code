// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_mission_anchors.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace ScintillaMissionAnchors
{
namespace
{
using namespace ScintillaJSON;
bool fail(std::string* reason,const char* message)
{if(reason)*reason=message;return false;}
bool hash(const std::string& value)
{
    if(value.size()!=64)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
bool label(const std::string& value,std::size_t maximum=128)
{
    if(value.empty()||value.size()>maximum)return false;
    for(unsigned char c:value)if(c<32||c==127)return false;
    return true;
}
bool assetPath(const std::string& value)
{
    if(value.empty())return true;
    if(value.size()>256||value[0]=='/'||value.find("..")!=std::string::npos)return false;
    for(char c:value)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'))return false;
    return true;
}
bool position(const Position& p)
{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<=1e6&&std::abs(p.y)<=1e6&&std::abs(p.z)<=1e6;}
bool fraction(double value)
{return std::isfinite(value)&&value>=0&&value<=1;}
bool validLaps(const std::vector<unsigned>& values)
{
    if(values.empty()||values.size()>12)return false;
    std::set<unsigned> seen;
    for(unsigned value:values)if(value>=12||!seen.insert(value).second)return false;
    return true;
}
bool sharesLap(const Group& a,const Group& b)
{
    for(unsigned lap:a.laps)if(std::find(b.laps.begin(),b.laps.end(),lap)!=b.laps.end())return true;
    return false;
}
void validate(const Manifest& m)
{
    if(m.schemaVersion!=1||m.profileId!="sc4_cargo_three_slots_v1"||m.profileRevision!=1)
        throw Invalid("invalid_mission_profile");
    if(m.trackId!="snowtuxpeak"||m.reverse||!hash(m.trackHash))throw Invalid("invalid_mission_track");
    if(m.authoringStatus!="development_unqualified")throw Invalid("invalid_mission_authoring_status");
    if((!m.author.empty()&&!label(m.author,256))||(!m.license.empty()&&!label(m.license,256)))
        throw Invalid("invalid_mission_provenance");
    if(m.groups.empty()||m.groups.size()>12)throw Invalid("invalid_mission_group_count");
    std::set<std::string> ids;
    for(std::size_t index=0;index<m.groups.size();++index)
    {
        const Group& group=m.groups[index];
        if(!label(group.id)||!ids.insert(group.id).second||!validLaps(group.laps)||
           group.activationNode>=4096||group.expiryNode>=4096||
           !fraction(group.activationFraction)||!fraction(group.expiryFraction)||
           group.activationFraction>=group.expiryFraction||group.capsules.empty()||group.capsules.size()>2)
            throw Invalid("invalid_mission_group");
        for(const Capsule& capsule:group.capsules)
            if(!position(capsule.center)||!std::isfinite(capsule.radius)||capsule.radius<0.1||capsule.radius>5||!assetPath(capsule.asset))
                throw Invalid("invalid_mission_capsule");
        if(group.capsules.size()==2)
        {
            const Capsule& a=group.capsules[0];const Capsule& b=group.capsules[1];
            const double x=a.center.x-b.center.x,y=a.center.y-b.center.y,z=a.center.z-b.center.z;
            const double radius=a.radius+b.radius;
            if(x*x+y*y+z*z<radius*radius)throw Invalid("overlapping_mission_capsules");
        }
        for(std::size_t previous=0;previous<index;++previous)
        {
            const Group& other=m.groups[previous];
            if(sharesLap(group,other)&&std::max(group.activationFraction,other.activationFraction)<
               std::min(group.expiryFraction,other.expiryFraction))throw Invalid("overlapping_mission_windows");
        }
    }
    for(unsigned slot=0;slot<m.confirmations.size();++slot)
    {
        const Confirmation& c=m.confirmations[slot];
        if(c.slot!=slot||c.node>=4096||!fraction(c.fraction)||c.fraction<=0||c.fraction>=1||!validLaps(c.laps))
            throw Invalid("invalid_mission_confirmation");
    }
}
std::vector<unsigned> laps(const J& value)
{
    std::vector<unsigned> result;
    for(const J& lap:arr(value,1,12))
    {
        const int64_t n=integer(lap);if(n>=12)throw Invalid("invalid_mission_lap");
        if(std::find(result.begin(),result.end(),unsigned(n))!=result.end())throw Invalid("duplicate_mission_lap");
        result.push_back(unsigned(n));
    }
    std::sort(result.begin(),result.end());return result;
}
unsigned node(const J& object,const char* key)
{
    const int64_t value=num(object,key);if(value>=4096)throw Invalid("invalid_mission_node");
    return unsigned(value);
}
bool sameCapsule(const Capsule& a,const Capsule& b)
{return a.center.x==b.center.x&&a.center.y==b.center.y&&a.center.z==b.center.z&&a.radius==b.radius&&a.asset==b.asset;}
bool equivalent(const Manifest& a,const Manifest& b)
{
    if(a.schemaVersion!=b.schemaVersion||a.profileId!=b.profileId||a.profileRevision!=b.profileRevision||
       a.trackId!=b.trackId||a.trackHash!=b.trackHash||a.reverse!=b.reverse||a.authoringStatus!=b.authoringStatus||
       a.author!=b.author||a.license!=b.license||a.groups.size()!=b.groups.size())return false;
    for(std::size_t i=0;i<a.groups.size();++i)
    {
        const Group& x=a.groups[i];const Group& y=b.groups[i];
        if(x.id!=y.id||x.activationNode!=y.activationNode||x.expiryNode!=y.expiryNode||
           x.activationFraction!=y.activationFraction||x.expiryFraction!=y.expiryFraction||
           x.laps!=y.laps||x.capsules.size()!=y.capsules.size())return false;
        for(std::size_t j=0;j<x.capsules.size();++j)if(!sameCapsule(x.capsules[j],y.capsules[j]))return false;
    }
    for(unsigned i=0;i<a.confirmations.size();++i)
    {
        const Confirmation& x=a.confirmations[i];const Confirmation& y=b.confirmations[i];
        if(x.slot!=y.slot||x.node!=y.node||x.fraction!=y.fraction||x.laps!=y.laps)return false;
    }
    return true;
}
bool nodeContains(const NativeTrack& track,unsigned node,double value)
{
    if(node>=track.nodeFractions.size())return false;
    const double lower=track.nodeFractions[node];
    const double upper=node+1<track.nodeFractions.size()?track.nodeFractions[node+1]:1;
    // About one centimetre on this ~1km track; this only reconciles authored
    // decimal boundaries with native float driveline values, never hit depth.
    const double tolerance=1e-5;
    return value>=lower-tolerance&&value<=upper+tolerance;
}
int priority(Kind kind)
{return kind==Kind::Expiry?0:kind==Kind::Confirmation?1:2;}
}

bool load(const std::string& json,Manifest* out,std::string* reason)
{
    if(!out)return fail(reason,"invalid_mission_output");
    try
    {
        const J root=Parser(json).parse();
        closed(root,{"schemaVersion","profileId","profileRevision","trackId","direction","trackHash","authoringStatus","groups","confirmations"},{"author","license"});
        Manifest m;
        if(num(root,"schemaVersion")!=1)throw Invalid("invalid_mission_schema");
        m.profileId=tok(root,"profileId");m.profileRevision=uint64_t(num(root,"profileRevision"));
        m.trackId=tok(root,"trackId");m.trackHash=text(root,"trackHash");m.authoringStatus=tok(root,"authoringStatus");
        const std::string direction=text(root,"direction");
        if(direction!="forward")throw Invalid("invalid_mission_direction");
        if(has(root,"author"))m.author=text(root,"author");if(has(root,"license"))m.license=text(root,"license");
        for(const J& value:arr(at(root,"groups"),1,12))
        {
            closed(value,{"id","activationNode","expiryNode","activationFraction","expiryFraction","laps","capsules"});
            Group group;group.id=tok(value,"id");group.activationNode=node(value,"activationNode");group.expiryNode=node(value,"expiryNode");
            group.activationFraction=real(at(value,"activationFraction"),0,1);group.expiryFraction=real(at(value,"expiryFraction"),0,1);
            group.laps=laps(at(value,"laps"));
            for(const J& capsule:arr(at(value,"capsules"),1,2))
            {
                closed(capsule,{"center","radius"},{"asset"});const auto& p=arr(at(capsule,"center"),3,3);
                Capsule c;c.center.x=real(p[0],-1e6,1e6);c.center.y=real(p[1],-1e6,1e6);c.center.z=real(p[2],-1e6,1e6);
                c.radius=real(at(capsule,"radius"),0.1,5);if(has(capsule,"asset"))c.asset=text(capsule,"asset");
                group.capsules.push_back(c);
            }
            m.groups.push_back(group);
        }
        std::bitset<3> seen;
        for(const J& value:arr(at(root,"confirmations"),3,3))
        {
            closed(value,{"slot","node","fraction","laps"});const int64_t slot=num(value,"slot");
            if(slot>=3||seen.test(unsigned(slot)))throw Invalid("invalid_mission_confirmation_slot");
            seen.set(unsigned(slot));Confirmation c;c.slot=unsigned(slot);c.node=node(value,"node");
            c.fraction=real(at(value,"fraction"),0,1);c.laps=laps(at(value,"laps"));m.confirmations[c.slot]=c;
        }
        validate(m);*out=m;if(reason)reason->clear();return true;
    }
    catch(const Invalid& e){return fail(reason,e.what());}
    catch(const std::exception&){return fail(reason,"invalid_mission_manifest");}
}

bool validateNative(const Manifest& manifest,const NativeTrack& track,std::string* reason)
{
    try{validate(manifest);}catch(const Invalid& e){return fail(reason,e.what());}
    if(track.trackId!=manifest.trackId||track.reverse||track.reverse!=manifest.reverse||
       !hash(track.trackHash)||track.trackHash!=manifest.trackHash||
       !std::isfinite(track.lapLength)||track.lapLength<=0||track.lapLength>1e6||
       track.nodeFractions.empty()||track.nodeFractions.size()>4096)return fail(reason,"mission_native_track_changed");
    double previous=-1;
    for(double value:track.nodeFractions)
    {
        if(!fraction(value)||value>=1||value<=previous)return fail(reason,"invalid_mission_native_driveline");
        previous=value;
    }
    for(const Group& group:manifest.groups)
        if(!nodeContains(track,group.activationNode,group.activationFraction)||
           !nodeContains(track,group.expiryNode,group.expiryFraction))return fail(reason,"mission_anchor_node_mismatch");
    for(const Confirmation& c:manifest.confirmations)
        if(!nodeContains(track,c.node,c.fraction))return fail(reason,"mission_confirmation_node_mismatch");
    if(reason)reason->clear();return true;
}

bool Traversal::prepare(const Manifest& manifest,unsigned total,unsigned crew)
{
    try{validate(manifest);}catch(const Invalid& e){m_reason=e.what();return false;}
    if(!total||total>12||!crew||crew>5){m_reason="invalid_mission_schedule";return false;}
    if(m_prepared)
    {
        if(total!=m_totalLaps||crew!=m_crew||!equivalent(manifest,m_manifest))
        {m_reason="mission_schedule_changed";return false;}
        return true;
    }
    std::vector<Point> points;points.reserve(612);unsigned obtainable=0;
    for(unsigned lap=0;lap<total;++lap)
    {
        for(unsigned group=0;group<manifest.groups.size();++group)
        {
            const Group& g=manifest.groups[group];
            if(std::find(g.laps.begin(),g.laps.end(),lap)==g.laps.end())continue;
            for(unsigned capsule=0;capsule<g.capsules.size();++capsule)
            {
                Point p;p.crossing.group=group;p.crossing.capsule=capsule;p.crossing.lap=lap;
                p.crossing.ordinal=lap*24+group*2+capsule;p.crossing.fraction=g.activationFraction;
                p.crossing.skipped=crew==1&&capsule>0;p.progress=lap+g.activationFraction;
                if(!p.crossing.skipped)++obtainable;
                points.push_back(p);p.crossing.kind=Kind::Expiry;p.crossing.fraction=g.expiryFraction;
                p.progress=lap+g.expiryFraction;points.push_back(p);
            }
        }
        for(const Confirmation& c:manifest.confirmations)
        {
            if(std::find(c.laps.begin(),c.laps.end(),lap)==c.laps.end())continue;
            Point p;p.crossing.kind=Kind::Confirmation;p.crossing.slot=c.slot;p.crossing.lap=lap;
            p.crossing.ordinal=lap*3+c.slot;p.crossing.fraction=c.fraction;p.progress=lap+c.fraction;
            points.push_back(p);
        }
    }
    if(obtainable<3){m_reason="mission_schedule_insufficient_opportunities";return false;}
    std::sort(points.begin(),points.end(),[](const Point& a,const Point& b){
        if(a.progress!=b.progress)return a.progress<b.progress;
        if(priority(a.crossing.kind)!=priority(b.crossing.kind))return priority(a.crossing.kind)<priority(b.crossing.kind);
        if(a.crossing.group!=b.crossing.group)return a.crossing.group<b.crossing.group;
        if(a.crossing.capsule!=b.crossing.capsule)return a.crossing.capsule<b.crossing.capsule;
        return a.crossing.slot<b.crossing.slot;
    });
    m_manifest=manifest;m_totalLaps=total;m_crew=crew;m_points=points;
    m_prepared=true;m_valid=true;m_reason="prepared";return true;
}

void Traversal::reset(){*this=Traversal();}

std::vector<Crossing> Traversal::update(int lap,double value,bool continuous)
{
    std::vector<Crossing> crossings;m_valid=true;
    if(!m_prepared){m_valid=false;m_reason="unprepared";return crossings;}
    if(!std::isfinite(value)||lap< -1||lap>=int(m_totalLaps)||
       (lap>=0&&!fraction(value))||(m_haveSample&&lap<m_previousLap))
    {m_valid=false;m_reason="invalid_mission_progress";return crossings;}
    if(lap<0){m_reason="before_native_start";return crossings;}
    const bool initial=!m_haveSample;
    const double progress=lap+value,previous=m_previousLap+m_previousFraction;
    const bool discontinuous=!continuous||initial||lap>m_previousLap+1||
        (lap==m_previousLap&&value-m_previousFraction>0.5);
    m_haveSample=true;m_previousLap=lap;m_previousFraction=value;
    if(!initial&&progress<=previous){m_reason="stationary_or_backward";return crossings;}
    if(!initial&&progress<=m_highWater){m_reason="previously_traversed";return crossings;}
    for(const Point& point:m_points)
    {
        if(point.progress>progress||(!initial&&point.progress<=m_highWater))continue;
        Crossing crossing=point.crossing;bool seen=false;
        if(crossing.kind==Kind::Activation)
        {seen=m_activated.test(crossing.ordinal);m_activated.set(crossing.ordinal);}
        else if(crossing.kind==Kind::Expiry)
        {seen=m_expired.test(crossing.ordinal);m_expired.set(crossing.ordinal);}
        else {seen=m_confirmed.test(crossing.ordinal);m_confirmed.set(crossing.ordinal);}
        if(seen)continue;
        crossing.skipped=crossing.skipped||discontinuous;crossings.push_back(crossing);
    }
    m_highWater=std::max(m_highWater,progress);
    m_reason=discontinuous?"segment_skipped":"continuous";return crossings;
}
}
