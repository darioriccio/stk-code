// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_combat.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ScintillaCombat {
bool Profile::create(int fps, Profile* out)
{
    if(!out||fps<1||fps>10000)return false;
    const auto ticks=[fps](int64_t us,bool minimum){return (us*fps+(minimum?999999:0))/1000000;};
    Profile p;
    p.acquire=ticks(600000,true);p.breach=ticks(3500000,false);
    p.cooperation=ticks(1250000,false);p.soloGap=ticks(2500000,false);p.soloTotal=ticks(8000000,false);
    p.stagger=ticks(1000000,true);p.recovery=ticks(4000000,true);
    if(!p.breach||!p.cooperation||!p.soloGap)return false;
    *out=p;return true;
}
bool Profile::inRange(double d2,double radius)
{
    const double sine=std::sin(0.75*3.14159265358979323846/180.0);
    return std::isfinite(d2)&&std::isfinite(radius)&&radius>0&&d2>radius*radius&&
        d2<=std::min(900.0,radius*radius/(sine*sine));
}
double Profile::staggerCurve(double u)
{u=std::max(0.0,std::min(1.0,u));return 16*u*u*(1-u)*(1-u);}
constexpr double StaggerEnvelope::MinimumSupportAlignment;
bool StaggerEnvelope::create(const double lower[3],const double upper[3],
                             double lift,double maximumPitch,StaggerEnvelope* out)
{
    if(!lower||!upper||!out||!std::isfinite(lift)||lift<=0||
       !std::isfinite(maximumPitch)||maximumPitch<0||maximumPitch>1.5707963267948966)return false;
    for(unsigned i=0;i<3;++i)
        if(!std::isfinite(lower[i])||!std::isfinite(upper[i])||lower[i]>=upper[i])return false;
    StaggerEnvelope e;
    const double y=std::max(std::abs(lower[1]),std::abs(upper[1]));
    const double z=std::max(std::abs(lower[2]),std::abs(upper[2]));
    e.radius=std::hypot(y,z);
    if(!std::isfinite(e.radius)||e.radius<=0)return false;
    // |rotation displacement| <= radius*angle. Even a supporting normal
    // with alignment just above the admitted minimum sees outward motion.
    e.pitch=std::min(maximumPitch,MinimumSupportAlignment*lift/e.radius);
    const double c=std::cos(e.pitch),s=std::sin(e.pitch);
    const double ycLo=std::min(lower[1],lower[1]*c),ycHi=std::max(upper[1],upper[1]*c);
    const double zcLo=std::min(lower[2],lower[2]*c),zcHi=std::max(upper[2],upper[2]*c);
    // Interval enclosure of y*cos(theta)-z*sin(theta),
    // z*cos(theta)+y*sin(theta), for every theta in [0,pitch].
    const double lo[3]={lower[0],ycLo-std::max(0.0,upper[2]*s),zcLo+std::min(0.0,lower[1]*s)};
    const double hi[3]={upper[0],ycHi-std::min(0.0,lower[2]*s),zcHi+std::max(0.0,upper[1]*s)};
    for(unsigned i=0;i<3;++i){
        e.center[i]=lo[i]*0.5+hi[i]*0.5;
        e.halfExtents[i]=hi[i]*0.5-lo[i]*0.5+0.02;
        if(!std::isfinite(e.center[i])||!std::isfinite(e.halfExtents[i])||e.halfExtents[i]<=0)return false;
    }
    *out=e;return true;
}
bool StaggerEnvelope::supportingFace(double normalUp,double contactHeight,double bodyBottom)
{
    return std::isfinite(normalUp)&&std::isfinite(contactHeight)&&std::isfinite(bodyBottom)&&
        normalUp>MinimumSupportAlignment&&contactHeight<=bodyBottom;
}
int Runtime::slot(const std::string& person) const
{auto it=std::find(m_crew.begin(),m_crew.end(),person);return it==m_crew.end()?-1:int(it-m_crew.begin());}
bool Runtime::prepare(uint64_t race,uint64_t input,const std::vector<std::string>& crew,int fps)
{
    Profile p;const std::set<std::string> unique(crew.begin(),crew.end());
    if(!race||!input||crew.empty()||crew.size()>5||unique.size()!=crew.size()||unique.count("")||!Profile::create(fps,&p))return false;
    if(configured()&&(race!=m_race||fps!=m_fps))return false;
    if(!configured()){m_race=race;m_profile=p;m_fps=fps;}
    if(m_crew!=crew){suspendInput();m_crew=crew;for(auto& e:m_targets)e.second.view.required=crew.size()==1?5:crew.size()==2?1:2;}
    if(m_input!=input){suspendInput();m_input=input;m_serial=0;}
    return true;
}
void Runtime::reset(bool preserveIntent)
{const bool keep=preserveIntent&&required();*this=Runtime();m_required=keep;}
bool Runtime::setRequired(bool required)
{
    if((!required&&configured())||m_intentRevision==std::numeric_limits<uint64_t>::max())return false;
    m_required=required;++m_intentRevision;return true;
}
void Runtime::close(Target& t)
{
    if(t.view.opener>=0&&size_t(t.view.opener)<m_crew.size())m_tools[m_crew[t.view.opener]].blocked=true;
    const uint64_t id=t.view.target,instance=t.view.instance;
    t.view=Snapshot();t.view.target=id;t.view.instance=instance;
    t.view.required=m_crew.size()==1?5:m_crew.size()==2?1:2;t.view.eligible=t.compatible;
    t.first=t.last=t.acquired=0;t.notBefore=std::max(t.notBefore,m_now);t.soloParticipant.clear();t.events.clear();
}
bool Runtime::nextWindow(Target& t)
{
    if(m_window==std::numeric_limits<uint64_t>::max())return false;
    t.view.window=++m_window;return true;
}
void Runtime::suspendInput()
{
    m_active=false;
    for(auto& entry:m_targets)if(entry.second.view.phase!=Phase::Stagger&&entry.second.view.phase!=Phase::Recovery)close(entry.second);
    m_tools.clear();
}
void Runtime::observeTarget(uint64_t id,uint64_t instance,bool compatible)
{
    if(!configured()||!id||!instance)return;
    auto it=m_targets.find(id);
    if(it==m_targets.end()){
        if(m_targets.size()>=32)return;
        Target t;t.view.target=id;t.view.instance=instance;t.compatible=compatible;
        close(t);it=m_targets.emplace(id,t).first;
    }else if(it->second.view.instance!=instance){
        close(it->second);it->second=Target();it->second.view.target=id;it->second.view.instance=instance;
    }
    Target& t=it->second;t.compatible=compatible;
    if(!compatible&&(t.view.phase==Phase::Acquiring||t.view.phase==Phase::Breached||(t.view.phase==Phase::Shielded&&t.view.count)))close(t);
    t.view.required=m_crew.size()==1?5:m_crew.size()==2?1:2;
    t.view.eligible=compatible&&(t.view.phase==Phase::Shielded||t.view.phase==Phase::Acquiring||t.view.phase==Phase::Breached);
}
void Runtime::retireTarget(uint64_t id)
{auto it=m_targets.find(id);if(it!=m_targets.end()){close(it->second);m_targets.erase(it);}}
void Runtime::expire()
{
    for(auto& e:m_targets){Target& t=e.second;
        if((t.view.phase==Phase::Breached||t.view.phase==Phase::Recovery||
            (t.view.phase==Phase::Shielded&&t.view.count))&&m_now>=t.view.deadline){const Tick expired=t.view.deadline;close(t);t.notBefore=expired;}
    }
}
void Runtime::update(Tick now,bool active,const std::vector<Beam>& beams)
{
    if(!configured()||now<m_now)return;
    const Tick delta=now-m_now;m_now=now;expire();
    if(!active){suspendInput();return;}
    m_active=true;
    // Close lost locks before considering a new opener, irrespective of source order.
    for(auto& e:m_targets){Target& t=e.second;
        if(t.view.phase!=Phase::Acquiring&&t.view.phase!=Phase::Breached)continue;
        const auto found=std::find_if(beams.begin(),beams.end(),[&](const Beam& b){
            return slot(b.participant)==t.view.opener&&b.held&&b.visible&&b.target==t.view.target&&b.instance==t.view.instance&&
                m_tools[b.participant].epoch==b.controlEpoch;
        });
        if(found==beams.end()||!t.compatible)close(t);
    }
    if(m_crew.size()==1)return;
    for(const std::string& person:m_crew){
        const auto b=std::find_if(beams.begin(),beams.end(),[&](const Beam& value){return value.participant==person;});
        Tool& tool=m_tools[person];
        if(b==beams.end()||!b->held){tool=Tool();continue;}
        if(tool.epoch!=b->controlEpoch){tool=Tool();tool.epoch=b->controlEpoch;}
        tool.aimTarget=tool.aimInstance=0;
        if(tool.blocked||!b->visible)continue;
        auto it=m_targets.find(b->target);if(it==m_targets.end())continue;
        Target& t=it->second;
        if(!t.compatible||t.view.instance!=b->instance||!t.view.eligible)continue;
        tool.aimTarget=b->target;tool.aimInstance=b->instance;
        if(t.view.phase==Phase::Shielded){
            if(tool.target&&tool.target!=b->target){tool.blocked=true;continue;}
            if(!nextWindow(t))continue;
            tool.target=b->target;t.view.phase=Phase::Acquiring;t.view.opener=slot(person);t.acquired=0;
        }else if(t.view.phase==Phase::Acquiring&&t.view.opener==slot(person)&&tool.target==b->target){
            // A delayed batch does not prove its intermediate simulation intervals.
            if(delta==1)++t.acquired;
            t.view.progress=std::min(1.0,double(t.acquired)/m_profile.acquire);
            if(t.acquired>=m_profile.acquire){t.view.phase=Phase::Breached;t.view.deadline=m_now+m_profile.breach;}
        }
    }
}
Outcome Runtime::shoot(const Shot& s)
{
    Outcome out;out.reason="combat_unavailable";
    if(!m_active||!configured()||s.race!=m_race||s.inputEpoch!=m_input||!s.serial||s.serial<=m_serial||s.admitted<0||s.admitted>m_now)return out;
    m_serial=s.serial;expire();
    const int person=slot(s.participant);auto it=m_targets.find(s.target);
    if(person<0||it==m_targets.end()||s.event.empty())return out;
    Target& t=it->second;
    if(t.view.instance!=s.instance||!t.view.eligible||!s.historicalEligible){out.reason="target_not_attackable";return out;}
    if(std::find(t.events.begin(),t.events.end(),s.event)!=t.events.end()){out.reason="duplicate_contribution";return out;}
    if(m_crew.size()==1){
        if(t.view.phase!=Phase::Shielded){out.reason="target_protected";return out;}
        if(!t.view.count){
            // A late shot cannot start a new chain in the past after its old opportunity expired.
            if(s.admitted<t.notBefore||m_now-s.admitted>=m_profile.soloGap){out.reason="chain_expired";return out;}
            if(!nextWindow(t)){out.reason="window_exhausted";return out;}
            t.first=s.admitted;t.last=s.admitted;t.soloParticipant=s.participant;
        }else if(t.soloParticipant!=s.participant||s.admitted<t.last||s.admitted>=t.view.deadline||m_now>=t.view.deadline){
            out.reason="chain_expired";return out;
        }
        ++t.view.count;t.last=s.admitted;t.view.deadline=std::min(t.first+m_profile.soloTotal,t.last+m_profile.soloGap);
    }else{
        if(t.view.phase!=Phase::Breached||!s.historicalOpen||!s.admittedOpen||s.window!=t.view.window){out.reason="shield_rebound";return out;}
        if(person==t.view.opener){out.reason="opener_cannot_shoot";return out;}
        if(s.admitted>=t.view.deadline||m_now>=t.view.deadline){out.reason="window_expired";return out;}
        const unsigned mask=1u<<person;
        if(t.view.contributors&mask){out.reason="shooter_already_counted";return out;}
        t.view.contributors|=mask;++t.view.count;
        if(t.view.count==1&&m_crew.size()>=3)t.view.deadline=std::min(t.view.deadline,s.admitted+m_profile.cooperation);
    }
    t.events.push_back(s.event);out.reason="contribution";out.contributed=true;
    if(t.view.count>=t.view.required){
        t.view.phase=Phase::StaggerPending;t.view.eligible=false;
        if(t.view.opener>=0)m_tools[m_crew[t.view.opener]].blocked=true;
        out.requestEffect=true;out.reason="stagger_pending";
        out.effect.race=m_race;out.effect.inputEpoch=m_input;out.effect.target=t.view.target;out.effect.instance=t.view.instance;out.effect.window=t.view.window;
    }
    return out;
}
bool Runtime::confirmEffect(const Effect& e,bool applied)
{
    if(e.race!=m_race||e.inputEpoch!=m_input)return false;
    auto it=m_targets.find(e.target);if(it==m_targets.end())return false;
    Target& t=it->second;
    if(t.view.instance!=e.instance||t.view.window!=e.window||t.view.phase!=Phase::StaggerPending)return false;
    if(applied){t.view.phase=Phase::Stagger;t.view.eligible=false;t.view.deadline=0;}
    else close(t);
    return true;
}
void Runtime::animationFinished(uint64_t id,uint64_t instance)
{
    auto it=m_targets.find(id);if(it==m_targets.end())return;
    Target& t=it->second;
    if(t.view.instance==instance&&t.view.phase==Phase::Stagger){t.view.phase=Phase::Recovery;t.view.deadline=m_now+m_profile.recovery;t.view.eligible=false;}
}
std::vector<ToolSnapshot> Runtime::toolSnapshots() const
{
    std::vector<ToolSnapshot> out;out.reserve(m_crew.size());
    for(const auto& person:m_crew){
        ToolSnapshot view;view.participant=person;
        const auto entry=m_tools.find(person);
        if(m_active&&entry!=m_tools.end()){
            const Tool& tool=entry->second;view.controlEpoch=tool.epoch;
            if(tool.blocked){view.status=ToolStatus::ReleaseRequired;view.target=tool.target;}
            else {
                const auto target=m_targets.find(tool.aimTarget);
                if(target!=m_targets.end()){
                    const auto& t=target->second.view;
                    if(t.instance==tool.aimInstance&&t.opener>=0&&t.opener!=slot(person)&&
                       (t.phase==Phase::Acquiring||t.phase==Phase::Breached)){
                        view.status=ToolStatus::TargetBusy;view.target=t.target;view.owner=t.opener;
                    }
                }
            }
        }
        out.push_back(view);
    }
    return out;
}
Snapshot Runtime::snapshot(uint64_t id) const
{auto it=m_targets.find(id);return it==m_targets.end()?Snapshot():it->second.view;}
std::vector<Snapshot> Runtime::snapshots() const
{std::vector<Snapshot> out;for(const auto& e:m_targets)out.push_back(e.second.view);return out;}
}
