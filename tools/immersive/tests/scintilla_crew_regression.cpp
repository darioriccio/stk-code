// SPDX-License-Identifier: GPL-3.0-or-later
// Pure SC5 production core regressions. No native world, GPU or hardware.
#include "modes/immersive_relay/scintilla_crew.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
using namespace ScintillaCrew;
#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
std::vector<Participant> people(unsigned count) {
    std::vector<Participant> result;for(unsigned i=0;i<count;++i){Participant p;
        p.id="person-"+std::to_string(i);p.name="Person "+std::to_string(i);result.push_back(p);}return result;
}
Runtime prepared(unsigned count,bool party=false) {
    Runtime runtime;CHECK(runtime.setIntent(true,party?"sc5_crew_party_v1":"sc5_crew_standard_v1"));
    CHECK(runtime.prepare(1,1,1,people(count),0,0,120));return runtime;
}
void rolesMatchEveryNativeRaceAndFairnessCalculation() {
    const unsigned laps[5]={6,6,8,10,12};
    for(unsigned count=2;count<=6;++count) {
        auto runtime=prepared(count);const auto roster=people(count);
        std::array<unsigned,6> driving={{0,0,0,0,0,0}},support={{0,0,0,0,0,0}};
        unsigned exposure[6][5]={};
        for(unsigned turn=0;turn<laps[count-2];++turn) {
            if(turn){CHECK(runtime.observeTurn(1,turn%count,turn));
                CHECK(!runtime.prepared());CHECK(runtime.prepare(1,turn+1,turn+1,roster,turn%count,turn,120));}
            const auto assignments=runtime.assignments();CHECK(assignments.size()==count);
            std::set<Competence> competencies;unsigned pilots=0;
            for(const auto& a:assignments) {
                const unsigned i=a.rosterIndex;CHECK(a.participantId==roster[i].id);
                if(a.isPilot){++driving[i];++pilots;CHECK(i==turn%count);}
                else {++support[i];if(count<=3)CHECK(a.competence==Competence::Universal);
                    else {const unsigned j=(i+count-turn%count)%count-1;
                        const unsigned expected=(j+(count-1)*(turn/count))%5;
                        CHECK(unsigned(a.competence)==expected+1);++exposure[i][expected];
                        CHECK(competencies.insert(a.competence).second);}}
            }
            CHECK(pilots==1&&runtime.supporterIDs().size()==count-1);
            for(unsigned j=0;j<count-1;++j)CHECK(runtime.supporterIDs()[j]==roster[(turn%count+1+j)%count].id);
        }
        for(unsigned i=0;i<count;++i) {
            CHECK(driving[i]==laps[count-2]/count);CHECK(support[i]==laps[count-2]-driving[i]);
            if(count>=4){const auto limits=std::minmax_element(exposure[i],exposure[i]+5);
                CHECK(*limits.first>=1&&*limits.second-*limits.first<=1);
                unsigned total=0;for(unsigned n:exposure[i])total+=n;CHECK(total==support[i]);}
        }
    }
}
void heldPreviewHasRolesButCannotClaimPrepared() {
    Runtime runtime;CHECK(runtime.assignments().empty());CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));
    CHECK(runtime.bindRace(1,people(5),2,0,120));CHECK(runtime.configured()&&!runtime.prepared());
    const auto roles=runtime.assignments();CHECK(roles.size()==5&&roles[2].isPilot);
    CHECK(runtime.supporterIDs()==std::vector<std::string>({"person-3","person-4","person-0","person-1"}));
    CHECK(runtime.bindRace(1,people(5),2,0,120));CHECK(!runtime.prepared());
    CHECK(runtime.prepare(1,1,1,people(5),2,0,120));CHECK(runtime.prepared());
}
void previewProfileChoiceAndMissedFullRotationRemainHeld() {
    Runtime runtime;CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));
    CHECK(runtime.bindRace(1,people(4),0,0,120));const auto revision=runtime.intentRevision();
    CHECK(runtime.setIntent(true,"sc5_crew_party_v1"));CHECK(runtime.partyEnabled()&&!runtime.prepared());
    CHECK(runtime.intentRevision()==revision+1);
    CHECK(runtime.prepare(1,1,1,people(4),0,0,120));CHECK(!runtime.setIntent(true,"sc5_crew_standard_v1"));
    CHECK(!runtime.observeTurn(1,0,1));CHECK(!runtime.observeTurn(1,0,3));CHECK(runtime.prepared());
    CHECK(runtime.observeTurn(1,0,4));CHECK(runtime.driverTurnId()==4&&runtime.rotationAnomalies()==1&&!runtime.prepared());
    CHECK(runtime.observeTurn(1,0,4,true));CHECK(runtime.rotationAnomalies()==1);
    CHECK(!runtime.prepare(1,1,1,people(4),0,4,120));CHECK(runtime.prepare(1,2,2,people(4),0,4,120));
    CHECK(!runtime.observeTurn(1,3,3));CHECK(runtime.driverTurnId()==4&&runtime.prepared());
    Runtime maximumName;CHECK(maximumName.setIntent(true,"sc5_crew_standard_v1"));
    auto roster=people(2);roster[0].name=std::string(128,'x');CHECK(maximumName.bindRace(1,roster,0,0,120));
    Runtime tooLong;CHECK(tooLong.setIntent(true,"sc5_crew_standard_v1"));
    roster[0].name+='x';CHECK(!tooLong.bindRace(1,roster,0,0,120)&&!tooLong.configured());
}
void disablingNeverPreparedPreviewClearsRolesAndKeepsIntentRevision() {
    for(bool party:{false,true}) {
        Runtime runtime;CHECK(runtime.setIntent(true,party?"sc5_crew_party_v1":"sc5_crew_standard_v1"));
        CHECK(runtime.bindRace(1,people(5),2,0,120));const auto revision=runtime.intentRevision();
        CHECK(runtime.required()&&runtime.configured()&&runtime.assignments().size()==5);
        // A failed change is observationally inert, including preview roles.
        CHECK(!runtime.setIntent(false,"unknown"));CHECK(runtime.intentRevision()==revision);
        CHECK(runtime.required()&&runtime.configured()&&runtime.assignments()[2].isPilot);
        CHECK(runtime.setIntent(false,"sc5_crew_standard_v1"));
        CHECK(runtime.intentRevision()==revision+1&&!runtime.required()&&!runtime.configured());
        CHECK(!runtime.prepared()&&!runtime.active()&&runtime.race()==0&&runtime.driverTurnId()==0);
        CHECK(runtime.inputEpoch()==0&&runtime.rosterRevision()==0&&runtime.driverIndex()==0);
        CHECK(runtime.records().empty()&&runtime.assignments().empty()&&runtime.supporterIDs().empty());
        CHECK(runtime.suggestions().empty()&&runtime.releaseAdvice().empty()&&runtime.partyFeedback().empty());
        CHECK(!runtime.partyEnabled()&&runtime.partyView().state==PartyState::Disabled&&!runtime.partyWanted());
        CHECK(!runtime.prepare(1,1,1,people(5),2,0,120));
        CHECK(runtime.setIntent(false,"sc5_crew_standard_v1"));CHECK(runtime.intentRevision()==revision+1);
        CHECK(runtime.setIntent(true,"sc5_crew_party_v1"));CHECK(runtime.intentRevision()==revision+2);
        // Re-enabling freezes the new explicit roster, with no old preview data.
        CHECK(runtime.bindRace(2,people(3),1,0,120));CHECK(runtime.records().size()==3);
        CHECK(runtime.assignments()[1].isPilot&&runtime.race()==2&&runtime.partyEnabled());
        CHECK(runtime.prepare(2,1,1,people(3),1,0,120));
        runtime.suspend();CHECK(!runtime.setIntent(false,"sc5_crew_standard_v1"));
        CHECK(runtime.required()&&runtime.configured()&&runtime.inputEpoch()==1);
        CHECK(runtime.profileId()=="sc5_crew_party_v1"&&runtime.intentRevision()==revision+2);
    }
    Runtime explicitParty;CHECK(explicitParty.setIntent(true,"sc5_crew_party_v1"));
    CHECK(explicitParty.bindRace(1,people(4),0,0,120));const auto revision=explicitParty.intentRevision();
    CHECK(explicitParty.setIntent(false,"sc5_crew_party_v1"));
    CHECK(!explicitParty.required()&&!explicitParty.configured()&&explicitParty.partyEnabled());
    CHECK(explicitParty.intentRevision()==revision+1&&explicitParty.records().empty());
    CHECK(explicitParty.partyView().state==PartyState::Unavailable&&explicitParty.partyView().requiredCount==1);
}
void observedTurnLocksEvenNeverPreparedPreviewAgainstDowngrade() {
    Runtime runtime;CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));
    CHECK(runtime.bindRace(1,people(4),0,0,120));const auto revision=runtime.intentRevision();
    CHECK(runtime.observeTurn(1,1,1));CHECK(runtime.inputEpoch()==0&&!runtime.prepared());
    CHECK(!runtime.setIntent(false,"sc5_crew_standard_v1"));
    CHECK(!runtime.setIntent(true,"sc5_crew_party_v1"));
    CHECK(runtime.required()&&runtime.configured()&&runtime.driverTurnId()==1&&runtime.driverIndex()==1);
    CHECK(runtime.intentRevision()==revision&&runtime.records().size()==4);
    CHECK(runtime.assignments()[1].isPilot&&!runtime.partyEnabled());
}
void nativeTurnCommitIsTheOnlyAdvancement() {
    auto runtime=prepared(4,true);const auto revision=runtime.intentRevision();
    CHECK(runtime.driverTurnId()==0);CHECK(runtime.observeTurn(1,0,0,true));
    CHECK(runtime.rotationAnomalies()==0&&runtime.prepared());
    CHECK(!runtime.observeTurn(1,1,0));CHECK(runtime.driverTurnId()==0&&runtime.prepared());
    CHECK(!runtime.observeTurn(2,1,1));CHECK(!runtime.observeTurn(1,4,1));CHECK(!runtime.observeTurn(1,0,1));
    CHECK(runtime.observeTurn(1,1,1));CHECK(!runtime.prepared()&&runtime.driverTurnId()==1);
    CHECK(runtime.rotationAnomalies()==0&&runtime.intentRevision()==revision);
    CHECK(runtime.observeTurn(1,1,1,true));CHECK(runtime.rotationAnomalies()==0);
    CHECK(!runtime.observeTurn(1,0,0));CHECK(!runtime.observeTurn(1,2,1));
    CHECK(!runtime.prepare(1,1,1,people(4),1,1,120));
    CHECK(runtime.prepare(1,2,2,people(4),1,1,120));
    CHECK(runtime.observeTurn(1,3,2,true));CHECK(runtime.rotationAnomalies()==1&&runtime.driverTurnId()==2);
    CHECK(runtime.observeTurn(1,3,2,true));CHECK(runtime.rotationAnomalies()==1);
    CHECK(runtime.prepare(1,3,3,people(4),3,2,120));
    CHECK(runtime.observeTurn(1,1,4));CHECK(runtime.driverTurnId()==4&&runtime.rotationAnomalies()==2);
    CHECK(!runtime.observeTurn(1,2,3));CHECK(runtime.driverTurnId()==4);
}
void sameRaceReprepareSuspendAndResetKeepCorrectAuthority() {
    auto runtime=prepared(5,true);const auto revision=runtime.intentRevision();
    CHECK(runtime.observeTurn(1,1,1));CHECK(runtime.prepare(1,2,2,people(5),1,1,120));
    const auto roles=runtime.assignments();runtime.suspend();
    CHECK(!runtime.prepared()&&runtime.race()==1&&runtime.driverTurnId()==1&&runtime.partyEnabled());
    CHECK(runtime.prepare(1,2,2,people(5),1,1,120));CHECK(runtime.prepared());
    CHECK(runtime.prepare(1,3,3,people(5),1,1,120));CHECK(runtime.driverTurnId()==1);
    CHECK(runtime.assignments().size()==roles.size()&&runtime.intentRevision()==revision);
    CHECK(!runtime.prepare(1,2,3,people(5),1,1,120));CHECK(runtime.inputEpoch()==3);
    CHECK(!runtime.prepare(1,3,4,people(5),1,1,120));CHECK(runtime.rosterRevision()==3);
    CHECK(!runtime.prepare(1,4,2,people(5),1,1,120));CHECK(runtime.rosterRevision()==3);
    CHECK(!runtime.prepare(2,4,4,people(5),0,0,120));CHECK(runtime.race()==1);
    runtime.reset();CHECK(!runtime.configured()&&runtime.required()&&runtime.partyEnabled());
    CHECK(runtime.intentRevision()==revision&&runtime.records().empty()&&runtime.driverTurnId()==0);
    CHECK(!runtime.prepare(2,1,1,people(5),1,1,120));CHECK(!runtime.configured());
    CHECK(runtime.prepare(2,1,1,people(5),0,0,120));CHECK(runtime.rotationAnomalies()==0);
    runtime.reset(false);CHECK(!runtime.configured()&&!runtime.required()&&!runtime.partyEnabled());
    CHECK(runtime.intentRevision()==revision);
}
void frozenRosterAndProfileCannotBeRewrittenByRetry() {
    auto runtime=prepared(4);const auto revision=runtime.intentRevision();
    CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));CHECK(runtime.intentRevision()==revision);
    runtime.suspend();CHECK(!runtime.setIntent(false,"sc5_crew_standard_v1"));
    CHECK(!runtime.setIntent(true,"sc5_crew_party_v1"));CHECK(!runtime.partyEnabled());
    auto swapped=people(4);std::swap(swapped[1],swapped[2]);CHECK(!runtime.prepare(1,2,2,swapped,0,0,120));
    auto renamed=people(4);renamed[0].name="Other";CHECK(!runtime.prepare(1,2,2,renamed,0,0,120));
    CHECK(runtime.records()[0].participant.name=="Person 0"&&runtime.records()[1].participant.id=="person-1");
    CHECK(!runtime.prepare(1,2,2,people(3),0,0,120));CHECK(!runtime.prepare(1,2,2,people(4),0,0,144));
    CHECK(runtime.inputEpoch()==1&&runtime.rosterRevision()==1);
    runtime.reset();CHECK(runtime.setIntent(true,"sc5_crew_party_v1"));CHECK(runtime.intentRevision()==revision+1);
}
void invalidInitialPreparationDoesNotSeedRace() {
    for(unsigned mutation=0;mutation<12;++mutation) {
        Runtime runtime;CHECK(runtime.setIntent(true,"sc5_crew_standard_v1"));
        auto roster=people(4);uint64_t race=1,input=1,revision=1,turn=0;int fps=120;unsigned driver=0;
        if(mutation==0)race=0;if(mutation==1)input=0;if(mutation==2)revision=0;
        if(mutation==3)turn=1;if(mutation==4)fps=59;if(mutation==5)fps=1001;
        if(mutation==6)roster.resize(1);if(mutation==7)roster=people(7);
        if(mutation==8)roster[1].id=roster[0].id;if(mutation==9)roster[1].id="bad/id";
        if(mutation==10)roster[1].name="";if(mutation==11)driver=4;
        CHECK(!runtime.prepare(race,input,revision,roster,driver,turn,fps));
        CHECK(!runtime.configured()&&!runtime.prepared()&&runtime.records().empty()&&runtime.driverTurnId()==0);
    }
    Runtime runtime;CHECK(!runtime.prepare(1,1,1,people(2),0,0,120));
    CHECK(!runtime.setIntent(true,"unknown"));CHECK(runtime.intentRevision()==0);
    Profile profile;CHECK(!Profile::create(120,0,&profile));CHECK(!Profile::create(120,6,&profile));
    CHECK(!Profile::create(120,1,nullptr));CHECK(Profile::create(60,1,&profile));CHECK(Profile::create(1000,5,&profile));
}
void competenceModuloRemainsExactAtUInt64Maximum() {
    auto runtime=prepared(6);CHECK(runtime.observeTurn(1,1,UINT64_MAX,true));
    CHECK(runtime.driverTurnId()==UINT64_MAX&&runtime.rotationAnomalies()==1);
    std::set<Competence> seen;const unsigned cycle=unsigned((UINT64_MAX/6)%5);
    for(const auto& a:runtime.assignments())if(!a.isPilot) {
        const unsigned j=(a.rosterIndex+6-1)%6-1;
        CHECK(unsigned(a.competence)==1+(j+5*cycle)%5);CHECK(seen.insert(a.competence).second);
    }
    CHECK(!runtime.observeTurn(1,2,0));CHECK(runtime.driverTurnId()==UINT64_MAX);
    for(unsigned i=0;i<6;++i)CHECK(std::string(Runtime::competenceName(Competence(i))).size()>0);
}

