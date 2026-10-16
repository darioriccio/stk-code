// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_DEFENSE_HPP
#define HEADER_SCINTILLA_DEFENSE_HPP
#include "modes/immersive_relay/scintilla_layout.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace ScintillaDefense {
using Tick=int64_t;
enum class ServiceKind {NormalBoost=0,PartyBoost=1};
enum class Phase {Idle=0,AwaitWarning=1,Telegraph=2,LaunchPending=3,Flying=4,AwaitImpact=5,Cooldown=6};
// Stable, additive codes shared with shaders and source feedback.
enum class Status {None=0,ReleaseRequired=1,Busy=2,Reinforcing=3,ReserveProtected=4,EnergyDepleted=5,Overheated=6,WrongSector=7,BoostReady=8,BoostCooldown=9,ThreatActive=10,VehicleNotEligible=11,Blocked=12};
struct Profile {
    enum {BoostCost=25,PartyCost=15};
    int fps=0;Tick warning=0,hold=0,margin=0,gap=0,attackerGap=0,protection=0,boostAcquire=0,boostCooldown=0,maxFlight=0;
    int64_t q=0,reserve=0,admitHeat=0;
    static bool create(int fps,unsigned crew,Profile* out);
    int64_t serviceCost(ServiceKind)const;
    bool admitsService(ServiceKind,int64_t availableEnergy)const;
};
struct Beam {
    std::string participant;uint64_t epoch=0,serial=0,threat=0;
    Context context=Context::None;Sector sector=Sector::None;
    bool held=false,visible=false;Tick admitted=0;
};
struct ToolFeedback {std::string participant;uint64_t epoch=0,threat=0;Sector sector=Sector::None;Status status=Status::None;int owner=-1;};
struct Candidate {uint64_t id=0,instance=0,humanInstance=0;Sector sector=Sector::None;Vec origin,velocity;double distance=0;};
enum class CandidateVisibility {Pending,Visible,Rejected};
struct View {
    bool enabled=false;Phase phase=Phase::Idle;uint64_t threat=0,contact=0,attacker=0,instance=0,humanInstance=0;
    Sector sector=Sector::None;Vec pulse;bool hasPulse=false;double progress=0;int outcome=0;
};
struct ImpactEvent {uint64_t race=0,threat=0,humanInstance=0;Tick tick=0;};
struct Statistics {uint64_t cancelled=0,blocked=0,impacts=0,missed=0;};
struct Observation {
    bool geometryValid=false,humanEligible=false,videoContinuous=false,visibilityLost=false;
    uint64_t presentedThreat=0,presentedContact=0;Phase presentedPhase=Phase::Idle;
    int64_t presentedLow=0,presentedHigh=0;
};
struct Ballistic {Vec direction;double seconds=0;static bool solve(const Vec& r,const Vec& velocity,int fps,Ballistic* out);};
class Runtime {
public:
    bool setRequired(bool value);
    bool required()const{return m_required||m_configured;}
    uint64_t intentRevision()const{return m_intentRevision;}
    bool prepare(uint64_t race,uint64_t input,const std::vector<std::string>& crew,int fps);
    void reset(bool preserveIntent=true);
    void suspend(const std::string& reason);
    void update(Tick tick,int64_t nowUs,bool active,const Observation&,const std::vector<Beam>&);
    bool begin(const Candidate&,int64_t nowUs);
    bool launch(const Vec& origin,const Vec& velocity);
    void advancePulse(const Vec& point);
    void contact(bool human,const Vec& point,int64_t nowUs);
    bool confirmImpact(bool applied);
    bool confirmBoost(bool applied);
    Status serviceStatus(ServiceKind,bool humanEligible=true)const;
    bool canApplyService(ServiceKind)const;
    bool confirmService(ServiceKind,bool applied);
    int64_t energyUnits()const{return m_energy;}
    bool wantsImpact()const{return m_impactWanted;}
    bool wantsBoost()const{return m_boostWanted;}
    bool canApplyBoost()const{return canApplyService(ServiceKind::NormalBoost);}
    bool readyForCandidate()const;
    bool candidateReady(uint64_t id,uint64_t instance)const;
    // Candidates already passed live geometry/eligibility checks. A pending
    // higher-priority proof must finish before considering a lower priority.
    bool chooseCandidate(std::vector<Candidate>,const std::function<CandidateVisibility(const Candidate&)>&,Candidate*)const;
    Tick lastAttack(uint64_t id,uint64_t instance)const;
    bool configured()const{return m_configured;}
    uint64_t race()const{return m_race;}
    uint64_t inputEpoch()const{return m_input;}
    const std::vector<std::string>& crew()const{return m_crew;}
    const Profile& profile()const{return m_profile;}
    const View& view()const{return m_view;}
    const std::vector<ToolFeedback>& feedback()const{return m_feedback;}
    const ImpactEvent& impactEvent()const{return m_impactEvent;}
    const Statistics& statistics()const{return m_statistics;}
    int owner()const{return m_owner;}
    int boostOwner()const{return m_boostOwner;}
    Status boostStatus(bool humanEligible)const;
    const std::string& reason()const{return m_reason;}
    Tick now()const{return m_now;}
    Tick deadline()const{return m_deadline;}
    Tick nextAttack()const{return m_nextAttack;}
    Tick protectionUntil()const{return m_protectedUntil;}
    Tick boostUntil()const{return m_boostUntil;}
    double energy()const{return m_profile.q?double(m_energy)/m_profile.q:0;}
    double heat()const{return m_profile.q?double(m_heat)/m_profile.q:0;}
    double boostProgress()const{return m_boostOwner<0?0:std::min(1.0,double(m_now-m_tools[m_boostOwner].start)/m_profile.boostAcquire);}
    const Vec& velocity()const{return m_velocity;}
    void cancel(const std::string& reason);
    static Sector sector(const Vec& delta,const Vec& forward,const Vec& up,Sector retained=Sector::None);
    static const char* statusReason(Status);
private:
    struct Tool {uint64_t epoch=0,serial=0;bool blocked=false;Tick start=0;Context context=Context::None;Sector sector=Sector::None;uint64_t threat=0;};
    struct Cooldown {uint64_t instance=0;Tick last=0;};
    bool m_required=false,m_configured=false,m_active=false,m_impactWanted=false,m_boostWanted=false;
    uint64_t m_intentRevision=0,m_race=0,m_input=0,m_nextThreat=0;
    Tick m_now=0,m_deadline=0,m_nextAttack=0,m_protectedUntil=0,m_boostUntil=0,m_ownerSince=0,m_outcomeUntil=0;
    int64_t m_waitStarted=0,m_energy=0,m_heat=0;
    int m_owner=-1,m_boostOwner=-1;
    Profile m_profile;View m_view;ImpactEvent m_impactEvent;Statistics m_statistics;Vec m_velocity;
    std::string m_reason="unprepared";
    std::vector<std::string> m_crew;std::vector<Tool> m_tools;std::vector<ToolFeedback> m_feedback;
    std::map<uint64_t,Cooldown> m_cooldowns;
    void closeTools();void terminal(const std::string& reason);void resources(Tick dt);
};
}
#endif
