// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_defense.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace ScintillaDefense {
namespace {
Tick ticks(int us,int fps){return (int64_t(us)*fps+999999)/1000000;}
Vec cross(const Vec&a,const Vec&b){return Vec(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
bool unit(Vec* v){double n=v->length();if(!v->finite()||n<1e-6)return false;*v=*v*(1/n);return true;}
}
bool Profile::create(int fps,unsigned crew,Profile* out)
{return create(fps,crew,ScintillaTuning::Defense(),out);}
bool Profile::create(int fps,unsigned crew,const ScintillaTuning::Defense& t,Profile* out)
{
    if(!out||fps<60||fps>1000||crew<1||crew>5)return false;
    const auto us=[](double seconds){return int(std::llround(seconds*1e6));};
    // Per-second amounts become per-tick q units: rate*q/fps == rate*1000.
    const auto rate=[](double perSecond){return int64_t(std::llround(perSecond*1000));};
    Profile p;p.fps=fps;p.q=int64_t(1000)*fps;
    p.warning=ticks(us(crew==1?t.telegraphSoloS:t.telegraphCrewS),fps);p.hold=ticks(us(t.holdS),fps);p.margin=ticks(us(t.marginS),fps);
    p.gap=ticks(us(t.gapS),fps);p.attackerGap=ticks(us(t.attackerGapS),fps);p.protection=ticks(us(t.protectionS),fps);
    p.boostAcquire=ticks(us(t.boostAcquireS),fps);p.boostCooldown=ticks(us(t.boostCooldownS),fps);p.maxFlight=int64_t(us(t.maxFlightS))*fps/1000000;
    p.technicalRetry=ticks(us(t.technicalRetryS),fps);p.maxPause=ticks(us(t.maxPauseS),fps);
    p.activation=std::llround(t.activationCost*p.q);p.boostCost=std::llround(t.boostCost*p.q);
    p.counterCost=rate(t.counterCostPerS);p.maintainCost=rate(t.maintainCostPerS);
    p.counterHeat=rate(t.counterHeatPerS);p.maintainHeat=rate(t.maintainHeatPerS);
    p.regen=rate(t.regenPerS);p.cooling=rate(t.coolingPerS);
    p.minDistance=t.minDistanceM;p.maxDistance=t.maxDistanceM;p.speed=t.projectileSpeed;
    p.minFlightS=double(ticks(us(t.minFlightS),fps))/fps;p.maxFlightS=double(p.maxFlight)/fps;
    p.impactRetained=t.impactSpeedRetained;p.inheritVelocity=t.inheritAttackerVelocity;
    p.solveDuringTelegraph=t.launchSolutionDuringTelegraph;p.boostPolicy=t.boostPolicy;
    p.reserve=p.activation+p.counterCost*(p.warning+p.margin);p.admitHeat=100*p.q-p.counterHeat*(p.warning+p.margin);
    if(p.admitHeat<0||p.reserve>100*p.q||!(p.minDistance<p.maxDistance)||!(p.minFlightS<p.maxFlightS)||!(p.speed>0))return false;
    *out=p;return true;
}
int64_t Profile::serviceCost(ServiceKind kind)const {
    if(fps<60||fps>1000||q!=int64_t(1000)*fps)return -1;
    // The normal boost cost is tunable (default BoostCost); Party keeps SC5's constant.
    return kind==ServiceKind::NormalBoost?boostCost:kind==ServiceKind::PartyBoost?int64_t(PartyCost)*q:-1;
}
bool Profile::admitsService(ServiceKind kind,int64_t available)const {
    const int64_t cost=serviceCost(kind);
    return cost>=0&&reserve>=0&&reserve<=100*q&&available>=reserve+cost;
}
bool Ballistic::solve(const Vec&r,const Vec&v,int fps,Ballistic*out)
{
    Profile p;if(fps<60||fps>1000||!Profile::create(fps,1,&p))return false;
    return solve(r,v,p,out);
}
bool Ballistic::solve(const Vec&r,const Vec&v,const Profile& p,Ballistic*out)
{
    if(!out||p.fps<60||p.fps>1000||!r.finite()||!v.finite())return false;
    const double s2=p.speed*p.speed;
    const double vv=v.dot(v),a=vv-s2,b=2*r.dot(v),c=r.dot(r);
    if(!std::isfinite(vv)||!std::isfinite(b)||!std::isfinite(c)||c<1e-12)return false;
    double roots[2]={-1,-1};
    if(std::abs(a)<=1e-12*std::max({vv,s2,1.0})){
        if(std::abs(b)<=1e-12*std::max(std::abs(b),1.0))return false;roots[0]=-c/b;
    }else{
        double d=b*b-4*a*c,guard=1e-12*std::max({b*b,std::abs(4*a*c),1.0});
        if(!std::isfinite(d)||d< -guard)return false;d=std::max(0.0,d);
        const double q=-.5*(b+std::copysign(std::sqrt(d),b));
        if(q==0)roots[0]=-b/(2*a);else{roots[0]=q/a;roots[1]=c/q;}
    }
    double t=std::numeric_limits<double>::infinity();for(double r:roots)if(r>0&&std::isfinite(r))t=std::min(t,r);
    if(t<p.minFlightS||t>p.maxFlightS)return false;
    Vec d=(r+v*t)*(1/(p.speed*t));if(!d.finite()||std::abs(d.length()-1)>1e-9||!unit(&d))return false;
    out->seconds=t;out->direction=d;return true;
}
Sector Runtime::sector(const Vec&delta,const Vec&forward,const Vec&up,Sector retained)
{
    Vec u=up;if(!unit(&u)||!delta.finite())return Sector::None;
    Vec f=forward-u*forward.dot(u);if(!unit(&f))return Sector::None;Vec r=cross(u,f);if(!unit(&r))return Sector::None;
    const double x=delta.dot(r),z=delta.dot(f);if(std::hypot(x,z)<1e-6)return Sector::None;
    const double theta=std::atan2(x,z),pi=3.14159265358979323846;
    auto distance=[&](Sector s){double d=theta-int(s)*pi*.5;return std::abs(std::atan2(std::sin(d),std::cos(d)));};
    if(retained!=Sector::None)return distance(retained)<=50*pi/180?retained:Sector::None;
    for(int i=0;i<4;++i)if(distance(Sector(i))<=40*pi/180)return Sector(i);return Sector::None;
}
const char* Runtime::statusReason(Status s)
{
    switch(s){case Status::ReleaseRequired:return "release_required";case Status::Busy:return "sector_busy";
    case Status::Reinforcing:return "reinforcing";case Status::ReserveProtected:return "defense_reserve_protected";
    case Status::EnergyDepleted:return "energy_depleted";case Status::Overheated:return "overheated";
    case Status::WrongSector:return "wrong_sector";case Status::BoostReady:return "boost_ready";
    case Status::BoostCooldown:return "boost_cooldown";case Status::ThreatActive:return "threat_active";
    case Status::VehicleNotEligible:return "vehicle_not_eligible";case Status::Blocked:return "blocked";default:return "";}
}
bool Runtime::setRequired(bool value)
{if(m_configured&&!value)return false;if(value!=m_required){if(m_intentRevision==UINT64_MAX)return false;m_required=value;++m_intentRevision;}return true;}
void Runtime::reset(bool preserveIntent)
{
    const bool required=preserveIntent&&this->required();const uint64_t revision=m_intentRevision;
    *this=Runtime();m_required=required;m_intentRevision=revision+(!preserveIntent&&required?1:0);
}
bool Runtime::prepare(uint64_t race,uint64_t input,const std::vector<std::string>&crew,int fps,const ScintillaTuning::Defense& tuning)
{
    Profile p;if(!race||!input||!Profile::create(fps,crew.size(),tuning,&p)||std::set<std::string>(crew.begin(),crew.end()).size()!=crew.size())return false;
    for(const auto& person:crew)if(person.empty())return false;
    if(m_configured&&m_race!=race)return false;
    if(m_configured&&m_profile.fps!=fps)return false;
    if(!m_configured){m_energy=100*p.q;m_heat=0;m_now=0;m_nextAttack=0;}
    suspend("prepared");m_profile=p;m_race=race;m_input=input;m_configured=true;m_required=true;m_crew=crew;
    m_tools.assign(crew.size(),Tool());m_feedback.assign(crew.size(),ToolFeedback());m_view.enabled=true;
    for(size_t i=0;i<crew.size();++i)m_feedback[i].participant=crew[i];return true;
}
void Runtime::closeTools(){
    m_owner=-1;m_boostOwner=-1;m_boostWanted=false;
    for(size_t i=0;i<m_tools.size();++i){m_tools[i].blocked=true;m_feedback[i].status=Status::ReleaseRequired;}
}
void Runtime::terminal(const std::string&reason)
{
    uint64_t& count=reason=="blocked"?m_statistics.blocked:reason=="impact_applied"?m_statistics.impacts:reason=="miss"||reason=="world_blocked"?m_statistics.missed:m_statistics.cancelled;
    if(count!=UINT64_MAX)++count;
    // A technical cancellation is no game outcome: with technicalRetry set it
    // pauses the director (and that attacker) only briefly.
    const bool technical=m_profile.technicalRetry>0&&reason!="blocked"&&reason!="impact_applied"&&reason!="miss"&&reason!="world_blocked";
    if(m_view.attacker){
        if(m_cooldowns.size()>=32&&!m_cooldowns.count(m_view.attacker))m_cooldowns.erase(m_cooldowns.begin());
        auto& c=m_cooldowns[m_view.attacker];c.instance=m_view.instance;
        c.last=technical?m_now-m_profile.attackerGap+std::min(m_profile.attackerGap,m_profile.technicalRetry):m_now;
    }
    m_nextAttack=m_now+(technical?m_profile.technicalRetry:m_profile.gap);m_view.phase=Phase::Cooldown;m_view.hasPulse=false;
    m_reason=reason;m_view.outcome=reason=="blocked"?1:reason=="impact_applied"?2:reason=="world_blocked"||reason=="miss"?3:4;m_outcomeUntil=m_now+(m_profile.fps*4+4)/5;m_impactWanted=false;closeTools();
}
void Runtime::cancel(const std::string&reason){if(m_view.phase!=Phase::Idle&&m_view.phase!=Phase::Cooldown)terminal(reason);else m_reason=reason;}
void Runtime::suspend(const std::string&reason){cancel(reason);m_active=false;closeTools();}
Tick Runtime::lastAttack(uint64_t id,uint64_t instance)const
{auto it=m_cooldowns.find(id);return it!=m_cooldowns.end()&&it->second.instance==instance?it->second.last:-m_profile.attackerGap;}
bool Runtime::candidateReady(uint64_t id,uint64_t instance)const{return m_now-lastAttack(id,instance)>=m_profile.attackerGap;}
bool Runtime::chooseCandidate(std::vector<Candidate> candidates,const std::function<CandidateVisibility(const Candidate&)>& visible,Candidate*out)const
{
    if(!out||!visible||candidates.size()>32)return false;
    for(const auto& c:candidates)if(!c.id||!c.instance||!std::isfinite(c.distance)||c.distance<0)return false;
    std::sort(candidates.begin(),candidates.end(),[&](const Candidate&a,const Candidate&b){
        const Tick aa=lastAttack(a.id,a.instance),bb=lastAttack(b.id,b.instance);
        return aa!=bb?aa<bb:a.distance!=b.distance?a.distance<b.distance:a.id<b.id;
    });
    for(const auto& c:candidates){
        const auto proof=visible(c);
        if(proof==CandidateVisibility::Pending)return false;
        if(proof==CandidateVisibility::Visible){*out=c;return true;}
    }
    return false;
}
bool Runtime::readyForCandidate()const
{return m_configured&&m_active&&m_view.phase==Phase::Idle&&m_now>=m_nextAttack&&m_now>=m_protectedUntil&&m_energy>=m_profile.reserve&&m_heat<=m_profile.admitHeat;}
bool Runtime::begin(const Candidate& c,int64_t nowUs)
{
    if(!readyForCandidate()||!c.id||!c.instance||!c.humanInstance||c.sector==Sector::None||!c.origin.finite()||!candidateReady(c.id,c.instance)||m_nextThreat==UINT64_MAX)return false;
    closeTools();m_view=View();m_view.enabled=true;m_view.phase=Phase::AwaitWarning;m_view.threat=++m_nextThreat;
    m_view.attacker=c.id;m_view.instance=c.instance;m_view.humanInstance=c.humanInstance;m_view.sector=c.sector;m_view.pulse=c.origin;
    m_waitStarted=nowUs;m_reason="await_warning_presentation";return true;
}
void Runtime::resources(Tick dt)
{
    for(Tick i=0;i<dt;++i){
        bool charged=false;
        if(m_owner>=0){
            const bool counter=m_view.phase==Phase::Telegraph&&m_tools[m_owner].sector==m_view.sector;
            const int64_t cost=counter?m_profile.counterCost:m_profile.maintainCost,heat=counter?m_profile.counterHeat:m_profile.maintainHeat;
            const bool reserve=!counter&&(m_energy-cost<m_profile.reserve||m_heat+heat>m_profile.admitHeat);
            if(reserve||m_energy<cost||m_heat>=100*m_profile.q){
                auto& f=m_feedback[m_owner];f.status=reserve?Status::ReserveProtected:m_energy<cost?Status::EnergyDepleted:Status::Overheated;
                m_tools[m_owner].blocked=true;m_owner=-1;
            }else{m_energy-=cost;m_heat=std::min(100*m_profile.q,m_heat+heat);charged=true;
                if(m_energy==0||m_heat==100*m_profile.q){m_feedback[m_owner].status=m_energy==0?Status::EnergyDepleted:Status::Overheated;m_tools[m_owner].blocked=true;m_owner=-1;}
            }
        }
        if(!charged){m_energy=std::min(100*m_profile.q,m_energy+m_profile.regen);m_heat=std::max(int64_t(0),m_heat-m_profile.cooling);}
    }
}
Status Runtime::boostStatus(bool humanEligible)const
{
    if(!m_configured)return Status::VehicleNotEligible;
    if(m_owner>=0||m_boostOwner>=0)return Status::Busy;
    if(m_view.phase!=Phase::Idle&&m_view.phase!=Phase::Cooldown)return Status::ThreatActive;
    if(!humanEligible)return Status::VehicleNotEligible;
    if(m_now<m_boostUntil)return Status::BoostCooldown;
    return m_energy-m_profile.boostCost<m_profile.reserve?Status::ReserveProtected:Status::BoostReady;
}
void Runtime::update(Tick tick,int64_t nowUs,bool active,const Observation&o,const std::vector<Beam>&beams)
{
    if(!m_configured||tick<m_now)return;
    const Tick previous=m_now,delta=tick-previous;
    // No new proof earns past time. Catch-up without step evidence cancels the attack.
    if(delta>1&&m_active){m_now=tick;cancel("physics_step_gap");}
    resources(std::min<Tick>(delta,Tick(m_profile.fps)*120));m_now=tick;
    if(m_now>=m_outcomeUntil)m_view.outcome=0;
    if(!active){suspend("gameplay_suspended");return;}m_active=true;
    if(m_view.phase==Phase::Cooldown&&m_now>=m_nextAttack){m_view.phase=Phase::Idle;m_view.hasPulse=false;}
    const bool threat=m_view.phase!=Phase::Idle&&m_view.phase!=Phase::Cooldown;
    if(threat&&(!o.geometryValid||!o.humanEligible)){cancel("opportunity_lost");return;}
    if(threat&&o.visibilityLost){cancel("source_visibility_lost");return;}
    if((m_view.phase==Phase::Telegraph||m_view.phase==Phase::Flying)&&!o.videoContinuous){cancel("presentation_gap");return;}
    // Apply ALL releases/negative proofs before arbitrating new presses. A new
    // epoch on the same source is a new activation even if both edges arrived
    // between gameplay ticks: continuity and activation cost never carry over.
    std::vector<const Beam*> current(m_tools.size(),nullptr);
    std::vector<size_t> order;
    for(size_t i=0;i<m_tools.size();++i){
        Tool&t=m_tools[i];auto&f=m_feedback[i];
        const Beam* b=nullptr;for(const auto& candidate:beams)if(candidate.participant==m_crew[i]){b=&candidate;break;}
        if(!b||!b->held){if(m_owner==int(i))m_owner=-1;if(m_boostOwner==int(i))m_boostOwner=-1;continue;}
        if(b->epoch<t.epoch||(b->epoch==t.epoch&&b->serial<t.serial))continue;
        f.epoch=b->epoch;f.owner=-1;f.threat=m_view.threat;f.sector=b->sector;
        if(t.epoch!=b->epoch){
            if(m_owner==int(i))m_owner=-1;if(m_boostOwner==int(i))m_boostOwner=-1;
            t=Tool();t.epoch=b->epoch;t.start=m_now;t.context=b->context;t.sector=b->sector;t.threat=b->threat;f.status=Status::None;
        }
        t.serial=b->serial;
        if(!b->visible||b->admitted>m_now||t.context!=b->context||t.sector!=b->sector||t.threat!=b->threat){t.blocked=true;f.status=Status::ReleaseRequired;}
        if(t.blocked){if(m_owner==int(i))m_owner=-1;if(m_boostOwner==int(i))m_boostOwner=-1;continue;}
        current[i]=b;order.push_back(i);
    }
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return current[a]->admitted!=current[b]->admitted?current[a]->admitted<current[b]->admitted:m_crew[a]<m_crew[b];});
    for(size_t i:order){
        const Beam* b=current[i];Tool&t=m_tools[i];auto&f=m_feedback[i];
        if(b->context==Context::DefenseSector){
            if(threat&&(m_view.phase!=Phase::Telegraph||b->threat!=m_view.threat)){t.blocked=true;f.status=Status::ReleaseRequired;continue;}
            if(m_owner>=0&&m_owner!=int(i)){t.blocked=true;f.status=Status::Busy;f.owner=m_owner;continue;}
            if(m_owner<0){
                const bool counter=m_view.phase==Phase::Telegraph&&b->sector==m_view.sector;
                if(m_energy<m_profile.activation||(!counter&&m_energy-m_profile.activation<m_profile.reserve)){t.blocked=true;f.status=Status::ReserveProtected;continue;}
                m_energy-=m_profile.activation;m_owner=int(i);m_ownerSince=m_now;
            }
            f.status=m_view.phase==Phase::Telegraph&&b->sector!=m_view.sector?Status::WrongSector:Status::Reinforcing;f.owner=m_owner;
        }else if(b->context==Context::ServiceBoost){
            const Status status=m_owner>=0?Status::Busy:threat?Status::ThreatActive:!o.humanEligible?Status::VehicleNotEligible:m_now<m_boostUntil?Status::BoostCooldown:m_energy-m_profile.boostCost<m_profile.reserve?Status::ReserveProtected:Status::BoostReady;
            f.status=status;
            if(status!=Status::BoostReady){t.blocked=true;continue;}
            if(m_boostOwner>=0&&m_boostOwner!=int(i)){t.blocked=true;f.status=Status::Busy;continue;}
            m_boostOwner=int(i);
            if(m_now-t.start>=m_profile.boostAcquire){m_boostWanted=true;t.blocked=true;f.status=Status::ReleaseRequired;}
        }
    }
    if(m_view.phase==Phase::AwaitWarning||m_view.phase==Phase::LaunchPending||m_view.phase==Phase::AwaitImpact){
        if(nowUs<m_waitStarted||nowUs-m_waitStarted>500000){cancel("presentation_timeout");return;}
        const bool proof=o.presentedThreat==m_view.threat&&o.presentedPhase==m_view.phase&&o.presentedLow>=0&&o.presentedHigh<=nowUs&&o.presentedHigh>=m_waitStarted;
        if(proof&&m_view.phase==Phase::AwaitWarning){m_view.phase=Phase::Telegraph;m_deadline=m_now+m_profile.warning;m_paused=0;closeTools();m_reason="telegraph";}
        else if(proof&&m_view.phase==Phase::AwaitImpact&&o.presentedContact==m_view.contact)m_impactWanted=true;
        // LaunchPending is consumed by the physics adapter after the same proof.
    }
    if(m_view.phase==Phase::Telegraph&&m_profile.boostPolicy==ScintillaTuning::BoostPolicy::Pause&&o.humanTransient){
        // Time stands still for the warning, never for its energy cost.
        m_deadline+=delta;m_paused+=delta;
        if(m_paused>m_profile.maxPause){cancel("pause_exceeded");return;}
    }
    if(m_view.phase==Phase::Telegraph){
        m_view.progress=std::min(1.0,double(m_now-(m_deadline-m_profile.warning))/m_profile.warning);
        if(m_now>=m_deadline){
            const bool blocked=m_owner>=0&&!m_tools[m_owner].blocked&&m_tools[m_owner].sector==m_view.sector&&m_tools[m_owner].threat==m_view.threat&&m_ownerSince<=m_deadline-m_profile.hold;
            if(blocked)terminal("blocked");else{closeTools();m_view.phase=Phase::LaunchPending;m_view.hasPulse=true;m_waitStarted=nowUs;m_reason="launch_pending";}
        }
    }
}
bool Runtime::launch(const Vec&origin,const Vec&velocity)
{
    // A relative launch adds the attacker's own velocity to the muzzle speed.
    const double length=velocity.length();
    if(m_view.phase!=Phase::LaunchPending||!origin.finite()||!velocity.finite()||!(length>0)||
       (m_profile.inheritVelocity?length>m_profile.speed+200:std::abs(length-m_profile.speed)>1e-6))return false;
    m_view.phase=Phase::Flying;m_view.pulse=origin;m_view.hasPulse=true;m_velocity=velocity;m_deadline=m_now+m_profile.maxFlight;m_reason="flying";return true;
}
void Runtime::advancePulse(const Vec&p){if(m_view.phase==Phase::Flying&&p.finite()){m_view.pulse=p;if(m_now>=m_deadline)terminal("miss");}}
void Runtime::contact(bool human,const Vec&p,int64_t nowUs)
{
    if(m_view.phase!=Phase::Flying)return;
    if(!human){terminal("world_blocked");return;}
    m_view.pulse=p;m_view.hasPulse=false;m_view.contact=m_view.threat;m_view.phase=Phase::AwaitImpact;m_waitStarted=nowUs;m_reason="await_impact_presentation";
}
bool Runtime::confirmImpact(bool applied)
{if(!m_impactWanted||m_view.phase!=Phase::AwaitImpact)return false;if(applied){m_protectedUntil=m_now+m_profile.protection;m_impactEvent.race=m_race;m_impactEvent.threat=m_view.threat;m_impactEvent.humanInstance=m_view.humanInstance;m_impactEvent.tick=m_now;}terminal(applied?"impact_applied":"impact_rejected");return true;}
Status Runtime::serviceStatus(ServiceKind kind,bool humanEligible)const {
    if(!m_configured||!m_active||!humanEligible||m_profile.serviceCost(kind)<0||
       m_now>std::numeric_limits<Tick>::max()-m_profile.boostCooldown)return Status::VehicleNotEligible;
    if(m_owner>=0||(kind==ServiceKind::PartyBoost&&(m_boostOwner>=0||m_boostWanted)))return Status::Busy;
    if(m_view.phase!=Phase::Idle&&m_view.phase!=Phase::Cooldown)return Status::ThreatActive;
    if(kind==ServiceKind::PartyBoost&&m_impactEvent.race==m_race&&m_impactEvent.threat&&m_impactEvent.tick==m_now)return Status::ThreatActive;
    if(m_now<m_boostUntil)return Status::BoostCooldown;
    return m_profile.admitsService(kind,m_energy)?Status::BoostReady:Status::ReserveProtected;
}
bool Runtime::canApplyService(ServiceKind kind)const {
    return (kind!=ServiceKind::NormalBoost||m_boostWanted)&&serviceStatus(kind)==Status::BoostReady;
}
bool Runtime::confirmService(ServiceKind kind,bool applied) {
    if(kind!=ServiceKind::NormalBoost&&kind!=ServiceKind::PartyBoost)return false;
    if(kind==ServiceKind::NormalBoost&&!m_boostWanted)return false;
    if(applied&&!canApplyService(kind))return false;
    if(kind==ServiceKind::NormalBoost){m_boostWanted=false;m_boostOwner=-1;}
    if(applied){m_energy-=m_profile.serviceCost(kind);m_boostUntil=m_now+m_profile.boostCooldown;}
    m_reason=applied?"boost_applied":"boost_rejected";return true;
}
bool Runtime::confirmBoost(bool applied){return confirmService(ServiceKind::NormalBoost,applied);}
}