Bundle opportunity(TaskKind kind,const std::vector<std::string>& eligible,Tick deadline=10000,uint64_t entity=10) {
    Bundle b;b.kind=kind;b.eligibleParticipantIds=eligible;b.deadline=deadline;
    if(kind==TaskKind::Defense)b.threatId=entity;
    else if(kind!=TaskKind::NormalBoost&&kind!=TaskKind::PartyBoost){b.targetId=entity;b.instance=1;b.phaseRevision=1;
        if(kind!=TaskKind::Offense)b.moduleId=entity+100;}
    return b;
}
std::string hintsKey(const Runtime& runtime) {
    std::string key;for(const auto& hint:runtime.suggestions())key+=hint.participantId+":"+Runtime::taskName(hint.kind)+":"+
        std::to_string(hint.targetId)+":"+std::to_string(hint.threatId)+":"+Runtime::offenseActionName(hint.offenseAction)+"|";return key;
}
unsigned hintCount(const Runtime& runtime,TaskKind kind) {
    unsigned n=0;for(const auto& hint:runtime.suggestions())if(hint.kind==kind)++n;return n;
}
void offenseActionsFollowTheActualOwnerAcrossShieldPhasesAndUniversalCrews() {
    for(unsigned count=2;count<=6;++count) {
        auto runtime=prepared(count);const auto ready=runtime.supporterIDs();auto offense=opportunity(TaskKind::Offense,ready);
        CHECK(runtime.update(0,true,{offense},ready,{}));
        unsigned opening=0,firing=0,waiting=0;
        std::string proposedOpener;
        for(const auto& hint:runtime.suggestions()) {
            CHECK(offenseActionCurrent(hint,offense,count-1));
            if(hint.offenseAction==OffenseAction::OpenShield){++opening;proposedOpener=hint.participantId;}
            if(hint.offenseAction==OffenseAction::Fire)++firing;
            if(hint.offenseAction==OffenseAction::WaitForBreach)++waiting;
        }
        if(count==2){CHECK(opening==0&&firing==1&&waiting==0);continue;}
        CHECK(opening==1&&firing==0&&waiting==Runtime::bundleSlots(TaskKind::Offense,count-1)-1);
        // A volunteer may open instead of the suggested person, regardless of
        // their recommended competence. Reassign the actual SC2 owner at once.
        offense.offenseOwner=*std::find_if(ready.begin(),ready.end(),[&](const std::string& id){return id!=proposedOpener;});
        CHECK(runtime.update(0,true,{offense},ready,{}));
        for(const auto& hint:runtime.suggestions())CHECK(hint.offenseAction==
            (hint.participantId==offense.offenseOwner?OffenseAction::MaintainShield:OffenseAction::WaitForBreach));
        offense.offenseBreached=true;
        CHECK(runtime.update(0,true,{offense},ready,{})); // Phase invalidation bypasses 0.6 s stability.
        unsigned owners=0;
        for(const auto& hint:runtime.suggestions()) {
            if(hint.participantId==offense.offenseOwner){++owners;CHECK(hint.offenseAction==OffenseAction::MaintainShield);}
            else CHECK(hint.offenseAction==OffenseAction::Fire);
            CHECK(offenseActionCurrent(hint,offense,count-1));
        }
        CHECK(owners==1);
        if(count>=4) {
            const auto contributor=std::find_if(runtime.suggestions().begin(),runtime.suggestions().end(),[&](const Suggestion& h){return h.participantId!=offense.offenseOwner;})->participantId;
            offense.offenseContributors={contributor};CHECK(runtime.update(0,true,{offense},ready,{}));
            for(const auto& hint:runtime.suggestions())if(hint.participantId==contributor)CHECK(hint.offenseAction==OffenseAction::ContributionComplete);
        }
        auto available=ready;available.erase(std::remove(available.begin(),available.end(),offense.offenseOwner),available.end());
        CHECK(runtime.update(0,true,{offense},available,{}));CHECK(runtime.suggestions().empty());
    }
}
void contradictoryOffenseOwnershipFactsCannotProduceInstructions() {
    auto runtime=prepared(4);const auto ready=runtime.supporterIDs();auto offense=opportunity(TaskKind::Offense,ready);
    offense.offenseBreached=true;CHECK(!runtime.update(0,true,{offense},ready,{}));
    offense.offenseOwner="person-0";CHECK(!runtime.update(0,true,{offense},ready,{})); // Driver is not an opener.
    offense.offenseOwner=ready[0];offense.offenseContributors={ready[0]};CHECK(!runtime.update(0,true,{offense},ready,{}));
    offense.offenseContributors={ready[1],ready[1]};CHECK(!runtime.update(0,true,{offense},ready,{}));
    offense.offenseContributors.clear();CHECK(runtime.update(0,true,{offense},ready,{}));
}
void allAvailabilitySubsetsAreBoundedInjectiveAndComplete() {
    for(unsigned crew=1;crew<=5;++crew)for(unsigned mask=0;mask<128;++mask) {
        auto runtime=prepared(crew+1,true);const auto ready=runtime.supporterIDs();
        const std::vector<Bundle> available={opportunity(TaskKind::Offense,ready),opportunity(TaskKind::Defense,ready),
            opportunity(TaskKind::CapsuleShell,ready,10000,20),opportunity(TaskKind::CapsuleCollect,ready,10000,21),
            opportunity(TaskKind::TetherRecover,ready),opportunity(TaskKind::NormalBoost,ready),opportunity(TaskKind::PartyBoost,ready)};
        std::vector<Bundle> selected;for(unsigned i=0;i<7;++i)if(mask&(1u<<i))selected.push_back(available[i]);
        CHECK(runtime.update(0,true,selected,ready,{}));CHECK(runtime.suggestions().size()<=crew);
        CHECK(runtime.lastEvaluationCount()>0&&runtime.lastEvaluationCount()<=15360);
        std::set<std::string> peopleUsed;
        for(const auto& hint:runtime.suggestions()) {
            CHECK(peopleUsed.insert(hint.participantId).second);
            CHECK(std::find(ready.begin(),ready.end(),hint.participantId)!=ready.end());
            bool found=false;for(const auto& fact:selected)if(fact.kind==hint.kind&&fact.targetId==hint.targetId&&fact.threatId==hint.threatId)found=true;
            CHECK(found);
        }
        for(unsigned kind=0;kind<7;++kind){const auto count=hintCount(runtime,TaskKind(kind));
            if(kind==unsigned(TaskKind::CapsuleShell)||kind==unsigned(TaskKind::CapsuleCollect))CHECK(count<=2);
            else CHECK(!count||count==Runtime::bundleSlots(TaskKind(kind),crew));}
        CHECK(!(hintCount(runtime,TaskKind::PartyBoost)&&hintCount(runtime,TaskKind::NormalBoost)));
        CHECK(!(hintCount(runtime,TaskKind::PartyBoost)&&hintCount(runtime,TaskKind::Defense)));
        // Input container order cannot change deterministic assignment.
        auto reordered=prepared(crew+1,true);std::reverse(selected.begin(),selected.end());auto reverseReady=ready;
        std::reverse(reverseReady.begin(),reverseReady.end());for(auto& fact:selected)std::reverse(fact.eligibleParticipantIds.begin(),fact.eligibleParticipantIds.end());
        CHECK(reordered.update(0,true,selected,reverseReady,{}));CHECK(hintsKey(reordered)==hintsKey(runtime));
    }
}
void defenseAndRecoveryRespectRealHandCapacity() {
    for(unsigned crew=1;crew<=5;++crew) {
        auto runtime=prepared(crew+1);const auto ready=runtime.supporterIDs();
        auto offense=opportunity(TaskKind::Offense,ready);auto defense=opportunity(TaskKind::Defense,ready);defense.imminent=true;
        CHECK(runtime.update(0,true,{offense,defense},ready,{}));CHECK(hintCount(runtime,TaskKind::Defense)==1);
        CHECK(hintCount(runtime,TaskKind::Offense)==(crew>=4?3u:0u));
        auto collection=opportunity(TaskKind::CapsuleCollect,ready);runtime.suspend();
        CHECK(runtime.prepare(1,2,2,people(crew+1),0,0,120));
        CHECK(runtime.update(0,true,{offense,defense,collection},ready,{}));CHECK(hintCount(runtime,TaskKind::Defense)==1);
        if(crew==5)CHECK(hintCount(runtime,TaskKind::Offense)==3&&hintCount(runtime,TaskKind::CapsuleCollect)==1);
        CHECK(runtime.suggestions().size()<=crew);
    }
    auto runtime=prepared(3);const auto ready=runtime.supporterIDs();
    auto offense=opportunity(TaskKind::Offense,ready);auto tether=opportunity(TaskKind::TetherRecover,ready,144);
    CHECK(runtime.update(0,true,{offense,tether},ready,{}));CHECK(hintCount(runtime,TaskKind::TetherRecover)==1);
    CHECK(hintCount(runtime,TaskKind::Offense)==0);
}
void heldOccupationIsPreservedAndInterruptionAdviceIsSeparate() {
    auto runtime=prepared(4);const auto ready=runtime.supporterIDs();auto offense=opportunity(TaskKind::Offense,ready);
    std::vector<Occupation> occupied;for(const auto& person:ready){Occupation o;static_cast<Task&>(o)=static_cast<const Task&>(offense);o.participantId=person;occupied.push_back(o);}
    CHECK(runtime.update(0,true,{offense},ready,occupied));CHECK(hintCount(runtime,TaskKind::Offense)==3);
    auto defense=opportunity(TaskKind::Defense,ready);defense.imminent=true;
    CHECK(runtime.update(0,true,{offense,defense},ready,occupied));
    CHECK(hintCount(runtime,TaskKind::Defense)==0&&hintCount(runtime,TaskKind::Offense)==3);
    CHECK(runtime.releaseAdvice().size()==1);
    CHECK(runtime.releaseAdvice()[0].participantId==ready[0]&&runtime.releaseAdvice()[0].threatId==defense.threatId);
    CHECK(runtime.releaseAdvice()[0].expiresAtTick==defense.deadline);
    for(const auto& hint:runtime.suggestions())CHECK(hint.kind==TaskKind::Offense);
    CHECK(runtime.update(1,true,{offense,defense},ready,occupied));
    for(unsigned i=1;i<4;++i)CHECK(runtime.records()[i].opportunityTicks[unsigned(OpportunityMetric::Engaged)]==1);
    // A held target outside the one rival selected as a hint is still a hand.
    occupied.resize(1);occupied[0].targetId=999;
    CHECK(runtime.update(1,true,{offense,defense},ready,occupied));
    for(const auto& hint:runtime.suggestions())CHECK(hint.participantId!=occupied[0].participantId);
    CHECK(hintCount(runtime,TaskKind::Defense)==1);
    Occupation standby;standby.participantId=ready[0];standby.kind=TaskKind::Defense;standby.unassigned=true;
    CHECK(runtime.update(1,true,{offense,defense},ready,{standby}));
    for(const auto& hint:runtime.suggestions())CHECK(hint.participantId!=standby.participantId);
    auto invalid=standby;invalid.threatId=7;CHECK(!runtime.update(1,true,{defense},ready,{invalid}));
}
void invalidationBypassesStabilityAndExpiredTargetsDisappear() {
    auto runtime=prepared(4);const auto ready=runtime.supporterIDs();auto offense=opportunity(TaskKind::Offense,ready);
    CHECK(runtime.update(0,true,{offense},ready,{}));CHECK(hintCount(runtime,TaskKind::Offense)==3);
    auto defense=opportunity(TaskKind::Defense,ready);defense.imminent=true;
    CHECK(runtime.update(1,true,{offense,defense},ready,{}));CHECK(hintCount(runtime,TaskKind::Defense)==1);
    auto soon=opportunity(TaskKind::CapsuleCollect,ready,2);runtime.suspend();CHECK(runtime.prepare(1,2,2,people(4),0,0,120));
    CHECK(runtime.update(1,true,{soon},ready,{}));CHECK(runtime.suggestions().size()==1);
    CHECK(runtime.update(2,true,{soon},ready,{}));CHECK(runtime.suggestions().empty());
    auto capsule=opportunity(TaskKind::CapsuleCollect,ready);CHECK(runtime.update(2,true,{capsule},ready,{}));
    // A newly available ordinary fact waits for the bounded ordinary refresh.
    for(Tick t=3;t<=32;++t)CHECK(runtime.update(t,true,{capsule},ready,{}));CHECK(runtime.suggestions().size()==1);
    const auto oldOwner=runtime.suggestions()[0].participantId;
    capsule.eligibleParticipantIds.erase(std::find(capsule.eligibleParticipantIds.begin(),capsule.eligibleParticipantIds.end(),oldOwner));
    CHECK(runtime.update(32,true,{capsule},ready,{}));CHECK(runtime.suggestions().size()==1&&runtime.suggestions()[0].participantId!=oldOwner);
    capsule.phaseRevision=2;CHECK(runtime.update(32,true,{capsule},ready,{}));CHECK(runtime.suggestions()[0].phaseRevision==2);
}
void urgentReleaseAdviceDisappearsImmediatelyAfterReleaseThreatChangeOrHold() {
    auto runtime=prepared(4);const auto ready=runtime.supporterIDs();auto offense=opportunity(TaskKind::Offense,ready);
    auto defense=opportunity(TaskKind::Defense,ready);defense.imminent=true;
    std::vector<Occupation> occupied;
    for(const auto& person:ready){Occupation o;static_cast<Task&>(o)=static_cast<const Task&>(offense);o.participantId=person;occupied.push_back(o);}
    CHECK(runtime.update(0,true,{offense,defense},ready,occupied));CHECK(runtime.releaseAdvice().size()==1);
    const auto advised=runtime.releaseAdvice()[0].participantId;
    occupied.erase(std::remove_if(occupied.begin(),occupied.end(),[&](const Occupation& o){return o.participantId==advised;}),occupied.end());
    CHECK(runtime.update(0,true,{offense,defense},ready,occupied));
    CHECK(runtime.releaseAdvice().empty()&&hintCount(runtime,TaskKind::Defense)==1);
    CHECK(runtime.suggestions()[0].participantId==advised); // No 0.6 s wait; input itself was never synthesized.
    Occupation back;static_cast<Task&>(back)=static_cast<const Task&>(offense);back.participantId=advised;occupied.push_back(back);
    CHECK(runtime.update(1,true,{offense,defense},ready,occupied));CHECK(runtime.releaseAdvice().size()==1);
    defense.threatId=99;CHECK(runtime.update(1,true,{offense,defense},ready,occupied));
    CHECK(runtime.releaseAdvice().size()==1&&runtime.releaseAdvice()[0].threatId==99);
    CHECK(runtime.update(1,true,{offense},ready,occupied));CHECK(runtime.releaseAdvice().empty());
    CHECK(runtime.update(1,true,{offense,defense},ready,occupied));CHECK(!runtime.releaseAdvice().empty());
    CHECK(runtime.update(1,false,{},ready,occupied));CHECK(runtime.releaseAdvice().empty());
}
void waitingOutranksAffinityWithoutRoleMonopoly() {
    auto runtime=prepared(6);const auto ready=runtime.supporterIDs();auto capsule=opportunity(TaskKind::CapsuleShell,ready);
    std::set<std::string> owners;CHECK(runtime.update(0,true,{capsule},ready,{}));owners.insert(runtime.suggestions()[0].participantId);
    const auto first=hintsKey(runtime);for(Tick tick=1;tick<runtime.profile().suggestionHold;++tick){
        CHECK(runtime.update(tick,true,{capsule},ready,{}));CHECK(hintsKey(runtime)==first);}
    for(Tick tick=runtime.profile().suggestionHold;tick<=runtime.profile().suggestionHold*5;++tick){
        CHECK(runtime.update(tick,true,{capsule},ready,{}));CHECK(runtime.suggestions().size()==1);owners.insert(runtime.suggestions()[0].participantId);}
    CHECK(owners.size()==5);for(const auto& record:runtime.records())CHECK(record.waiting<=runtime.profile().maxWait);
}
void commitCreditsAreDistinctBoundedAndPersistForPeople() {
    auto runtime=prepared(4);CHECK(!runtime.credit("person-0",ContributionKind::Opening,1));
    CHECK(!runtime.credit("unknown",ContributionKind::Opening,1));CHECK(!runtime.credit("person-1",ContributionKind(9),1));
    CHECK(!runtime.credit("person-1",ContributionKind::Opening,0));
    for(unsigned kind=0;kind<9;++kind){CHECK(runtime.credit("person-1",ContributionKind(kind),10));
        CHECK(!runtime.credit("person-1",ContributionKind(kind),10));CHECK(!runtime.credit("person-1",ContributionKind(kind),9));}
    CHECK(runtime.credit("person-2",ContributionKind::Opening,1));
    for(unsigned retry=0;retry<200;++retry){CHECK(runtime.prepare(1,1,1,people(4),0,0,120));
        CHECK(!runtime.credit("person-1",ContributionKind::Opening,10));}
    CHECK(runtime.records().size()==4&&runtime.records()[1].contributions[0]==1&&runtime.records()[2].contributions[0]==1);
    runtime.suspend();CHECK(!runtime.credit("person-1",ContributionKind::Opening,11));
    CHECK(runtime.prepare(1,2,2,people(4),0,0,120));CHECK(runtime.credit("person-1",ContributionKind::Opening,11));
    CHECK(runtime.records()[1].contributions[0]==2);CHECK(runtime.observeTurn(1,1,1));
    CHECK(runtime.prepare(1,3,3,people(4),1,1,120));CHECK(!runtime.credit("person-1",ContributionKind::Opening,12));
    CHECK(runtime.records()[1].contributions[0]==2&&runtime.credit("person-0",ContributionKind::Opening,1));
    CHECK(!runtime.credit("source-previous-person",ContributionKind::Opening,999));
    runtime.reset();CHECK(runtime.prepare(2,1,1,people(4),0,0,120));
    CHECK(runtime.records()[1].contributions[0]==0&&runtime.credit("person-1",ContributionKind::Opening,1));
}
void opportunityAccountingSeparatesReadinessPauseAndNoTarget() {
    auto runtime=prepared(3);const auto ready=runtime.supporterIDs();
    CHECK(runtime.update(0,true,{},ready,{}));CHECK(runtime.update(1,true,{},ready,{}));
    CHECK(runtime.records()[1].opportunityTicks[unsigned(OpportunityMetric::NoOpportunity)]==1);
    CHECK(runtime.update(2,true,{},std::vector<std::string>{"person-1"},{}));
    CHECK(runtime.records()[2].opportunityTicks[unsigned(OpportunityMetric::NotReady)]==1);
    CHECK(!runtime.accountPaused(1));CHECK(runtime.update(2,false,{},ready,{}));CHECK(runtime.accountPaused(240));
    CHECK(runtime.now()==2&&runtime.records()[1].opportunityTicks[unsigned(OpportunityMetric::Paused)]==240);
    CHECK(!runtime.accountPaused(-1));CHECK(runtime.records()[0].opportunityTicks[unsigned(OpportunityMetric::Paused)]==0);
    CHECK(runtime.update(2,true,{},ready,{}));CHECK(runtime.now()==2);
    CHECK(runtime.records()[1].opportunityTicks[unsigned(OpportunityMetric::NoOpportunity)]==2);
}
void malformedFactsFailClosedAndNextValidTickRecovers() {
    auto runtime=prepared(4);const auto ready=runtime.supporterIDs();auto good=opportunity(TaskKind::CapsuleCollect,ready);
    CHECK(runtime.update(0,true,{good},ready,{}));auto bad=good;bad.targetId=0;
    CHECK(!runtime.update(1,true,{bad},ready,{}));CHECK(runtime.suggestions().empty()&&runtime.now()==1);
    CHECK(runtime.update(2,true,{good},ready,{}));CHECK(runtime.suggestions().size()==1);
    CHECK(!runtime.update(2,true,{good,good},ready,{}));CHECK(runtime.suggestions().empty());
    CHECK(runtime.update(2,true,{good},ready,{}));
    auto withPilot=good;withPilot.eligibleParticipantIds.push_back("person-0");CHECK(!runtime.update(2,true,{withPilot},ready,{}));
    CHECK(!runtime.update(2,true,{good},{"person-1","person-1"},{}));
    auto twoPhases=good;twoPhases.kind=TaskKind::CapsuleShell;CHECK(!runtime.update(2,true,{good,twoPhases},ready,{}));
    CHECK(!runtime.update(4,true,{good},ready,{}));CHECK(runtime.now()==2);
    auto standard=prepared(2);auto party=opportunity(TaskKind::PartyBoost,standard.supporterIDs());
    CHECK(standard.update(0,true,{party},standard.supporterIDs(),{}));CHECK(standard.suggestions().empty());
}
void maximumFrequencyWaitAndCountersNeverWrap() {
    Runtime runtime;CHECK(runtime.setIntent(true,"sc5_crew_party_v1"));CHECK(runtime.prepare(1,1,1,people(6),0,0,1000));
    const auto ready=runtime.supporterIDs();
    auto defense=opportunity(TaskKind::Defense,{ready[0]},1000000);defense.imminent=true;
    auto offense=opportunity(TaskKind::Offense,{ready[0],ready[1],ready[2]},1000000);
    // Unequal task eligibility permits sustained waiting: the sole defender is
    // also essential to the three-person offense. Fairness cannot override a
    // real imminent defense, and its counters must therefore saturate safely.
    CHECK(runtime.update(0,true,{defense,offense},ready,{}));
    for(Tick tick=1;tick<=30005;++tick)CHECK(runtime.update(tick,true,{defense,offense},ready,{}));
    for(const auto& record:runtime.records())CHECK(record.waiting<=30000);
    CHECK(runtime.records()[2].waiting==30000&&runtime.records()[3].waiting==30000);
    CHECK(runtime.lastEvaluationCount()<=15360);
    CHECK(runtime.update(30005,false,{},ready,{}));
    CHECK(runtime.accountPaused(INT64_MAX));CHECK(runtime.accountPaused(INT64_MAX));CHECK(runtime.accountPaused(INT64_MAX));
    CHECK(runtime.countersSaturated());CHECK(runtime.records()[1].opportunityTicks[unsigned(OpportunityMetric::Paused)]==UINT64_MAX);
    uint64_t value=UINT64_MAX-1;CHECK(!saturatingAdd(&value,1)&&value==UINT64_MAX);
    CHECK(saturatingAdd(&value,1)&&value==UINT64_MAX);
    int64_t maximum=0;for(unsigned i=0;i<5;++i)maximum+=INT64_C(30001)*30001;
    CHECK(maximum==INT64_C(4500300005)&&maximum>INT32_MAX);
}

