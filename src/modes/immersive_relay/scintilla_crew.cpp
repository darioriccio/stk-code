// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_crew.hpp"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <sstream>
#include <tuple>
#include <limits>
#include <set>
namespace ScintillaCrew {
namespace {
bool token(const std::string& value) {
    if(value.empty()||value.size()>128)return false;
    for(char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
       c=='_'||c=='.'||c==':'||c=='-'))return false;
    return true;
}
bool name(const std::string& value) {
    if(value.empty()||value.size()>128)return false;
    for(unsigned char c:value)if(c<32||c==127)return false;return true;
}
bool validRoster(const std::vector<Participant>& people) {
    if(people.size()<2||people.size()>Profile::MaxParticipants)return false;
    std::set<std::string> unique;
    for(const auto& person:people)
        if(!token(person.id)||!name(person.name)||!unique.insert(person.id).second)return false;
    return true;
}
}
bool Profile::create(int frequency,unsigned supporters,Profile* out) {
    if(!out||frequency<60||frequency>1000||supporters<1||supporters>MaxCrew)return false;
    Profile p;p.fps=frequency;p.crew=supporters;
    p.suggestionInterval=(frequency+3)/4;p.suggestionHold=(INT64_C(600000)*frequency+999999)/1000000;
    p.urgentTether=(INT64_C(1200000)*frequency+999999)/1000000;p.maxWait=30*Tick(frequency);
    p.partyHold=(INT64_C(750000)*frequency+999999)/1000000;p.partyWindow=3*Tick(frequency);
    *out=p;return true;
}
bool Runtime::setIntent(bool value,const std::string& selected) {
    if(selected!="sc5_crew_standard_v1"&&selected!="sc5_crew_party_v1")return false;
    // A held preview has never admitted gameplay. It may be switched or
    // explicitly discarded; even a suspended prepared race remains locked.
    if(configured()&&(m_input!=0||m_turn!=0)&&(!value||selected!=m_profileId))return false;
    if(value==m_required&&selected==m_profileId)return true;
    if(m_intentRevision==std::numeric_limits<uint64_t>::max())return false;
    // required() includes configured(): merely clearing the intent would leave
    // the frozen preview and its roles latched as an SC5 requirement.
    if(!value)reset(false);
    m_required=value;m_profileId=selected;++m_intentRevision;cancelParty(PartyReason::NotPrepared);return true;
}
bool Runtime::sameRoster(const std::vector<Participant>& people)const {
    if(people.size()!=m_records.size())return false;
    for(std::size_t i=0;i<people.size();++i)if(people[i].id!=m_records[i].participant.id||
        people[i].name!=m_records[i].participant.name)return false;
    return true;
}
bool Runtime::bindRace(uint64_t race,const std::vector<Participant>& people,
                       unsigned driver,uint64_t completed,int fps) {
    Profile p;if(!m_required||!race||!validRoster(people)||driver>=people.size()||
        !Profile::create(fps,people.size()-1,&p))return false;
    if(configured())return race==m_race&&completed==m_turn&&driver==m_driver&&
        fps==m_profile.fps&&sameRoster(people);
    if(completed!=0)return false;
    m_race=race;m_turn=0;m_driver=driver;m_profile=p;m_records.reserve(people.size());
    for(const auto& person:people){ParticipantRecord record;record.participant=person;m_records.push_back(record);}
    refreshPartyView(PartyReason::NotPrepared);return true;
}
bool Runtime::prepare(uint64_t race,uint64_t input,uint64_t roster,
                      const std::vector<Participant>& people,unsigned driver,
                      uint64_t completed,int fps) {
    // Validate before bindRace so an invalid first request cannot seed state.
    if(!input||!roster||input<m_input||roster<m_rosterRevision||
        (m_turnNeedsInput&&input<=m_input)||
        (configured()&&roster!=m_rosterRevision&&m_rosterRevision&&input==m_input))return false;
    if(!bindRace(race,people,driver,completed,fps))return false;
    const bool changed=!m_prepared||input!=m_input||roster!=m_rosterRevision;
    m_input=input;m_rosterRevision=roster;m_prepared=true;m_turnNeedsInput=false;
    if(changed){clearAdvice();m_active=false;cancelParty(PartyReason::AuthorityChanged);
        if(input!=m_partyAuthority.inputEpoch)m_partyTools=std::array<PartyTool,Profile::MaxParticipants>();
        m_partyAuthorityBound=false;refreshPartyView(PartyReason::NotPrepared);}
    return true;
}
bool Runtime::observeTurn(uint64_t race,unsigned driver,uint64_t completed,bool administrative) {
    if(!configured()||race!=m_race||driver>=m_records.size()||completed<m_turn)return false;
    if(completed==m_turn)return driver==m_driver;
    const uint64_t difference=completed-m_turn;
    // Recover an authoritative snapshot after a whole missed rotation. A
    // single purported handoff to the unchanged driver remains invalid.
    if(driver==m_driver&&difference<m_records.size())return false;
    const bool anomaly=administrative||difference!=1||driver!=(m_driver+1)%m_records.size();
    if(anomaly)m_countersSaturated=saturatingAdd(&m_rotationAnomalies,1)||m_countersSaturated;
    m_partyUsed=false;m_turn=completed;m_driver=driver;suspend();m_turnNeedsInput=true;
    return true;
}
void Runtime::suspend(){m_prepared=false;m_active=false;clearAdvice();cancelParty(PartyReason::NotPrepared);m_partyAuthorityBound=false;}
void Runtime::reset(bool preserveIntent) {
    const bool required=preserveIntent&&m_required;const auto selected=m_profileId;
    const uint64_t revision=m_intentRevision;
    *this=Runtime();m_required=required;m_intentRevision=revision;
    if(preserveIntent)m_profileId=selected;refreshPartyView(PartyReason::NotPrepared);
}
std::vector<Assignment> Runtime::assignments()const {
    std::vector<Assignment> result;if(!configured())return result;
    result.reserve(m_records.size());const unsigned count=m_records.size();
    for(unsigned i=0;i<count;++i) {
        Assignment a;a.participantId=m_records[i].participant.id;a.name=m_records[i].participant.name;
        a.rosterIndex=i;a.isPilot=i==m_driver;
        a.competence=competenceAtTurn(i,count,m_driver,m_turn);
        result.push_back(a);
    }
    return result;
}
std::vector<std::string> Runtime::supporterIDs()const {
    std::vector<std::string> result;if(!configured())return result;
    for(unsigned j=1;j<m_records.size();++j)result.push_back(m_records[(m_driver+j)%m_records.size()].participant.id);
    return result;
}
const char* Runtime::competenceName(Competence value) {
    switch(value){case Competence::Hacker:return "hacker";case Competence::Gunner:return "gunner";
        case Competence::Defender:return "defender";case Competence::Engineer:return "engineer";
        case Competence::Recon:return "recon";default:return "universal";}
}
namespace {
using TaskKey=std::tuple<int,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t>;
TaskKey taskKey(const Task& t){return TaskKey(int(t.kind),t.targetId,t.instance,t.phaseRevision,t.threatId,t.moduleId);}
bool sameTask(const Task& a,const Task& b){return taskKey(a)==taskKey(b);}
bool matches(const Occupation& occupied,const Task& task){return !occupied.unassigned&&sameTask(occupied,task);}

bool validTask(const Task& t) {
    switch(t.kind) {
    case TaskKind::Offense:return t.targetId&&t.instance&&!t.threatId&&!t.moduleId;
    case TaskKind::Defense:return !t.targetId&&!t.instance&&!t.phaseRevision&&t.threatId&&!t.moduleId;
    case TaskKind::CapsuleShell:case TaskKind::CapsuleCollect:case TaskKind::TetherRecover:
        return t.targetId&&t.instance&&!t.threatId&&t.moduleId;
    case TaskKind::NormalBoost:case TaskKind::PartyBoost:
        return !t.targetId&&!t.instance&&!t.phaseRevision&&!t.threatId&&!t.moduleId;
    }
    return false;
}
bool validOccupation(const Occupation& o) {
    if(!o.unassigned)return validTask(o);
    return unsigned(o.kind)<=unsigned(TaskKind::PartyBoost)&&!o.targetId&&!o.instance&&!o.phaseRevision&&!o.threatId&&!o.moduleId;
}
Tick safeAdd(Tick now,Tick duration) {
    return now>std::numeric_limits<Tick>::max()-duration?std::numeric_limits<Tick>::max():now+duration;
}
std::string stableTaskKey(const Task& task) {
    std::ostringstream out;out<<int(task.kind)<<':'<<task.targetId<<':'<<task.instance<<':'<<task.phaseRevision<<':'<<task.threatId<<':'<<task.moduleId;
    return out.str();
}
Competence affinity(TaskKind kind,unsigned slot) {
    switch(kind){case TaskKind::Offense:return slot==0?Competence::Hacker:Competence::Gunner;
        case TaskKind::Defense:return Competence::Defender;case TaskKind::CapsuleShell:return Competence::Gunner;
        case TaskKind::CapsuleCollect:return Competence::Recon;case TaskKind::TetherRecover:
        case TaskKind::NormalBoost:return Competence::Engineer;default:return Competence::Universal;}
}
OffenseAction offenseAction(const Bundle& fact,const std::string& person,unsigned slot,unsigned crew) {
    if(fact.kind!=TaskKind::Offense)return OffenseAction::None;
    if(crew==1)return OffenseAction::Fire;
    if(fact.offenseOwner.empty())return slot==0?OffenseAction::OpenShield:OffenseAction::WaitForBreach;
    if(person==fact.offenseOwner)return OffenseAction::MaintainShield;
    if(std::find(fact.offenseContributors.begin(),fact.offenseContributors.end(),person)!=fact.offenseContributors.end())return OffenseAction::ContributionComplete;
    return fact.offenseBreached?OffenseAction::Fire:OffenseAction::WaitForBreach;
}
struct Score {
    int defense=0,tether=0,continuity=0,people=0,role=0;
    int64_t waiting=0;std::string stable;
    bool betterThan(const Score& b)const {
        const auto aTuple=std::make_tuple(defense,tether,continuity,people,waiting,role);
        const auto bTuple=std::make_tuple(b.defense,b.tether,b.continuity,b.people,b.waiting,b.role);
        return aTuple!=bTuple?aTuple>bTuple:stable<b.stable;
    }
};
}
bool offenseActionCurrent(const Suggestion& hint,const Bundle& fact,unsigned crew) {
    if(fact.kind!=TaskKind::Offense)return hint.offenseAction==OffenseAction::None;
    if(crew>1&&fact.offenseOwner.empty())
        return hint.offenseAction==OffenseAction::OpenShield||hint.offenseAction==OffenseAction::WaitForBreach;
    return hint.offenseAction==offenseAction(fact,hint.participantId,0,crew);
}
const char* Runtime::offenseActionName(OffenseAction action) {
    switch(action){case OffenseAction::OpenShield:return "open_shield";case OffenseAction::MaintainShield:return "maintain_shield";
        case OffenseAction::WaitForBreach:return "wait_for_breach";case OffenseAction::Fire:return "fire";
        case OffenseAction::ContributionComplete:return "contribution_complete";default:return "none";}
}
bool saturatingAdd(uint64_t* value,uint64_t amount) {
    if(!value)return true;
    if(amount>std::numeric_limits<uint64_t>::max()-*value){*value=std::numeric_limits<uint64_t>::max();return true;}
    *value+=amount;return false;
}
int Runtime::participantIndex(const std::string& person)const {
    for(unsigned i=0;i<m_records.size();++i)if(m_records[i].participant.id==person)return int(i);return -1;
}
void Runtime::clearAdvice() {
    m_suggestions.clear();m_releaseAdvice.clear();m_emergencyKey.clear();m_scheduled=false;
    m_nextSchedule=m_stableUntil=m_now;m_lastEvaluationCount=0;
}
unsigned Runtime::bundleSlots(TaskKind kind,unsigned crew) {
    if(crew<1||crew>Profile::MaxCrew)return 0;
    if(kind==TaskKind::Offense)return std::min(crew,3u);
    if(kind==TaskKind::PartyBoost)return std::min(crew,2u);
    return unsigned(kind)<=unsigned(TaskKind::PartyBoost)?1:0;
}
const char* Runtime::taskName(TaskKind kind) {
    switch(kind){case TaskKind::Offense:return "offense";case TaskKind::Defense:return "defense";
        case TaskKind::CapsuleShell:return "capsule_shell";case TaskKind::CapsuleCollect:return "capsule_collect";
        case TaskKind::TetherRecover:return "tether_recover";case TaskKind::NormalBoost:return "normal_boost";
        case TaskKind::PartyBoost:return "party_boost";}return "";
}
const char* Runtime::contributionName(ContributionKind kind) {
    switch(kind){case ContributionKind::Opening:return "openings";case ContributionKind::ComboContribution:return "comboContributions";
        case ContributionKind::Stagger:return "staggers";case ContributionKind::Defense:return "defenses";
        case ContributionKind::ShellOpened:return "shellsOpened";case ContributionKind::ModuleCollected:return "modulesCollected";
        case ContributionKind::ModuleRecovered:return "modulesRecovered";case ContributionKind::NormalBoost:return "normalBoosts";
        case ContributionKind::PartyConsent:return "partyConsents";}return "";
}
bool Runtime::credit(const std::string& participant,ContributionKind kind,uint64_t serial) {
    const int index=participantIndex(participant);const unsigned category=unsigned(kind);
    if(!m_prepared||!serial||index<0||unsigned(index)==m_driver||category>=9)return false;
    auto& record=m_records[unsigned(index)];if(serial<=record.lastCommitSerial[category])return false;
    record.lastCommitSerial[category]=serial;
    m_countersSaturated=saturatingAdd(&record.contributions[category],1)||m_countersSaturated;return true;
}
bool Runtime::accountPaused(Tick elapsed) {
    if(!configured()||m_active||elapsed<0)return false;
    for(unsigned i=0;i<m_records.size();++i)if(i!=m_driver)
        m_countersSaturated=saturatingAdd(&m_records[i].opportunityTicks[unsigned(OpportunityMetric::Paused)],uint64_t(elapsed))||m_countersSaturated;
    return true;
}
bool Runtime::update(Tick tick,bool active,const std::vector<Bundle>& supplied,
                     const std::vector<std::string>& readyIDs,const std::vector<Occupation>& occupied) {
    auto reject=[&](){
        // A malformed optional hint batch does not erase knowledge of an
        // otherwise valid native tick or make every later batch unrecoverable.
        if(active&&m_prepared&&tick>=m_now&&tick-m_now<=1)m_now=tick;
        clearAdvice();m_active=false;return false;
    };
    if(!configured()||tick<0||tick<m_now||tick-m_now>1||
       (active&&!m_prepared)||supplied.size()>Profile::MaxBundles||readyIDs.size()>Profile::MaxCrew||
       occupied.size()>Profile::MaxCrew)return reject();
    if(!active){m_now=tick;m_active=false;clearAdvice();return true;}
    const Tick elapsed=tick-m_now;
    std::array<bool,Profile::MaxParticipants> ready={{}};
    std::array<const Occupation*,Profile::MaxParticipants> occupation={{}};
    for(const auto& id:readyIDs) {
        const int index=participantIndex(id);if(index<0||unsigned(index)==m_driver||ready[unsigned(index)])return reject();
        ready[unsigned(index)]=true;
    }
    for(const auto& fact:occupied) {
        const int index=participantIndex(fact.participantId);
        if(index<0||unsigned(index)==m_driver||occupation[unsigned(index)]||!validOccupation(fact))return reject();
        occupation[unsigned(index)]=&fact;
    }
    std::vector<Bundle> bundles;std::set<TaskKey> distinct;
    unsigned counts[7]={};std::set<std::pair<uint64_t,uint64_t>> capsules;
    for(auto fact:supplied) {
        if(!validTask(fact)||fact.deadline<0||fact.eligibleParticipantIds.size()>Profile::MaxCrew)return reject();
        if(fact.kind==TaskKind::Offense) {
            const int owner=participantIndex(fact.offenseOwner);
            if((!fact.offenseOwner.empty()&&(owner<0||unsigned(owner)==m_driver||m_profile.crew==1))||
               (fact.offenseBreached&&fact.offenseOwner.empty())||fact.offenseContributors.size()>m_profile.crew||
               (!fact.offenseContributors.empty()&&!fact.offenseBreached))return reject();
            std::set<std::string> contributors;
            for(const auto& id:fact.offenseContributors){const int person=participantIndex(id);
                if(person<0||unsigned(person)==m_driver||id==fact.offenseOwner||!contributors.insert(id).second)return reject();}
        } else if(!fact.offenseOwner.empty()||fact.offenseBreached||!fact.offenseContributors.empty())return reject();
        std::sort(fact.eligibleParticipantIds.begin(),fact.eligibleParticipantIds.end());
        for(unsigned n=0;n<fact.eligibleParticipantIds.size();++n) {
            const int index=participantIndex(fact.eligibleParticipantIds[n]);
            if(index<0||unsigned(index)==m_driver||(n&&fact.eligibleParticipantIds[n]==fact.eligibleParticipantIds[n-1]))return reject();
        }
        if(fact.deadline<=tick||(fact.kind==TaskKind::PartyBoost&&!partyEnabled()))continue;
        if(!distinct.insert(taskKey(fact)).second)return reject();
        ++counts[unsigned(fact.kind)];
        if((fact.kind==TaskKind::CapsuleShell||fact.kind==TaskKind::CapsuleCollect)&&
           !capsules.insert(std::make_pair(fact.targetId,fact.instance)).second)return reject();
        bundles.push_back(fact);
    }
    if(counts[0]>1||counts[1]>1||counts[2]+counts[3]>2||counts[4]>1||counts[5]>1||counts[6]>1)return reject();
    std::sort(bundles.begin(),bundles.end(),[](const Bundle& a,const Bundle& b){return taskKey(a)<taskKey(b);});
    std::vector<unsigned> people;for(unsigned i=0;i<m_records.size();++i)if(i!=m_driver&&ready[i])people.push_back(i);
    std::sort(people.begin(),people.end(),[&](unsigned a,unsigned b){return m_records[a].participant.id<m_records[b].participant.id;});
    std::vector<std::vector<unsigned>> eligible(bundles.size());
    std::array<bool,Profile::MaxParticipants> hasOpportunity={{}};
    std::string emergency;
    for(unsigned b=0;b<bundles.size();++b) {
        const auto& fact=bundles[b];
        for(unsigned person:people)if(std::binary_search(fact.eligibleParticipantIds.begin(),fact.eligibleParticipantIds.end(),m_records[person].participant.id)&&
           (!occupation[person]||matches(*occupation[person],fact)))eligible[b].push_back(person);
        if(fact.kind==TaskKind::Offense&&!fact.offenseOwner.empty()&&
           std::none_of(eligible[b].begin(),eligible[b].end(),[&](unsigned person){return m_records[person].participant.id==fact.offenseOwner;}))eligible[b].clear();
        if(eligible[b].size()>=bundleSlots(fact.kind,m_profile.crew))for(unsigned person:eligible[b])hasOpportunity[person]=true;
        if((fact.kind==TaskKind::Defense&&fact.imminent)||
           (fact.kind==TaskKind::TetherRecover&&fact.deadline-tick<=m_profile.urgentTether))emergency+=stableTaskKey(fact)+"|";
    }
    auto hintValid=[&](const Suggestion& hint) {
        const int index=participantIndex(hint.participantId);if(index<0||!ready[unsigned(index)]||
            (occupation[unsigned(index)]&&!matches(*occupation[unsigned(index)],hint))||hint.expiresAtTick<=tick)return false;
        for(unsigned b=0;b<bundles.size();++b)if(sameTask(bundles[b],hint)&&
            offenseActionCurrent(hint,bundles[b],m_profile.crew)&&
            eligible[b].size()>=bundleSlots(bundles[b].kind,m_profile.crew)&&
            std::find(eligible[b].begin(),eligible[b].end(),unsigned(index))!=eligible[b].end())return true;
        return false;
    };
    bool previousValid=true;for(const auto& hint:m_suggestions)if(!hintValid(hint))previousValid=false;
    for(const auto& advice:m_releaseAdvice) {
        const int person=participantIndex(advice.participantId);
        const bool valid=person>=0&&ready[unsigned(person)]&&occupation[unsigned(person)]&&
            std::any_of(bundles.begin(),bundles.end(),[&](const Bundle& b){return b.kind==TaskKind::Defense&&b.imminent&&
                b.threatId==advice.threatId&&b.deadline==advice.expiresAtTick&&b.deadline>tick&&
                !matches(*occupation[unsigned(person)],b)&&std::binary_search(b.eligibleParticipantIds.begin(),b.eligibleParticipantIds.end(),advice.participantId);});
        if(!valid)previousValid=false;
    }
    // Account the elapsed simulated interval once, before assigning its next
    // hint. Voluntary held input is engagement even without a selected bundle.
    for(unsigned person=0;person<m_records.size();++person)if(person!=m_driver) {
        auto& record=m_records[person];bool hinted=false;
        for(const auto& hint:m_suggestions)if(hint.participantId==record.participant.id&&hintValid(hint)){hinted=true;break;}
        OpportunityMetric metric=OpportunityMetric::NoOpportunity;
        if(!ready[person])metric=OpportunityMetric::NotReady;
        else if(occupation[person])metric=OpportunityMetric::Engaged;
        else if(hasOpportunity[person])metric=hinted?OpportunityMetric::Engaged:OpportunityMetric::Waiting;
        if(elapsed)m_countersSaturated=saturatingAdd(&record.opportunityTicks[unsigned(metric)],uint64_t(elapsed))||m_countersSaturated;
        if(hinted)record.waiting=0;
        else if(elapsed&&metric==OpportunityMetric::Waiting)record.waiting=std::min(m_profile.maxWait,record.waiting+elapsed);
    }
    m_now=tick;m_active=true;
    const bool urgentChange=emergency!=m_emergencyKey;
    const bool force=!m_scheduled||!previousValid||urgentChange;
    if(!force&&(tick<m_nextSchedule||tick<m_stableUntil))return true;
    std::vector<Suggestion> best;Score bestScore;bool found=false;m_lastEvaluationCount=0;
    const auto roles=assignments();
    struct Job {unsigned bundle=0,slot=0;};
    for(unsigned mask=0;mask<(1u<<bundles.size());++mask) {
        std::vector<Job> jobs;bool impossible=false,defense=false,party=false,normal=false;Score base;
        for(unsigned b=0;b<bundles.size();++b)if(mask&(1u<<b)) {
            const auto& fact=bundles[b];const unsigned slots=bundleSlots(fact.kind,m_profile.crew);
            if(eligible[b].size()<slots){impossible=true;break;}
            defense=defense||fact.kind==TaskKind::Defense;party=party||fact.kind==TaskKind::PartyBoost;normal=normal||fact.kind==TaskKind::NormalBoost;
            if(fact.kind==TaskKind::Defense&&fact.imminent)base.defense=1;
            if(fact.kind==TaskKind::TetherRecover&&fact.deadline-tick<=m_profile.urgentTether)base.tether=1;
            for(unsigned slot=0;slot<slots;++slot){Job job;job.bundle=b;job.slot=slot;jobs.push_back(job);}
        }
        if(impossible||jobs.size()>people.size()||jobs.size()>Profile::MaxSuggestions||(party&&(normal||defense)))continue;
        std::array<bool,Profile::MaxParticipants> used={{}};std::vector<Suggestion> assigned;
        std::function<void(unsigned,Score)> visit=[&](unsigned at,Score score) {
            if(at==jobs.size()) {
                ++m_lastEvaluationCount;if(!found||score.betterThan(bestScore)){found=true;bestScore=score;best=assigned;}return;
            }
            const auto job=jobs[at];const auto& fact=bundles[job.bundle];
            for(unsigned person:eligible[job.bundle])if(!used[person]) {
                if(fact.kind==TaskKind::Offense&&!fact.offenseOwner.empty()&&
                   ((job.slot==0)!=(m_records[person].participant.id==fact.offenseOwner)))continue;
                used[person]=true;Suggestion hint;static_cast<Task&>(hint)=static_cast<const Task&>(fact);
                hint.participantId=m_records[person].participant.id;hint.expiresAtTick=fact.deadline;
                hint.offenseAction=offenseAction(fact,hint.participantId,job.slot,m_profile.crew);assigned.push_back(hint);
                Score next=score;++next.people;
                if(occupation[person]&&matches(*occupation[person],fact))++next.continuity;
                const int64_t wait=m_records[person].waiting+1;next.waiting+=wait*wait;
                if(roles[person].competence!=Competence::Universal&&roles[person].competence==affinity(fact.kind,job.slot))++next.role;
                next.stable+=stableTaskKey(fact)+":"+hint.participantId+"|";
                visit(at+1,next);assigned.pop_back();used[person]=false;
            }
        };
        visit(0,base);
    }
    if(!found||m_lastEvaluationCount>15360)return reject();
    m_suggestions=best;m_releaseAdvice.clear();
    for(const auto& hint:m_suggestions){const int index=participantIndex(hint.participantId);m_records[unsigned(index)].waiting=0;}
    // If actual held commitments fill every eligible hand, advice can ask for
    // a release. It is separate from assignments and never fabricates a release
    // or a simultaneous second task for that participant.
    if(!bestScore.defense)for(const auto& fact:bundles)if(fact.kind==TaskKind::Defense&&fact.imminent) {
        for(unsigned person:people)if(occupation[person]&&!matches(*occupation[person],fact)&&
            std::binary_search(fact.eligibleParticipantIds.begin(),fact.eligibleParticipantIds.end(),m_records[person].participant.id)) {
            ReleaseAdvice advice;advice.participantId=m_records[person].participant.id;
            advice.threatId=fact.threatId;advice.expiresAtTick=fact.deadline;m_releaseAdvice.push_back(advice);break;
        }
    }
    m_nextSchedule=safeAdd(tick,m_profile.suggestionInterval);m_stableUntil=best.empty()?tick:safeAdd(tick,m_profile.suggestionHold);
    m_emergencyKey=emergency;m_scheduled=true;return true;
}

