// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_HPP
#define HEADER_SCINTILLA_CREW_HPP
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Pure coordination only. The native rotation remains the sole authority for
// completed handoffs; preparation, acknowledgements and input epochs never
// create a turn. No GL, Bullet, transport, media or global clock is accessed.
namespace ScintillaCrew {
using Tick=int64_t;
enum class Competence {Universal=0,Hacker=1,Gunner=2,Defender=3,Engineer=4,Recon=5};
// Pure shared policy for live assignments and the final native turn snapshot.
// Reducing the rotation cycle before multiplication also handles UInt64 max.
inline Competence competenceAtTurn(unsigned index,unsigned count,unsigned driver,uint64_t turn) {
    if(count<2||count>6||index>=count||driver>=count||index==driver||count<=3)return Competence::Universal;
    const unsigned cycle=unsigned((turn/count)%5),j=(index+count-driver)%count-1;
    return Competence(1+(j+(count-1)*cycle)%5);
}
struct Profile {
    enum {MaxParticipants=6,MaxCrew=5,MaxBundles=7,MaxSuggestions=5};
    int fps=0;unsigned crew=0;
    Tick suggestionInterval=0,suggestionHold=0,urgentTether=0,maxWait=0,partyHold=0,partyWindow=0;
    static bool create(int fps,unsigned crew,Profile* out);
};
struct Participant {std::string id,name;};
enum class ContributionKind {Opening=0,ComboContribution=1,Stagger=2,Defense=3,ShellOpened=4,
    ModuleCollected=5,ModuleRecovered=6,NormalBoost=7,PartyConsent=8};
enum class OpportunityMetric {NoOpportunity=0,Waiting=1,Engaged=2,NotReady=3,Paused=4};
enum class TaskKind {Offense=0,Defense=1,CapsuleShell=2,CapsuleCollect=3,TetherRecover=4,NormalBoost=5,PartyBoost=6};
enum class OffenseAction {None=0,OpenShield=1,MaintainShield=2,WaitForBreach=3,Fire=4,ContributionComplete=5};
struct ParticipantRecord {
    Participant participant;
    std::array<uint64_t,9> contributions={{}},lastCommitSerial={{}};
    std::array<uint64_t,5> opportunityTicks={{}};
    Tick waiting=0;
};
struct Task {
    TaskKind kind=TaskKind::Offense;
    uint64_t targetId=0,instance=0,phaseRevision=0,threatId=0,moduleId=0;
};
struct Bundle:Task {
    std::vector<std::string> eligibleParticipantIds;
    std::string offenseOwner;
    std::vector<std::string> offenseContributors;
    bool offenseBreached=false;
    Tick deadline=0;bool imminent=false;
};
struct Occupation:Task {
    std::string participantId;
    // A real held control without a current target still occupies its hand.
    // IDs must be zero; this never becomes a synthetic opportunity or task.
    bool unassigned=false;
};
struct Suggestion:Task {
    std::string participantId;Tick expiresAtTick=0;
    OffenseAction offenseAction=OffenseAction::None;
};
struct ReleaseAdvice {std::string participantId;uint64_t threatId=0;Tick expiresAtTick=0;};
// Shared by scheduling and presentation: an advisory action cannot contradict
// the current SC2 opener, phase or already committed contribution.
bool offenseActionCurrent(const Suggestion&,const Bundle&,unsigned crew);
// Returns true when saturation occurred. It never wraps and is also used by
// diagnostic accounting, whose largest increments need not fit signed32.
bool saturatingAdd(uint64_t* value,uint64_t amount);
struct Assignment {
    std::string participantId,name;unsigned rosterIndex=0;
    bool isPilot=false;Competence competence=Competence::Universal;
};
enum class PartyState {Disabled=0,Unavailable=1,Idle=2,Collecting=3,CommitPending=4,Used=5};
enum class PartyReason {Ready=0,Disabled=1,NotPrepared=2,UsedThisTurn=3,ReserveProtected=4,
    ConsensusExpired=5,ReleaseRequired=6,DriverVeto=7,ZoneUnavailable=8,ZoneUnqualified=9,Cooldown=10,
    NormalBoostPriority=11,ThreatActive=12,ReinforcementActive=13,NativeBoostActive=14,KartUnavailable=15,
    TrackingLost=16,AuthorityChanged=17,ServiceBusy=18};
struct PartyAuthority {
    uint64_t race=0,inputEpoch=0,rosterRevision=0,driverTurnId=0,crewIntentRevision=0;
    std::string routeRevision,projectionContextId;
    bool operator==(const PartyAuthority& b)const;
    bool operator!=(const PartyAuthority& b)const{return !(*this==b);}
};
struct PartyBeam {
    std::string participantId;PartyAuthority authority;
    uint64_t controlEpoch=0,serial=0,releaseSerial=0,capturedProposalId=0;
    Tick admittedTick=0;
    bool held=false,visible=false,interrupted=false,neutral=false,partyContext=false,pending=false;
};
struct PartyGates {
    bool live=false,zoneQualified=false,zoneAvailable=false,driverVeto=false,threatActive=false,
        impactThisTick=false,reinforcing=false,nativeBoostActive=false,humanEligible=false,
        normalBoostPriority=false,serviceBusy=false,reserveAvailable=false;
    Tick cooldownUntilTick=0;
};
struct PartyView {
    PartyState state=PartyState::Disabled;PartyReason reason=PartyReason::Disabled;
    uint64_t proposalId=0;Tick deadlineTick=0;
    std::vector<std::string> consentingParticipantIds;
    unsigned requiredCount=1,cost=15;
    bool usedThisTurn=false,zoneQualified=false,zoneAvailable=false;
    Tick cooldownUntilTick=0;
};
struct PartyFeedback {
    std::string participantId;uint64_t controlEpoch=0;
    PartyReason reason=PartyReason::Ready;double progress=0;bool consenting=false;
};
class Runtime {
public:
    bool setIntent(bool required,const std::string& profileId);
    bool required()const{return m_required||configured();}
    uint64_t intentRevision()const{return m_intentRevision;}
    const std::string& profileId()const{return m_profileId;}
    bool partyEnabled()const{return m_profileId=="sc5_crew_party_v1";}
    // Seed a held race for assignment presentation before SC1--4 are prepared.
    // First binding is turn zero; subsequent calls can only repeat this exact
    // native authority. A new native turn must go through observeTurn.
    bool bindRace(uint64_t race,const std::vector<Participant>& frozenRoster,
                  unsigned driverIndex,uint64_t completedHandoffs,int fps);
    bool prepare(uint64_t race,uint64_t input,uint64_t rosterRevision,
                 const std::vector<Participant>& frozenRoster,unsigned driverIndex,
                 uint64_t completedHandoffs,int fps);
    bool observeTurn(uint64_t race,unsigned driverIndex,uint64_t completedHandoffs,
                     bool administrative=false);
    void suspend();
    // Each call describes current, already admitted facts. Suggestions never
    // create input or target eligibility. Same-tick calls can invalidate hints;
    // simulated accounting advances only on the first call for a new tick.
    bool update(Tick tick,bool active,const std::vector<Bundle>& opportunities,
                const std::vector<std::string>& readyIDs,const std::vector<Occupation>& occupancy);
    bool credit(const std::string& participant,ContributionKind,uint64_t authoritativeCommitSerial);
    // Pauses freeze simulation ticks. The adapter may separately supply elapsed
    // monotonic wall time converted once to equivalent ticks, excluding overlap.
    bool accountPaused(Tick elapsedEquivalentTicks);
    const std::vector<Suggestion>& suggestions()const{return m_suggestions;}
    const std::vector<ReleaseAdvice>& releaseAdvice()const{return m_releaseAdvice;}
    bool active()const{return m_active;}
    Tick now()const{return m_now;}
    bool countersSaturated()const{return m_countersSaturated;}
    unsigned lastEvaluationCount()const{return m_lastEvaluationCount;}
    static unsigned bundleSlots(TaskKind,unsigned crew);
    static const char* taskName(TaskKind);
    static const char* offenseActionName(OffenseAction);
    static const char* contributionName(ContributionKind);
    bool updateParty(Tick tick,const PartyAuthority&,const PartyGates&,const std::vector<PartyBeam>&);
    bool partyWanted()const{return m_partyView.state==PartyState::CommitPending;}
    bool confirmParty(bool applied);
    void cancelParty(PartyReason);
    const PartyView& partyView()const{return m_partyView;}
    std::vector<PartyFeedback> partyFeedback()const;
    uint64_t partyActivations()const{return m_partyActivations;}
    static const char* partyStateName(PartyState);
    static const char* partyReasonName(PartyReason);
    void reset(bool preserveIntent=true);
    bool configured()const{return m_race!=0;}
    bool prepared()const{return m_prepared;}
    uint64_t race()const{return m_race;}
    uint64_t inputEpoch()const{return m_input;}
    uint64_t rosterRevision()const{return m_rosterRevision;}
    uint64_t driverTurnId()const{return m_turn;}
    unsigned driverIndex()const{return m_driver;}
    uint64_t rotationAnomalies()const{return m_rotationAnomalies;}
    const Profile& profile()const{return m_profile;}
    const std::vector<ParticipantRecord>& records()const{return m_records;}
    std::vector<Assignment> assignments()const;
    std::vector<std::string> supporterIDs()const;
    static const char* competenceName(Competence);
private:
    bool m_required=false,m_prepared=false,m_turnNeedsInput=false,m_active=false,m_countersSaturated=false,m_scheduled=false;
    Tick m_now=0,m_nextSchedule=0,m_stableUntil=0;
    unsigned m_lastEvaluationCount=0;
    std::string m_emergencyKey;
    std::vector<Suggestion> m_suggestions;
    std::vector<ReleaseAdvice> m_releaseAdvice;
    uint64_t m_intentRevision=0,m_race=0,m_input=0,m_rosterRevision=0,m_turn=0,m_rotationAnomalies=0;
    unsigned m_driver=0;
    std::string m_profileId="sc5_crew_standard_v1";
    Profile m_profile;
    std::vector<ParticipantRecord> m_records;
    struct PartyTool {
        uint64_t epoch=0,pressSerial=0,lastReleaseSerial=0,releaseFloor=0,boundProposal=0;
        Tick started=0,admitted=0;bool blocked=true,neutralKnown=false,held=false,vote=false,awaitingProof=false;
        PartyReason reason=PartyReason::ReleaseRequired;
    };
    std::array<PartyTool,Profile::MaxParticipants> m_partyTools;
    PartyAuthority m_partyAuthority;
    PartyView m_partyView;
    bool m_partyAuthorityBound=false,m_partyUsed=false;
    uint64_t m_partyNextProposal=0,m_partyActivations=0;
    Tick m_partyNow=0,m_partyCommitTick=-1,m_partyAdmissionFloor=0,m_partyStarted=0;
    std::vector<unsigned> m_partyVoters;
    void refreshPartyView(PartyReason);
    bool currentPartyAuthority(const PartyAuthority&)const;
    bool sameRoster(const std::vector<Participant>&)const;
    int participantIndex(const std::string&)const;
    void clearAdvice();
};
}
#endif