Runtime partyRuntime(unsigned crew,int fps=120) {
    Runtime r;CHECK(r.setIntent(true,"sc5_crew_party_v1"));CHECK(r.partyView().state==PartyState::Unavailable);
    CHECK(r.prepare(1,1,1,people(crew+1),0,0,fps));CHECK(r.partyView().requiredCount==(crew==1?1u:2u));return r;
}
PartyAuthority partyAuthority(const Runtime& r) {
    PartyAuthority a;a.race=r.race();a.inputEpoch=r.inputEpoch();a.rosterRevision=r.rosterRevision();
    a.driverTurnId=r.driverTurnId();a.crewIntentRevision=r.intentRevision();a.routeRevision="route";a.projectionContextId="projection";return a;
}
PartyGates partyReady() {
    PartyGates g;g.live=g.zoneQualified=g.zoneAvailable=g.humanEligible=g.reserveAvailable=true;return g;
}
std::vector<PartyBeam> partyNeutral(const Runtime& r,uint64_t release=1,uint64_t epoch=1,Tick admitted=0) {
    std::vector<PartyBeam> beams;for(const auto& person:r.supporterIDs()) {
        PartyBeam b;b.participantId=person;b.authority=partyAuthority(r);b.releaseSerial=release;b.controlEpoch=epoch;
        b.neutral=true;b.admittedTick=admitted;beams.push_back(b);
    }return beams;
}
std::vector<PartyBeam> partyPress(const Runtime& r,unsigned count,Tick admitted=0,uint64_t serial=2,uint64_t release=1,uint64_t epoch=2) {
    auto beams=partyNeutral(r,release,epoch,admitted);for(unsigned i=0;i<count;++i) {
        auto& b=beams[i];b.neutral=false;b.held=b.visible=b.partyContext=true;b.serial=serial;
    }return beams;
}
void partyDisabledAndPreprepareSnapshotsRemainCoherent() {
    Runtime r;CHECK(r.partyView().state==PartyState::Disabled&&!r.partyEnabled());
    CHECK(r.setIntent(true,"sc5_crew_party_v1"));CHECK(r.partyView().state==PartyState::Unavailable&&r.partyView().reason==PartyReason::NotPrepared);
    CHECK(r.bindRace(1,people(5),0,0,120));CHECK(r.partyView().requiredCount==2&&!r.partyView().usedThisTurn);
    auto standard=prepared(2);const auto beams=partyPress(standard,1);
    CHECK(standard.updateParty(0,partyAuthority(standard),partyReady(),beams));
    CHECK(!standard.partyWanted()&&!standard.confirmParty(true)&&standard.partyView().state==PartyState::Disabled);
    CHECK(standard.partyActivations()==0);
}
void partyHoldQuorumAndPhysicalAcknowledgementAtEveryFrequency() {
    for(int fps:{60,61,120,144,1000})for(unsigned crew:{1u,2u,3u,5u}) {
        auto r=partyRuntime(crew,fps);const auto a=partyAuthority(r);const auto g=partyReady();
        CHECK(r.updateParty(0,a,g,partyNeutral(r)));auto pressed=partyPress(r,std::min(crew,2u));
        CHECK(r.updateParty(0,a,g,pressed));CHECK(r.partyView().proposalId==1&&r.partyView().deadlineTick==3*fps);
        for(Tick t=1;t<r.profile().partyHold;++t){CHECK(r.updateParty(t,a,g,pressed));CHECK(!r.partyWanted());}
        const Tick mature=r.profile().partyHold;CHECK(r.updateParty(mature,a,g,pressed));CHECK(r.partyWanted());
        CHECK(r.partyView().consentingParticipantIds.size()==(crew==1?1u:2u));
        CHECK(r.confirmParty(false)&&!r.partyView().usedThisTurn&&r.partyActivations()==0);CHECK(!r.confirmParty(true));
        const Tick restart=mature+1;CHECK(r.updateParty(restart,a,g,partyNeutral(r,3,3,restart)));
        pressed=partyPress(r,std::min(crew,2u),restart,4,3,4);CHECK(r.updateParty(restart,a,g,pressed));
        CHECK(r.partyView().proposalId==2);for(Tick t=restart+1;t<=restart+r.profile().partyHold;++t)CHECK(r.updateParty(t,a,g,pressed));
        CHECK(r.partyWanted()&&r.confirmParty(true));CHECK(!r.confirmParty(true));
        CHECK(r.partyView().usedThisTurn&&r.partyView().state==PartyState::Used&&r.partyActivations()==1);
    }
}
void partyPendingProofUsesOriginalAdmissionAndRejectsNegativeRepair() {
    auto r=partyRuntime(1,60);const auto a=partyAuthority(r);const auto g=partyReady();
    CHECK(r.updateParty(0,a,g,partyNeutral(r)));auto pressed=partyPress(r,1);pressed[0].pending=true;pressed[0].visible=false;
    CHECK(r.updateParty(0,a,g,pressed));CHECK(r.updateParty(1,a,g,pressed));CHECK(!r.partyWanted()&&r.partyView().proposalId==0);
    pressed[0].pending=false;pressed[0].visible=true;CHECK(r.updateParty(2,a,g,pressed));
    CHECK(r.partyView().proposalId==1&&r.partyView().deadlineTick==180);
    for(Tick t=3;t<45;++t){CHECK(r.updateParty(t,a,g,pressed));CHECK(!r.partyWanted());}
    CHECK(r.updateParty(45,a,g,pressed)&&r.partyWanted());CHECK(r.confirmParty(false));
    auto q=partyRuntime(1,60);CHECK(q.updateParty(0,partyAuthority(q),g,partyNeutral(q)));
    auto delayed=partyPress(q,1);delayed[0].pending=true;delayed[0].visible=false;
    CHECK(q.updateParty(0,partyAuthority(q),g,delayed));delayed[0].pending=false;delayed[0].interrupted=true;
    CHECK(q.updateParty(1,partyAuthority(q),g,delayed));delayed[0].interrupted=false;delayed[0].visible=true;
    for(Tick t=2;t<=60;++t)CHECK(q.updateParty(t,partyAuthority(q),g,delayed));
    CHECK(!q.partyWanted()&&q.partyActivations()==0&&q.partyView().proposalId==0);
    CHECK(q.partyFeedback()[0].reason==PartyReason::ReleaseRequired);
    // A positive result for a different control epoch cannot complete the
    // queued proof merely because it copied the old immutable press serial.
    auto different=partyRuntime(1,60);const auto da=partyAuthority(different);
    CHECK(different.updateParty(0,da,g,partyNeutral(different)));
    auto changed=partyPress(different,1);changed[0].pending=true;changed[0].visible=false;
    CHECK(different.updateParty(0,da,g,changed));++changed[0].controlEpoch;
    changed[0].pending=false;changed[0].visible=true;
    for(Tick t=1;t<=46;++t)CHECK(different.updateParty(t,da,g,changed));
    CHECK(!different.partyWanted()&&different.partyView().proposalId==0);
}
void partyExpiryIsExclusiveAndZeroCannotReopenAnOldHold() {
    for(int offset:{-1,0,1}) {
        auto r=partyRuntime(2,60);const auto a=partyAuthority(r);const auto g=partyReady();
        CHECK(r.updateParty(0,a,g,partyNeutral(r)));auto beams=partyPress(r,1);CHECK(r.updateParty(0,a,g,beams));
        const Tick second=180-45+offset;
        for(Tick t=1;t<=180+offset;++t) {
            if(t==second){beams[1]=partyPress(r,2,t)[1];beams[1].capturedProposalId=1;}
            CHECK(r.updateParty(t,a,g,beams));
            if(t<180+offset)CHECK(!r.partyWanted());
        }
        if(offset<0){CHECK(r.partyWanted());CHECK(r.confirmParty(true));}
        else {CHECK(!r.partyWanted()&&r.partyView().proposalId==0);
            if(offset==0)CHECK(r.partyView().reason==PartyReason::ConsensusExpired);
            for(Tick t=181+offset;t<=190;++t)CHECK(r.updateParty(t,a,g,beams));CHECK(r.partyView().proposalId==0);}
    }
    auto r=partyRuntime(1);auto g=partyReady();CHECK(r.updateParty(0,partyAuthority(r),g,partyNeutral(r)));
    auto pending=partyPress(r,1);pending[0].visible=false;pending[0].pending=true;
    CHECK(r.updateParty(0,partyAuthority(r),g,pending));g.threatActive=true;CHECK(r.updateParty(1,partyAuthority(r),g,pending));
    g.threatActive=false;pending[0].pending=false;pending[0].visible=true;
    for(Tick t=2;t<=100;++t)CHECK(r.updateParty(t,partyAuthority(r),g,pending));CHECK(!r.partyWanted()&&r.partyView().proposalId==0);
}
void partyQuorumUsesOriginalAdmissionAcrossAsyncCompletionAndIdTies() {
    auto r=partyRuntime(3,60);const auto a=partyAuthority(r);const auto g=partyReady();
    CHECK(r.updateParty(0,a,g,partyNeutral(r)));auto beams=partyPress(r,3);
    beams[0].pending=true;beams[0].visible=false;beams[0].admittedTick=0;
    beams[1].admittedTick=0;beams[2].admittedTick=0;
    CHECK(r.updateParty(0,a,g,beams));CHECK(r.partyView().proposalId==1);
    CHECK(r.updateParty(1,a,g,beams));beams[0].pending=false;beams[0].visible=true;
    CHECK(r.updateParty(2,a,g,beams));for(Tick t=3;t<=45;++t)CHECK(r.updateParty(t,a,g,beams));
    CHECK(r.partyWanted());CHECK((r.partyView().consentingParticipantIds==std::vector<std::string>{"person-1","person-2"}));
    CHECK(!r.partyFeedback()[2].consenting&&r.partyFeedback()[2].reason==PartyReason::ServiceBusy);
    auto q=partyRuntime(3,60);const auto qa=partyAuthority(q);CHECK(q.updateParty(0,qa,g,partyNeutral(q)));
    auto later=partyNeutral(q);later[2]=partyPress(q,3,0)[2];later[2].pending=true;later[2].visible=false;
    CHECK(q.updateParty(0,qa,g,later));later[0]=partyPress(q,1,1)[0];later[1]=partyPress(q,2,1)[1];
    CHECK(q.updateParty(1,qa,g,later));later[2].pending=false;later[2].visible=true;
    CHECK(q.updateParty(2,qa,g,later));CHECK(q.partyView().deadlineTick==180);
    for(Tick t=3;t<=46;++t)CHECK(q.updateParty(t,qa,g,later));
    CHECK((q.partyView().consentingParticipantIds==std::vector<std::string>{"person-3","person-1"}));
    // Losing a selected vote cannot promote a previously unselected candidate.
    later[2].visible=false;later[2].interrupted=true;CHECK(q.updateParty(47,qa,g,later));
    later[2].visible=true;later[2].interrupted=false;CHECK(q.updateParty(48,qa,g,later));CHECK(!q.partyWanted()&&q.partyView().proposalId==0);
}
void partyAuthorityReleaseVetoAndQuotaSurviveReprepare() {
    auto r=partyRuntime(2);auto a=partyAuthority(r);auto g=partyReady();CHECK(r.updateParty(0,a,g,partyNeutral(r)));
    auto beams=partyPress(r,2);CHECK(r.updateParty(0,a,g,beams));
    g.driverVeto=true;CHECK(r.updateParty(1,a,g,beams));CHECK(r.partyView().reason==PartyReason::DriverVeto);
    g.driverVeto=false;for(Tick t=2;t<=91;++t)CHECK(r.updateParty(t,a,g,beams));CHECK(!r.partyWanted());
    CHECK(r.updateParty(92,a,g,partyNeutral(r,3,3,92)));beams=partyPress(r,2,92,4,3,4);
    CHECK(r.updateParty(92,a,g,beams));for(Tick t=93;t<=182;++t)CHECK(r.updateParty(t,a,g,beams));CHECK(r.confirmParty(true));
    for(unsigned retry=0;retry<200;++retry)CHECK(r.prepare(1,1,1,people(3),0,0,120));CHECK(r.partyView().usedThisTurn&&r.partyActivations()==1);
    r.suspend();CHECK(r.partyView().usedThisTurn&&r.partyActivations()==1);
    CHECK(r.prepare(1,2,2,people(3),0,0,120));CHECK(r.partyView().usedThisTurn);
    CHECK(r.updateParty(183,partyAuthority(r),g,partyNeutral(r,5,5,183)));CHECK(r.partyView().state==PartyState::Used);
    CHECK(r.observeTurn(1,1,1));CHECK(!r.partyView().usedThisTurn&&r.partyActivations()==1);
    CHECK(r.prepare(1,3,3,people(3),1,1,120));CHECK(r.partyView().requiredCount==2);
    r.reset();CHECK(r.partyActivations()==0&&!r.partyView().usedThisTurn&&r.partyEnabled());
}
void partyGateReasonsDoNotSpendQuotaOrAllowLatentPending() {
    for(unsigned cause=0;cause<13;++cause) {
        auto r=partyRuntime(1);auto a=partyAuthority(r);auto g=partyReady();CHECK(r.updateParty(0,a,g,partyNeutral(r)));
        auto b=partyPress(r,1);b[0].pending=true;b[0].visible=false;CHECK(r.updateParty(0,a,g,b));
        PartyReason expected=PartyReason::Ready;
        if(cause==0){g.driverVeto=true;expected=PartyReason::DriverVeto;}
        if(cause==1){g.normalBoostPriority=true;expected=PartyReason::NormalBoostPriority;}
        if(cause==2){g.threatActive=true;expected=PartyReason::ThreatActive;}
        if(cause==3){g.impactThisTick=true;expected=PartyReason::ThreatActive;}
        if(cause==4){g.reinforcing=true;expected=PartyReason::ReinforcementActive;}
        if(cause==5){g.nativeBoostActive=true;expected=PartyReason::NativeBoostActive;}
        if(cause==6){g.humanEligible=false;expected=PartyReason::KartUnavailable;}
        if(cause==7){g.zoneQualified=false;expected=PartyReason::ZoneUnqualified;}
        if(cause==8){g.zoneAvailable=false;expected=PartyReason::ZoneUnavailable;}
        if(cause==9){g.cooldownUntilTick=100;expected=PartyReason::Cooldown;}
        if(cause==10){g.reserveAvailable=false;expected=PartyReason::ReserveProtected;}
        if(cause==11){g.serviceBusy=true;expected=PartyReason::ServiceBusy;}
        if(cause==12){g.live=false;expected=PartyReason::NotPrepared;}
        CHECK(r.updateParty(1,a,g,b));CHECK(r.partyView().reason==expected&&!r.partyView().usedThisTurn);
        g=partyReady();b[0].pending=false;b[0].visible=true;for(Tick t=2;t<=100;++t)CHECK(r.updateParty(t,a,g,b));
        CHECK(!r.partyWanted()&&r.partyActivations()==0&&r.partyView().proposalId==0);
    }
}
void partyRejectsDuplicatePeopleOldAuthorityAndBootstrapAcrossRebind() {
    auto r=partyRuntime(2);auto a=partyAuthority(r);auto g=partyReady();CHECK(r.updateParty(0,a,g,partyNeutral(r)));
    auto b=partyPress(r,2);b[1]=b[0];CHECK(!r.updateParty(0,a,g,b));CHECK(!r.partyWanted());
    CHECK(r.updateParty(0,a,g,partyNeutral(r)));b=partyPress(r,2);CHECK(r.updateParty(0,a,g,b));
    auto old=a;old.driverTurnId=1;CHECK(!r.updateParty(1,old,g,b));CHECK(!r.partyWanted());
    CHECK(r.prepare(1,2,2,people(3),0,0,120));a=partyAuthority(r);CHECK(r.updateParty(2,a,g,partyNeutral(r,3,3,2)));
    CHECK(!r.updateParty(2,a,g,b));CHECK(r.partyView().proposalId==0);
    b=partyPress(r,2,0,4,3,4);CHECK(r.updateParty(2,a,g,b));CHECK(r.partyView().proposalId==0);
    CHECK(r.updateParty(3,a,g,partyNeutral(r,5,5,3)));b=partyPress(r,2,3,6,5,6);CHECK(r.updateParty(3,a,g,b));
    CHECK(r.partyView().proposalId>1);b[1].capturedProposalId=r.partyView().proposalId+1;
    CHECK(r.updateParty(4,a,g,b));CHECK(!r.partyWanted()&&r.partyView().proposalId==0);
}
void advisoryTechnicalHoldAlignsTickAndReadyAllowsPreviousTickPress() {
    auto r=partyRuntime(1);auto a=partyAuthority(r);auto g=partyReady();
    CHECK(r.update(0,true,{},r.supporterIDs(),{}));CHECK(r.update(1,false,{},r.supporterIDs(),{}));
    CHECK(r.now()==1&&r.update(1,false,{},r.supporterIDs(),{}));CHECK(r.update(2,true,{},r.supporterIDs(),{}));CHECK(r.now()==2);
    for(const auto& record:r.records())for(uint64_t n:record.opportunityTicks)CHECK(n<=1);
    auto q=partyRuntime(1,60);auto qa=partyAuthority(q);auto notLive=partyReady();notLive.live=false;
    CHECK(q.updateParty(0,qa,notLive,{}));auto pressed=partyPress(q,1,0);
    // UP and DOWN were both admitted after Ready on the same frozen tick.
    CHECK(q.updateParty(1,qa,partyReady(),pressed));CHECK(q.partyView().proposalId==1&&q.partyView().deadlineTick==180);
    for(Tick t=2;t<=45;++t)CHECK(q.updateParty(t,qa,partyReady(),pressed));CHECK(q.partyWanted());
}
struct Case{const char* name;void(*run)();};const Case cases[]={
    {"urgentReleaseAdviceDisappearsImmediatelyAfterReleaseThreatChangeOrHold",urgentReleaseAdviceDisappearsImmediatelyAfterReleaseThreatChangeOrHold},
    {"offenseActionsFollowTheActualOwnerAcrossShieldPhasesAndUniversalCrews",offenseActionsFollowTheActualOwnerAcrossShieldPhasesAndUniversalCrews},
    {"contradictoryOffenseOwnershipFactsCannotProduceInstructions",contradictoryOffenseOwnershipFactsCannotProduceInstructions},
    {"rolesMatchEveryNativeRaceAndFairnessCalculation",rolesMatchEveryNativeRaceAndFairnessCalculation},
    {"partyDisabledAndPreprepareSnapshotsRemainCoherent",partyDisabledAndPreprepareSnapshotsRemainCoherent},
    {"partyHoldQuorumAndPhysicalAcknowledgementAtEveryFrequency",partyHoldQuorumAndPhysicalAcknowledgementAtEveryFrequency},
    {"partyPendingProofUsesOriginalAdmissionAndRejectsNegativeRepair",partyPendingProofUsesOriginalAdmissionAndRejectsNegativeRepair},
    {"partyExpiryIsExclusiveAndZeroCannotReopenAnOldHold",partyExpiryIsExclusiveAndZeroCannotReopenAnOldHold},
    {"partyQuorumUsesOriginalAdmissionAcrossAsyncCompletionAndIdTies",partyQuorumUsesOriginalAdmissionAcrossAsyncCompletionAndIdTies},
    {"partyAuthorityReleaseVetoAndQuotaSurviveReprepare",partyAuthorityReleaseVetoAndQuotaSurviveReprepare},
    {"partyGateReasonsDoNotSpendQuotaOrAllowLatentPending",partyGateReasonsDoNotSpendQuotaOrAllowLatentPending},
    {"partyRejectsDuplicatePeopleOldAuthorityAndBootstrapAcrossRebind",partyRejectsDuplicatePeopleOldAuthorityAndBootstrapAcrossRebind},
    {"advisoryTechnicalHoldAlignsTickAndReadyAllowsPreviousTickPress",advisoryTechnicalHoldAlignsTickAndReadyAllowsPreviousTickPress},
    {"allAvailabilitySubsetsAreBoundedInjectiveAndComplete",allAvailabilitySubsetsAreBoundedInjectiveAndComplete},
    {"defenseAndRecoveryRespectRealHandCapacity",defenseAndRecoveryRespectRealHandCapacity},
    {"heldOccupationIsPreservedAndInterruptionAdviceIsSeparate",heldOccupationIsPreservedAndInterruptionAdviceIsSeparate},
    {"invalidationBypassesStabilityAndExpiredTargetsDisappear",invalidationBypassesStabilityAndExpiredTargetsDisappear},
    {"waitingOutranksAffinityWithoutRoleMonopoly",waitingOutranksAffinityWithoutRoleMonopoly},
    {"commitCreditsAreDistinctBoundedAndPersistForPeople",commitCreditsAreDistinctBoundedAndPersistForPeople},
    {"opportunityAccountingSeparatesReadinessPauseAndNoTarget",opportunityAccountingSeparatesReadinessPauseAndNoTarget},
    {"malformedFactsFailClosedAndNextValidTickRecovers",malformedFactsFailClosedAndNextValidTickRecovers},
    {"maximumFrequencyWaitAndCountersNeverWrap",maximumFrequencyWaitAndCountersNeverWrap},
    {"heldPreviewHasRolesButCannotClaimPrepared",heldPreviewHasRolesButCannotClaimPrepared},
    {"previewProfileChoiceAndMissedFullRotationRemainHeld",previewProfileChoiceAndMissedFullRotationRemainHeld},
    {"disablingNeverPreparedPreviewClearsRolesAndKeepsIntentRevision",disablingNeverPreparedPreviewClearsRolesAndKeepsIntentRevision},
    {"observedTurnLocksEvenNeverPreparedPreviewAgainstDowngrade",observedTurnLocksEvenNeverPreparedPreviewAgainstDowngrade},
    {"nativeTurnCommitIsTheOnlyAdvancement",nativeTurnCommitIsTheOnlyAdvancement},
    {"sameRaceReprepareSuspendAndResetKeepCorrectAuthority",sameRaceReprepareSuspendAndResetKeepCorrectAuthority},
    {"frozenRosterAndProfileCannotBeRewrittenByRetry",frozenRosterAndProfileCannotBeRewrittenByRetry},
    {"invalidInitialPreparationDoesNotSeedRace",invalidInitialPreparationDoesNotSeedRace},
    {"competenceModuloRemainsExactAtUInt64Maximum",competenceModuloRemainsExactAtUInt64Maximum}};
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto& c:cases)std::cout<<c.name<<'\n';return 0;}
    if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;
    bool found=false;for(const auto& c:cases){if(argc==3&&std::string(argv[2])!=c.name)continue;c.run();found=true;}
    return found?0:2;
}