bool PartyAuthority::operator==(const PartyAuthority& b)const {
    return race==b.race&&inputEpoch==b.inputEpoch&&rosterRevision==b.rosterRevision&&driverTurnId==b.driverTurnId&&
        crewIntentRevision==b.crewIntentRevision&&routeRevision==b.routeRevision&&projectionContextId==b.projectionContextId;
}
bool Runtime::currentPartyAuthority(const PartyAuthority& a)const {
    return m_prepared&&a.race==m_race&&a.inputEpoch==m_input&&a.rosterRevision==m_rosterRevision&&
        a.driverTurnId==m_turn&&a.crewIntentRevision==m_intentRevision&&token(a.routeRevision)&&token(a.projectionContextId);
}
void Runtime::refreshPartyView(PartyReason reason) {
    m_partyView.requiredCount=m_profile.crew<=1?1:2;m_partyView.cost=15;
    m_partyView.usedThisTurn=partyEnabled()&&m_partyUsed;
    if(!partyEnabled()){m_partyView.state=PartyState::Disabled;m_partyView.reason=PartyReason::Disabled;}
    else if(m_partyUsed){m_partyView.state=PartyState::Used;m_partyView.reason=PartyReason::UsedThisTurn;}
    else {m_partyView.state=reason==PartyReason::Ready?PartyState::Idle:PartyState::Unavailable;m_partyView.reason=reason;}
    m_partyView.proposalId=0;m_partyView.deadlineTick=0;m_partyView.consentingParticipantIds.clear();
}
void Runtime::cancelParty(PartyReason reason) {
    for(auto& tool:m_partyTools){tool.blocked=true;tool.neutralKnown=false;tool.vote=false;tool.held=false;
        tool.boundProposal=0;tool.awaitingProof=false;tool.releaseFloor=tool.lastReleaseSerial;tool.reason=reason;}
    m_partyVoters.clear();m_partyCommitTick=-1;
    m_partyAdmissionFloor=std::max(m_partyAdmissionFloor,m_partyNow);refreshPartyView(reason);
}
const char* Runtime::partyStateName(PartyState state) {
    switch(state){case PartyState::Disabled:return "disabled";case PartyState::Unavailable:return "unavailable";
        case PartyState::Idle:return "idle";case PartyState::Collecting:return "collecting";
        case PartyState::CommitPending:return "commit_pending";case PartyState::Used:return "used";}return "unavailable";
}
const char* Runtime::partyReasonName(PartyReason reason) {
    switch(reason){case PartyReason::Ready:return "ready";case PartyReason::Disabled:return "disabled";
        case PartyReason::NotPrepared:return "not_prepared";case PartyReason::UsedThisTurn:return "used_this_turn";
        case PartyReason::ReserveProtected:return "reserve_protected";case PartyReason::ConsensusExpired:return "consensus_expired";
        case PartyReason::ReleaseRequired:return "release_required";case PartyReason::DriverVeto:return "driver_veto";
        case PartyReason::ZoneUnavailable:return "zone_unavailable";case PartyReason::ZoneUnqualified:return "zone_unqualified";
        case PartyReason::Cooldown:return "cooldown";case PartyReason::NormalBoostPriority:return "normal_boost_priority";
        case PartyReason::ThreatActive:return "threat_active";case PartyReason::ReinforcementActive:return "reinforcement_active";
        case PartyReason::NativeBoostActive:return "native_boost_active";case PartyReason::KartUnavailable:return "kart_unavailable";
        case PartyReason::TrackingLost:return "tracking_lost";case PartyReason::AuthorityChanged:return "authority_changed";
        case PartyReason::ServiceBusy:return "service_busy";}return "authority_changed";
}
std::vector<PartyFeedback> Runtime::partyFeedback()const {
    std::vector<PartyFeedback> out;if(!configured())return out;
    for(unsigned i=0;i<m_records.size();++i)if(i!=m_driver) {
        const auto& tool=m_partyTools[i];PartyFeedback f;f.participantId=m_records[i].participant.id;f.controlEpoch=tool.epoch;
        f.reason=!partyEnabled()?PartyReason::Disabled:m_partyUsed?PartyReason::UsedThisTurn:tool.reason;
        if(tool.vote&&m_partyView.proposalId&&tool.boundProposal==m_partyView.proposalId) {
            f.progress=m_profile.partyHold?std::min(1.0,double(m_partyNow-tool.started)/m_profile.partyHold):0;
            f.consenting=std::find(m_partyView.consentingParticipantIds.begin(),m_partyView.consentingParticipantIds.end(),f.participantId)!=m_partyView.consentingParticipantIds.end();
        }
        out.push_back(f);
    }return out;
}
bool Runtime::updateParty(Tick tick,const PartyAuthority& authority,const PartyGates& gates,const std::vector<PartyBeam>& beams) {
    auto fail=[&](PartyReason reason){if(tick>=m_partyNow)m_partyNow=tick;cancelParty(reason);return false;};
    if(tick<0||tick<m_partyNow||gates.cooldownUntilTick<0||beams.size()>Profile::MaxCrew)return fail(PartyReason::AuthorityChanged);
    const Tick delta=tick-m_partyNow;m_partyNow=tick;
    m_partyView.zoneQualified=gates.zoneQualified;m_partyView.zoneAvailable=gates.zoneQualified&&gates.zoneAvailable;
    m_partyView.cooldownUntilTick=gates.cooldownUntilTick;
    if(!partyEnabled()){cancelParty(PartyReason::Disabled);return true;}
    if(!configured()||!m_prepared||!gates.live){cancelParty(PartyReason::NotPrepared);return true;}
    if(!currentPartyAuthority(authority))return fail(PartyReason::AuthorityChanged);
    if(m_partyAuthorityBound&&authority!=m_partyAuthority){suspend();refreshPartyView(PartyReason::AuthorityChanged);return false;}
    m_partyAuthority=authority;m_partyAuthorityBound=true;
    if(delta>1)return fail(PartyReason::AuthorityChanged);
    std::array<const PartyBeam*,Profile::MaxParticipants> current={{}};
    std::array<bool,Profile::MaxParticipants> freshRelease={{}};
    for(const auto& b:beams) {
        const int index=participantIndex(b.participantId);
        if(index<0||unsigned(index)==m_driver||current[unsigned(index)]||b.authority!=authority||b.admittedTick<0||
           b.admittedTick>tick||(b.held&&b.neutral))return fail(PartyReason::AuthorityChanged);
        current[unsigned(index)]=&b;auto& tool=m_partyTools[unsigned(index)];
        freshRelease[unsigned(index)]=b.releaseSerial>tool.pressSerial&&b.releaseSerial>tool.releaseFloor;
        tool.lastReleaseSerial=std::max(tool.lastReleaseSerial,b.releaseSerial);
    }
    // Only the selected quorum is necessary. If any selected vote is lost,
    // invalidate all candidates before sorting; a third vote cannot repair it.
    PartyReason interruption=PartyReason::Ready;
    for(unsigned person:m_partyVoters) {
        const auto* b=current[person];const auto& tool=m_partyTools[person];
        if(!b){interruption=PartyReason::TrackingLost;break;}
        if(b->controlEpoch!=tool.epoch||b->serial!=tool.pressSerial||!b->held||!b->partyContext){interruption=PartyReason::ReleaseRequired;break;}
        if(b->interrupted||(!b->visible&&!b->pending)){interruption=PartyReason::TrackingLost;break;}
        if(b->capturedProposalId&&b->capturedProposalId!=m_partyView.proposalId){interruption=PartyReason::AuthorityChanged;break;}
    }
    const bool expired=m_partyView.proposalId&&tick>=m_partyView.deadlineTick;
    if(expired)cancelParty(PartyReason::ConsensusExpired);
    else if(interruption!=PartyReason::Ready)cancelParty(interruption);
    PartyReason unavailable=PartyReason::Ready;
    if(m_partyUsed)unavailable=PartyReason::UsedThisTurn;
    else if(gates.driverVeto)unavailable=PartyReason::DriverVeto;
    else if(gates.normalBoostPriority)unavailable=PartyReason::NormalBoostPriority;
    else if(gates.threatActive||gates.impactThisTick)unavailable=PartyReason::ThreatActive;
    else if(gates.reinforcing)unavailable=PartyReason::ReinforcementActive;
    else if(gates.nativeBoostActive)unavailable=PartyReason::NativeBoostActive;
    else if(!gates.humanEligible)unavailable=PartyReason::KartUnavailable;
    else if(!gates.zoneQualified)unavailable=PartyReason::ZoneUnqualified;
    else if(!gates.zoneAvailable)unavailable=PartyReason::ZoneUnavailable;
    else if(gates.serviceBusy)unavailable=PartyReason::ServiceBusy;
    else if(tick<gates.cooldownUntilTick)unavailable=PartyReason::Cooldown;
    else if(!gates.reserveAvailable)unavailable=PartyReason::ReserveProtected;
    const bool admissionBlocked=expired||unavailable!=PartyReason::Ready;
    if(unavailable!=PartyReason::Ready)cancelParty(unavailable);
    std::vector<unsigned> admissions;bool heldBlocked=false;
    for(unsigned person=0;person<m_records.size();++person)if(person!=m_driver) {
        auto& tool=m_partyTools[person];const auto* b=current[person];
        if(!b){tool.blocked=true;tool.neutralKnown=false;tool.held=false;tool.vote=tool.awaitingProof=false;
            tool.releaseFloor=tool.lastReleaseSerial;tool.reason=PartyReason::TrackingLost;continue;}
        if(b->controlEpoch<tool.epoch){tool.blocked=true;tool.neutralKnown=false;tool.vote=tool.awaitingProof=false;
            tool.reason=PartyReason::AuthorityChanged;continue;}
        if(!b->held) {
            tool.held=false;tool.vote=tool.awaitingProof=false;
            if(b->neutral&&!b->interrupted&&b->releaseSerial>tool.pressSerial&&b->releaseSerial>=tool.releaseFloor) {
                tool.epoch=b->controlEpoch;tool.neutralKnown=true;tool.blocked=false;tool.reason=PartyReason::Ready;
            } else {tool.blocked=true;tool.neutralKnown=false;tool.reason=PartyReason::ReleaseRequired;}
            continue;
        }
        const bool newPress=b->serial>tool.pressSerial;
        if(newPress) {
            const bool neutral=tool.neutralKnown||freshRelease[person];
            tool.epoch=b->controlEpoch;tool.pressSerial=b->serial;tool.held=true;
            tool.started=tool.admitted=b->admittedTick;tool.neutralKnown=false;
            tool.blocked=!neutral||b->admittedTick<m_partyAdmissionFloor;
            tool.awaitingProof=false;tool.reason=tool.blocked?PartyReason::ReleaseRequired:PartyReason::Ready;
        } else tool.held=true;
        const bool failed=admissionBlocked||!b->serial||!b->controlEpoch||b->interrupted||!b->partyContext||
            (!b->visible&&!b->pending)||tool.blocked||b->serial!=tool.pressSerial||b->controlEpoch!=tool.epoch;
        if(failed) {
            tool.blocked=true;tool.neutralKnown=false;tool.vote=tool.awaitingProof=false;tool.releaseFloor=tool.lastReleaseSerial;
            tool.reason=admissionBlocked?(expired?PartyReason::ConsensusExpired:unavailable):
                !b->visible||b->interrupted?PartyReason::TrackingLost:PartyReason::ReleaseRequired;
            if(b->partyContext)heldBlocked=true;continue;
        }
        if(b->pending) {
            // Pending initial proof is not a negative, nor a vote. Keep its
            // immutable press/UP/authority until that same query resolves.
            if(newPress)tool.awaitingProof=true;
            continue;
        }
        if(tool.vote) {
            if(tool.boundProposal!=m_partyView.proposalId||(b->capturedProposalId&&b->capturedProposalId!=tool.boundProposal)) {
                cancelParty(PartyReason::AuthorityChanged);return true;
            }
        } else if(newPress||tool.awaitingProof) {tool.awaitingProof=false;admissions.push_back(person);}
        else {tool.blocked=true;tool.reason=PartyReason::ReleaseRequired;if(b->partyContext)heldBlocked=true;}
    }
    if(admissionBlocked)return true;
    std::sort(admissions.begin(),admissions.end(),[&](unsigned a,unsigned b){return m_partyTools[a].admitted!=m_partyTools[b].admitted?
        m_partyTools[a].admitted<m_partyTools[b].admitted:m_records[a].participant.id<m_records[b].participant.id;});
    for(unsigned person:admissions) {
        auto& tool=m_partyTools[person];const auto& b=*current[person];
        // Zero can bootstrap only this new, authenticated released-then-pressed
        // control, admitted after the last authority/cancel barrier. It is
        // immediately bound to one proposal and never acts as a later wildcard.
        if((b.capturedProposalId&&b.capturedProposalId!=m_partyView.proposalId)||tool.admitted<m_partyAdmissionFloor||
           (b.capturedProposalId&&tool.admitted<m_partyStarted)) {
            tool.blocked=true;tool.reason=PartyReason::AuthorityChanged;heldBlocked=true;continue;
        }
        const Tick start=m_partyView.proposalId?std::min(m_partyStarted,tool.admitted):tool.admitted;
        if(start>std::numeric_limits<Tick>::max()-m_profile.partyWindow||(m_partyNextProposal==UINT64_MAX&&!m_partyView.proposalId)) {
            cancelParty(PartyReason::AuthorityChanged);return false;
        }
        // A late completion can only shorten the original admission window.
        // Expiry is processed before quorum even when learned asynchronously.
        const Tick deadline=start+m_profile.partyWindow;
        if(tick>=deadline){cancelParty(PartyReason::ConsensusExpired);return true;}
        if(!m_partyView.proposalId)m_partyView.proposalId=++m_partyNextProposal;
        m_partyStarted=start;m_partyView.deadlineTick=deadline;
        tool.vote=true;tool.boundProposal=m_partyView.proposalId;tool.reason=PartyReason::Ready;
    }
    if(!m_partyView.proposalId){refreshPartyView(heldBlocked?PartyReason::ReleaseRequired:PartyReason::Ready);return true;}
    // All five candidates are bounded; asynchronous completion order is not
    // quorum priority. Selected identities are sorted by original admission.
    std::vector<unsigned> candidates;
    for(unsigned person=0;person<m_records.size();++person)if(m_partyTools[person].vote&&
        m_partyTools[person].boundProposal==m_partyView.proposalId&&!m_partyTools[person].blocked&&current[person]&&
        current[person]->held&&(current[person]->visible||current[person]->pending))candidates.push_back(person);
    std::sort(candidates.begin(),candidates.end(),[&](unsigned a,unsigned b){return m_partyTools[a].admitted!=m_partyTools[b].admitted?
        m_partyTools[a].admitted<m_partyTools[b].admitted:m_records[a].participant.id<m_records[b].participant.id;});
    m_partyVoters.assign(candidates.begin(),candidates.begin()+std::min(candidates.size(),std::size_t(m_partyView.requiredCount)));
    m_partyView.state=PartyState::Collecting;m_partyView.reason=PartyReason::Ready;m_partyView.consentingParticipantIds.clear();
    for(unsigned person:candidates)m_partyTools[person].reason=std::find(m_partyVoters.begin(),m_partyVoters.end(),person)!=m_partyVoters.end()?PartyReason::Ready:PartyReason::ServiceBusy;
    for(unsigned person:m_partyVoters)if(current[person]->visible&&!current[person]->pending&&tick-m_partyTools[person].started>=m_profile.partyHold)
        m_partyView.consentingParticipantIds.push_back(m_records[person].participant.id);
    if(m_partyView.consentingParticipantIds.size()==m_partyView.requiredCount) {
        m_partyView.state=PartyState::CommitPending;m_partyCommitTick=tick;
    }
    return true;
}
bool Runtime::confirmParty(bool applied) {
    if(!m_prepared||!partyEnabled()||m_partyUsed||!partyWanted()||m_partyCommitTick!=m_partyNow||
       !currentPartyAuthority(m_partyAuthority)||m_partyNow>=m_partyView.deadlineTick)return false;
    if(!applied){cancelParty(PartyReason::KartUnavailable);return true;}
    m_partyUsed=true;m_countersSaturated=saturatingAdd(&m_partyActivations,1)||m_countersSaturated;
    cancelParty(PartyReason::UsedThisTurn);return true;
}

}
