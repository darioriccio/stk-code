// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_mission.hpp"
#include "modes/immersive_relay/scintilla_mission_result_json.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>

#define CHECK(x) do {if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);}} while(false)
using namespace ScintillaMission;
namespace
{
void resultJSONKeepsWideIdentitiesAndAbsentOfficialFacts()
{
    Runtime r;
    const uint64_t race=std::numeric_limits<uint64_t>::max();
    CHECK(r.prepare(race,UINT64_C(9007199254740993),3,4,{"A"},120));
    CHECK(r.abort("operator_abort",1));
    std::ostringstream out;writeResultJSON(out,r.result());
    const std::string value=out.str();
    CHECK(value.find("\"raceEpoch\":\"18446744073709551615\"")!=std::string::npos);
    CHECK(value.find("\"missionRevision\":\"9007199254740993\"")!=std::string::npos);
    CHECK(value.find("\"officialTeamRacePosition\":null")!=std::string::npos);
    CHECK(value.find("\"nativeFinishTime\":null")!=std::string::npos);
    CHECK(value.find("\"outcome\":\"not_completed\"")!=std::string::npos);
    std::ostringstream absent;writeResultJSON(absent,Result());CHECK(absent.str()=="null");
}

struct Fixture
{
    Runtime r;
    uint64_t serial=1;
    std::array<uint64_t,Profile::MaxCrew> epochs={};
    explicit Fixture(unsigned count=1,int fps=120)
    {
        std::vector<std::string> people={"A","B","C","D","E"};people.resize(count);
        CHECK(r.prepare(1,2,3,4,people,fps));CHECK(r.advanceTo(0,true));
    }
    CapsuleSpec spec(unsigned index) const
    {
        CapsuleSpec s;s.id=Runtime::CapsuleEntityBase+index;s.instance=100+index;
        s.module=2000000+index;s.occurrence=index;s.expires=100000;
        return s;
    }
    Shot shot(uint64_t entity,unsigned person=0)
    {
        const Snapshot s=r.snapshot(entity);Shot result;
        result.race=r.race();result.revision=r.revision();result.inputEpoch=r.inputEpoch();
        result.entity=entity;result.instance=s.instance;result.phaseRevision=s.phaseRevision;
        result.participant=r.crew()[person];result.serial=serial++;
        result.event="event_"+std::to_string(result.serial);
        result.historicalEligible=result.admittedEligible=true;return result;
    }
    uint64_t addOpen(unsigned index)
    {
        const CapsuleSpec s=spec(index);CHECK(r.activateCapsule(s).changed);
        CHECK(r.openCapsule(shot(s.id)).changed);return s.id;
    }
    Beam press(uint64_t entity,unsigned person=0)
    {
        const Snapshot s=r.snapshot(entity);Beam b;
        b.race=r.race();b.revision=r.revision();b.inputEpoch=r.inputEpoch();
        b.entity=entity;b.instance=s.instance;b.phaseRevision=s.phaseRevision;b.kind=s.kind;
        b.participant=r.crew()[person];b.controlEpoch=++epochs[person];b.serial=serial++;
        b.held=b.visible=b.historicalEligible=true;return b;
    }
    Beam release(unsigned person=0)
    {
        Beam b;b.race=r.race();b.revision=r.revision();b.inputEpoch=r.inputEpoch();
        b.participant=r.crew()[person];b.controlEpoch=++epochs[person];b.serial=serial++;
        return b;
    }
    void step(const std::vector<Beam>& beams)
    {CHECK(r.advanceTo(r.tick()+1,true));r.updateBeams(beams);}
    void wait(Tick count,const std::vector<Beam>& beams={})
    {for(Tick i=0;i<count;++i) step(beams);}
    void collect(unsigned index,unsigned person=0)
    {
        const uint64_t entity=addOpen(index);
        r.updateBeams({release(person)});
        const Beam b=press(entity,person);r.updateBeams({b});wait(r.profile().collect,{b});
        CHECK(r.snapshot(entity).phase==Phase::Collected);
        r.updateBeams({release(person)});
    }
    void impact(uint64_t threat=1)
    {
        Impact i;i.race=r.race();i.humanInstance=r.humanInstance();i.threat=threat;
        CHECK(r.consumeImpact(i).accepted);
    }
    Beam recoverPress()
    {
        const Snapshot s=r.snapshot(Runtime::TetherEntityId);
        CHECK(r.confirmTetherPresented(s.id,s.instance,s.phaseRevision));
        r.updateBeams({release()});return press(Runtime::TetherEntityId);
    }
};

const Feedback& feedback(const std::vector<Feedback>& values,const std::string& participant)
{
    for(const Feedback& f:values) if(f.participant==participant) return f;
    CHECK(false);return values.front();
}

void conservation(const Runtime& r)
{
    std::set<uint64_t> modules;unsigned tethered=0;
    for(const Slot& s:r.slots())
    {
        if(s.state==SlotState::Empty) CHECK(s.module==0);
        else {CHECK(s.module!=0);CHECK(modules.insert(s.module).second);}
        if(s.state==SlotState::Tethered) ++tethered;
    }
    CHECK(tethered<=1);CHECK(r.occupiedCount()<=3);
    for(const Snapshot& s:r.snapshots())
    {
        CHECK(s.module!=0);
        if(s.kind==Kind::Tether) CHECK(modules.count(s.module)==1 && tethered==1);
        else CHECK(modules.count(s.module)==0);
    }
}

void profileRoundingAndGeometryBoundaries()
{
    for(int fps:{60,120,144})
    {
        Profile p;CHECK(Profile::create(fps,&p));
        CHECK(p.collect==(600000*fps+999999)/1000000);
        CHECK(p.recoveryWindow==3*fps);CHECK(p.recover==p.collect);
    }
    Profile p;p.fps=7;CHECK(!Profile::create(0,&p));CHECK(p.fps==7);
    CHECK(!Profile::create(1001,&p));CHECK(!Profile::create(120,nullptr));
    CHECK(Profile::inRange(Kind::Capsule,75*75,2));
    CHECK(!Profile::inRange(Kind::Capsule,75*75+0.001,2));
    CHECK(Profile::inRange(Kind::Capsule,8*8,2));
    CHECK(!Profile::inRange(Kind::Capsule,8*8-0.001,2));
    CHECK(Profile::inRange(Kind::Tether,0.75*0.75,0.5));
    CHECK(Profile::inRange(Kind::Tether,15*15,0.5));
    CHECK(!Profile::inRange(Kind::Tether,15*15+0.001,0.5));
    CHECK(!Profile::inRange(Kind::Capsule,50*50,0.1));
    CHECK(!Profile::inRange(Kind::Capsule,std::numeric_limits<double>::infinity(),2));
    CHECK(!Profile::inRange(Kind::Capsule,100,std::numeric_limits<double>::quiet_NaN()));
}

void configurationRejectsMalformedAndConflictingIdentity()
{
    Runtime r;CHECK(!r.prepare(0,2,3,4,{"A"},120));
    CHECK(!r.prepare(1,0,3,4,{"A"},120));CHECK(!r.prepare(1,2,0,4,{"A"},120));
    CHECK(!r.prepare(1,2,3,0,{"A"},120));CHECK(!r.prepare(1,2,3,4,{},120));
    CHECK(!r.prepare(1,2,3,4,{"A","A"},120));
    CHECK(!r.prepare(1,2,3,4,{"A","B","C","D","E","F"},120));
    CHECK(!r.prepare(1,2,3,4,{std::string(129,'X')},120));
    CHECK(r.prepare(1,2,3,4,{"A"},120));CHECK(r.advanceTo(0,true));
    CHECK(!r.prepare(2,2,3,4,{"A"},120));CHECK(!r.prepare(1,3,3,4,{"A"},120));
    CHECK(!r.prepare(1,2,2,4,{"A"},120));CHECK(!r.prepare(1,2,3,5,{"A"},120));
    CHECK(!r.prepare(1,2,3,4,{"B"},120));CHECK(!r.prepare(1,2,4,4,{"A","B"},120));
    CHECK(!r.prepare(1,2,3,4,{"A"},144));CHECK(!r.setRequired(false));
}

void sequentialSoloCollectionConservesModuleIdentity()
{
    Fixture f;
    for(unsigned i=0;i<3;++i)
    {f.collect(i);CHECK(f.r.securedCount()==i+1);CHECK(f.r.slots()[i].module==f.spec(i).module);conservation(f.r);}
    CHECK(f.r.occupiedCount()==3);
    const Outcome skipped=f.r.activateCapsule(f.spec(3));
    CHECK(skipped.accepted && skipped.status==Status::OccurrenceSkipped);
    CHECK(f.r.opportunityReceiptCount()==4);CHECK(f.r.snapshots().empty());
}

void wideIdentitiesStayExactAndInvalidSpecsDoNotConsumeReceipts()
{
    Fixture f;CapsuleSpec s=f.spec(0);
    s.id=Runtime::CapsuleEntityBase+1;s.instance=std::numeric_limits<uint64_t>::max();
    s.module=(UINT64_C(1)<<54)+3;
    CapsuleSpec invalid=s;invalid.id=Runtime::TetherEntityId;
    CHECK(!f.r.activateCapsule(invalid).accepted);
    invalid=s;invalid.occurrence=Profile::MaxOpportunities;
    CHECK(!f.r.activateCapsule(invalid).accepted);
    invalid=s;invalid.expires=f.r.tick();CHECK(!f.r.activateCapsule(invalid).accepted);
    CHECK(f.r.opportunityReceiptCount()==0);CHECK(f.r.activateCapsule(s).changed);
    CHECK(f.r.openCapsule(f.shot(s.id)).changed);
    const Beam b=f.press(s.id);f.r.updateBeams({b});f.wait(72,{b});
    CHECK(f.r.slots()[0].module==s.module && f.r.snapshot(s.id).instance==s.instance);
    CHECK(f.r.snapshot(s.id).id==s.id);conservation(f.r);
}

void shellOpeningReceiptIsIdempotentAfterOtherActions()
{
    Fixture f(2);CHECK(f.r.activateCapsule(f.spec(0)).changed);
    const Shot original=f.shot(f.spec(0).id);CHECK(f.r.openCapsule(original).changed);
    const Snapshot first=f.r.snapshot(original.entity);
    f.addOpen(1);CHECK(f.r.openCapsule(f.shot(original.entity)).status==Status::UseTool);
    const Outcome retry=f.r.openCapsule(original);
    CHECK(retry.accepted && !retry.changed && retry.status==Status::ShellOpened);
    CHECK(f.r.snapshot(original.entity).phaseRevision==first.phaseRevision);
    Shot forged=original;forged.event="different";CHECK(!f.r.openCapsule(forged).accepted);
}

void toolHeldBeforeShellOpensRequiresANewPress()
{
    Fixture f;CHECK(f.r.activateCapsule(f.spec(0)).changed);
    Beam early=f.press(f.spec(0).id);f.r.updateBeams({early});
    CHECK(feedback(f.r.feedback(),"A").status==Status::ReleaseRequired);
    CHECK(f.r.openCapsule(f.shot(early.entity)).changed);
    early.phaseRevision=f.r.snapshot(early.entity).phaseRevision;
    f.wait(100,{early});CHECK(f.r.securedCount()==0);
    f.r.updateBeams({f.release()});const Beam fresh=f.press(early.entity);
    f.r.updateBeams({fresh});f.wait(72,{fresh});CHECK(f.r.securedCount()==1);
}

void unobservedOldPressCannotUseANewOpenProof()
{
    Fixture f;CHECK(f.r.activateCapsule(f.spec(0)).changed);
    Beam old=f.press(f.spec(0).id);CHECK(f.r.openCapsule(f.shot(old.entity)).changed);
    old.phaseRevision=f.r.snapshot(old.entity).phaseRevision;
    f.r.updateBeams({old});f.wait(100,{old});CHECK(f.r.securedCount()==0);
    CHECK(feedback(f.r.feedback(),"A").status==Status::ReleaseRequired);
}

void exclusiveOwnershipUsesPressSerialThenParticipant()
{
    for(bool reverse:{false,true})
    {
        Fixture f(2);const uint64_t entity=f.addOpen(0);
        Beam b=f.press(entity,1),a=f.press(entity,0);
        f.r.updateBeams(reverse?std::vector<Beam>{a,b}:std::vector<Beam>{b,a});
        CHECK(f.r.snapshot(entity).owner==1);
        CHECK(feedback(f.r.feedback(),"A").status==Status::TargetBusy);
        f.wait(71,{a,b});CHECK(f.r.securedCount()==0);
        f.step({a,b});CHECK(f.r.securedCount()==1);conservation(f.r);
    }
    Fixture f(2);const uint64_t entity=f.addOpen(0);
    Beam b=f.press(entity,1),a=f.press(entity,0);a.serial=b.serial;
    f.r.updateBeams({b,a});CHECK(f.r.snapshot(entity).owner==0);
}

void twoCapsulesCompeteForTheLastEmptySlot()
{
    Fixture f(2);f.collect(0);f.collect(1);
    const uint64_t x=f.addOpen(2),y=f.addOpen(3);
    f.r.updateBeams({f.release(0),f.release(1)});
    const Beam b=f.press(y,1),a=f.press(x,0);f.r.updateBeams({a,b});
    f.wait(72,{a,b});CHECK(f.r.securedCount()==3);
    CHECK(f.r.slots()[2].module==f.spec(3).module);
    CHECK(f.r.snapshot(x).phase==Phase::Open);CHECK(f.r.snapshot(y).phase==Phase::Collected);
    CHECK(feedback(f.r.feedback(),"A").outcome==Status::CargoFull);conservation(f.r);
}

void stickyInterruptionCannotBeHealedByALaterHit()
{
    Fixture f;const Beam b=f.press(f.addOpen(0));f.r.updateBeams({b});f.wait(40,{b});
    Beam negative=b;negative.interrupted=true;f.step({negative});
    f.wait(100,{b});CHECK(f.r.securedCount()==0);
    CHECK(feedback(f.r.feedback(),"A").status==Status::ReleaseRequired);
    f.r.updateBeams({f.release()});const Beam fresh=f.press(b.entity);
    f.r.updateBeams({fresh});f.wait(72,{fresh});CHECK(f.r.securedCount()==1);
}

void targetInstancePhaseAndEpochChangesCannotTransferProgress()
{
    for(unsigned mutation=0;mutation<6;++mutation)
    {
        Fixture f(2);const Beam b=f.press(f.addOpen(0));f.addOpen(1);
        f.r.updateBeams({b});f.wait(40,{b});Beam changed=b;
        if(mutation==0) changed.entity=f.spec(1).id;
        if(mutation==1) ++changed.instance;
        if(mutation==2) ++changed.phaseRevision;
        if(mutation==3) ++changed.inputEpoch;
        if(mutation==4) ++changed.controlEpoch;
        if(mutation==5) changed.participant="B";
        f.step({changed});f.wait(100,{b});CHECK(f.r.securedCount()==0);conservation(f.r);
    }
}

void releaseAndPressBetweenTicksUseAnAdmittedReceipt()
{
    Fixture f;Beam b=f.press(f.addOpen(0));f.r.updateBeams({b});f.wait(30,{b});
    Beam miss=b;miss.visible=false;f.step({miss});
    // The released state is not sampled by gameplay; SC1 retains its receipt.
    const Beam release=f.release();Beam fresh=f.press(b.entity);fresh.releaseSerial=release.serial;
    f.step({fresh});f.wait(71,{fresh});CHECK(f.r.securedCount()==0);
    f.step({fresh});CHECK(f.r.securedCount()==1);
}

void releaseReceiptBeforeAnInvalidPressCannotRearmIt()
{
    Fixture f;const uint64_t entity=f.addOpen(0);const Beam release=f.release();
    Beam invalid=f.press(entity);invalid.releaseSerial=release.serial;invalid.visible=false;
    f.r.updateBeams({invalid});Beam healed=invalid;healed.visible=true;
    ++healed.controlEpoch;healed.serial=f.serial++;
    f.wait(100,{healed});CHECK(f.r.securedCount()==0);
    const Beam actualRelease=f.release();Beam fresh=f.press(entity);
    // Account for the deliberately forged epoch in the preceding negative case.
    fresh.controlEpoch=healed.controlEpoch+2;fresh.releaseSerial=actualRelease.serial;
    f.r.updateBeams({fresh});f.wait(72,{fresh});CHECK(f.r.securedCount()==1);
}

void missingDuplicateAndOversizedSamplesFailClosed()
{
    for(unsigned mutation=0;mutation<3;++mutation)
    {
        Fixture f;const Beam b=f.press(f.addOpen(0));f.r.updateBeams({b});f.wait(40,{b});
        if(mutation==0) f.step({});
        if(mutation==1) f.step({b,b});
        if(mutation==2) f.step(std::vector<Beam>(6,b));
        f.wait(100,{b});CHECK(f.r.securedCount()==0);
    }
}

void appliedImpactDetachesAtMostOneAndConsumesEmptyReceipts()
{
    for(unsigned cargo=0;cargo<=3;++cargo)
    {
        Fixture f;for(unsigned i=0;i<cargo;++i) f.collect(i);
        Impact impact;impact.race=1;impact.humanInstance=4;impact.threat=1;
        Impact invalid=impact;invalid.humanInstance=5;CHECK(!f.r.consumeImpact(invalid).accepted);
        const Outcome first=f.r.consumeImpact(impact);CHECK(first.accepted);
        CHECK(first.changed==(cargo!=0));CHECK(f.r.securedCount()==(cargo?cargo-1:0));
        if(cargo) CHECK(first.slot==int(cargo)-1 && f.r.slots()[cargo-1].state==SlotState::Tethered);
        CHECK(!f.r.consumeImpact(impact).accepted);conservation(f.r);
        if(cargo==0)
        {f.collect(0);CHECK(!f.r.consumeImpact(impact).accepted);CHECK(f.r.securedCount()==1);}
    }
}

void impactDuringAnExistingTetherCannotDetachAgain()
{
    Fixture f;f.collect(0);f.collect(1);f.collect(2);f.impact(1);
    const Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);f.impact(2);
    CHECK(f.r.securedCount()==2);CHECK(f.r.snapshot(tether.id).instance==tether.instance);
    const Beam b=f.recoverPress();f.r.updateBeams({b});f.wait(72,{b});
    CHECK(f.r.securedCount()==3);Impact duplicate;duplicate.race=1;duplicate.humanInstance=4;duplicate.threat=2;
    CHECK(!f.r.consumeImpact(duplicate).accepted);CHECK(f.r.securedCount()==3);conservation(f.r);
}

void presentationStartsTheRecoveryWindowExactlyOnce()
{
    Fixture f;f.collect(0);f.impact();const Snapshot initial=f.r.snapshot(Runtime::TetherEntityId);
    CHECK(f.r.awaitingTetherPresentation());f.wait(1000);CHECK(f.r.occupiedCount()==1);
    CHECK(!f.r.confirmTetherPresented(initial.id,initial.instance+1,initial.phaseRevision));
    CHECK(f.r.confirmTetherPresented(initial.id,initial.instance,initial.phaseRevision));
    const Tick deadline=f.r.snapshot(initial.id).deadline;CHECK(deadline==f.r.tick()+360);
    f.wait(20);CHECK(f.r.confirmTetherPresented(initial.id,initial.instance,initial.phaseRevision));
    CHECK(f.r.snapshot(initial.id).deadline==deadline);CHECK(!f.r.awaitingTetherPresentation());
}

void recoveryWindowIsHalfOpenAtAllSupportedFrequencies()
{
    for(int fps:{60,120,144}) for(bool onDeadline:{false,true})
    {
        Fixture f(1,fps);f.collect(0);f.impact();
        Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);
        CHECK(f.r.confirmTetherPresented(tether.id,tether.instance,tether.phaseRevision));
        tether=f.r.snapshot(tether.id);
        const Tick start=tether.deadline-f.r.profile().recover-(onDeadline?0:1);
        f.wait(start-f.r.tick());f.r.updateBeams({f.release()});
        const Beam b=f.press(tether.id);f.r.updateBeams({b});f.wait(f.r.profile().recover,{b});
        CHECK(f.r.securedCount()==(onDeadline?0:1));
        CHECK(f.r.occupiedCount()==(onDeadline?0:1));conservation(f.r);
    }
}

void pausePreservesDeadlineAndRequiresRelease()
{
    Fixture f;f.collect(0);f.impact();Beam b=f.recoverPress();f.r.updateBeams({b});f.wait(30,{b});
    const Tick now=f.r.tick(),deadline=f.r.snapshot(b.entity).deadline;
    CHECK(f.r.advanceTo(now,false));for(int i=0;i<100;++i) CHECK(f.r.advanceTo(now,false));
    CHECK(f.r.tick()==now && f.r.snapshot(b.entity).deadline==deadline);
    CHECK(f.r.advanceTo(now,true));f.wait(100,{b});CHECK(f.r.securedCount()==0);
    const Beam release=f.release();Beam fresh=f.press(b.entity);fresh.releaseSerial=release.serial;
    f.r.updateBeams({fresh});f.wait(72,{fresh});CHECK(f.r.securedCount()==1);
}

void sourceTrackingLossDoesNotPauseTheWindow()
{
    Fixture f;f.collect(0);f.impact();const Beam b=f.recoverPress();f.r.updateBeams({b});
    const Tick deadline=f.r.snapshot(b.entity).deadline;Beam lost=b;lost.visible=false;
    f.wait(deadline-f.r.tick(),{lost});CHECK(f.r.occupiedCount()==0);
    CHECK(f.r.tick()==deadline);conservation(f.r);
}

void occurrenceAndConfirmationReceiptsCannotFarm()
{
    Fixture f;CHECK(f.r.skipOccurrence(0));CHECK(!f.r.skipOccurrence(0));
    CHECK(!f.r.activateCapsule(f.spec(0)).accepted);
    CHECK(f.r.skipConfirmation(0));CHECK(!f.r.crossConfirmation(0,0));
    CHECK(!f.r.crossConfirmation(1,0));CHECK(f.r.confirmationReceiptCount()==2);
    f.collect(1);CHECK(!f.r.crossConfirmation(1,0));CHECK(f.r.crossConfirmation(2,0));
    CHECK(!f.r.crossConfirmation(2,0));CHECK(f.r.slots()[0].beaconLit);
}

void beaconSurvivesLossWithoutBecomingAVictoryCondition()
{
    Fixture f;f.collect(0);CHECK(f.r.crossConfirmation(0,0));f.impact();
    const Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);
    CHECK(f.r.confirmTetherPresented(tether.id,tether.instance,tether.phaseRevision));
    f.wait(360);CHECK(f.r.slots()[0].state==SlotState::Empty);
    CHECK(f.r.slots()[0].everAcquired && f.r.slots()[0].beaconLit);
    f.collect(1);f.collect(2);f.collect(3);
    CHECK(!f.r.slots()[1].beaconLit && !f.r.slots()[2].beaconLit);
    CHECK(f.r.freezeNaturalFinish(1,25,1));CHECK(f.r.result().outcome=="mission_victory");
}

void crossingBeforeSameTickCollectionNeedsAFutureOccurrence()
{
    Fixture f;const Beam b=f.press(f.addOpen(0));f.r.updateBeams({b});f.wait(71,{b});
    CHECK(f.r.advanceTo(f.r.tick()+1,true));CHECK(!f.r.crossConfirmation(0,0));
    f.r.updateBeams({b});CHECK(f.r.securedCount()==1);CHECK(!f.r.slots()[0].beaconLit);
    CHECK(!f.r.crossConfirmation(0,0));CHECK(f.r.crossConfirmation(1,0));
}

void naturalFinishProducesFourImmutableOutcomes()
{
    for(bool first:{false,true}) for(bool full:{false,true})
    {
        Fixture f;const unsigned cargo=full?3:2;for(unsigned i=0;i<cargo;++i) f.collect(i);
        CHECK(f.r.freezeNaturalFinish(first?1:3,14.25,99));const Result result=f.r.result();
        CHECK(result.valid && result.natural && result.securedCount==cargo);
        CHECK(result.officialTeamRacePosition==(first?1:3));CHECK(result.nativeFinishTime==14.25);
        CHECK(result.finishSequence==99 && result.raceEpoch==1 && result.missionRevision==2);
        const std::string expected=first?(full?"mission_victory":"race_won_cargo_incomplete"):
                                                (full?"cargo_delivered_race_lost":"mission_failed");
        CHECK(result.outcome==expected);CHECK(!f.r.freezeNaturalFinish(1,1,100));
        CHECK(!f.r.abort("forced",101));CHECK(f.r.result().outcome==expected);
        CHECK(f.r.result().finishSequence==99);CHECK(f.r.slots()[0].module==result.slots[0].module);
    }
}

void tetherAtFinishIsIncompleteAndLateInputsCannotChangeIt()
{
    Fixture f;f.collect(0);f.collect(1);f.collect(2);f.impact();
    const Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);
    CHECK(f.r.freezeNaturalFinish(1,30,7));CHECK(f.r.result().securedCount==2);
    CHECK(f.r.result().outcome=="race_won_cargo_incomplete");
    CHECK(!f.r.confirmTetherPresented(tether.id,tether.instance,tether.phaseRevision));
    CHECK(!f.r.advanceTo(f.r.tick()+1,true));CHECK(!f.r.activateCapsule(f.spec(3)).accepted);
    CHECK(!f.r.crossConfirmation(0,0));CHECK(f.r.result().slots[2].state==SlotState::Tethered);
}

void expiryBeforeFinishPreventsSameTickRecovery()
{
    Fixture f;f.collect(0);f.collect(1);f.collect(2);f.impact();
    Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);
    CHECK(f.r.confirmTetherPresented(tether.id,tether.instance,tether.phaseRevision));
    const Tick deadline=f.r.snapshot(tether.id).deadline;
    f.wait(deadline-f.r.tick()-1);CHECK(f.r.advanceTo(deadline,true));
    CHECK(f.r.freezeNaturalFinish(1,10,1));CHECK(f.r.result().slots[2].state==SlotState::Empty);
    CHECK(f.r.result().securedCount==2);CHECK(f.r.result().outcome=="race_won_cargo_incomplete");
}

void artificialTerminationNeverDeclaresNaturalVictory()
{
    for(const char* reason:{"force_finish","early_finish","aborted","timeout","test_accelerated"})
    {
        Fixture f;f.collect(0);f.collect(1);f.collect(2);CHECK(f.r.abort(reason,1));
        CHECK(!f.r.result().natural && f.r.result().officialTeamRacePosition==0);
        CHECK(f.r.result().outcome=="not_completed" && f.r.result().securedCount==3);
        CHECK(!f.r.freezeNaturalFinish(1,10,2));
    }
    Fixture f;CHECK(!f.r.abort("natural_finish",1));
    CHECK(!f.r.freezeNaturalFinish(0,10,1));CHECK(!f.r.freezeNaturalFinish(5,10,1));
    CHECK(!f.r.freezeNaturalFinish(1,std::numeric_limits<double>::quiet_NaN(),1));
}

void sameRaceRepreparePreservesStateAndNewRaceResetsIt()
{
    Fixture f;f.collect(0);CHECK(f.r.crossConfirmation(0,0));f.impact();
    const Beam b=f.recoverPress();f.r.updateBeams({b});f.wait(30,{b});
    const Tick now=f.r.tick(),deadline=f.r.snapshot(b.entity).deadline;
    // Exact retry must preserve the in-progress transaction too.
    CHECK(f.r.prepare(1,2,3,4,{"A"},120));CHECK(f.r.snapshot(b.entity).owner==0);
    CHECK(f.r.prepare(1,2,4,4,{"B"},120));CHECK(!f.r.active());
    CHECK(f.r.slots()[0].beaconLit && f.r.slots()[0].state==SlotState::Tethered);
    CHECK(f.r.tick()==now && f.r.snapshot(b.entity).deadline==deadline);
    CHECK(f.r.snapshot(b.entity).owner==-1 && f.r.opportunityReceiptCount()==1);
    CHECK(f.r.setRequired(true));f.r.reset();CHECK(f.r.required() && !f.r.configured());
    CHECK(f.r.prepare(2,2,5,4,{"B"},120));CHECK(f.r.occupiedCount()==0);
    CHECK(!f.r.result().valid && f.r.opportunityReceiptCount()==0 && f.r.confirmationReceiptCount()==0);
    CHECK(!f.r.slots()[0].everAcquired && !f.r.slots()[0].beaconLit);
}

void newInputEpochCanRestartItsActionSerials()
{
    Fixture f;f.serial=100;const uint64_t first=f.addOpen(0);
    CHECK(f.r.prepare(1,2,4,4,{"A"},120));CHECK(f.r.advanceTo(f.r.tick(),true));f.serial=1;
    CHECK(f.r.expireCapsule(first,f.spec(0).instance));
    const uint64_t second=f.addOpen(1);CHECK(f.r.snapshot(second).phase==Phase::Open);
    // A release receipt in this new input epoch is required to rearm.
    const Beam release=f.release();Beam b=f.press(second);b.releaseSerial=release.serial;
    f.r.updateBeams({b});f.wait(72,{b});CHECK(f.r.securedCount()==1);
}

void maximumSessionHasBoundedReceiptsAndEntities()
{
    Fixture f(5);
    for(unsigned i=0;i<Profile::MaxOpportunities;++i)
    {
        const uint64_t id=f.addOpen(i);const Snapshot snapshot=f.r.snapshot(id);
        CHECK(f.r.expireCapsule(id,snapshot.instance));
        CHECK(!f.r.activateCapsule(f.spec(i)).accepted);CHECK(f.r.snapshots().empty());
    }
    CHECK(f.r.opportunityReceiptCount()==Profile::MaxOpportunities);
    CHECK(!f.r.skipOccurrence(Profile::MaxOpportunities));
    for(unsigned i=0;i<Profile::MaxConfirmations;++i) CHECK(f.r.skipConfirmation(i));
    CHECK(f.r.confirmationReceiptCount()==Profile::MaxConfirmations);
    CHECK(!f.r.skipConfirmation(Profile::MaxConfirmations));CHECK(f.r.feedback().size()==5);
    for(uint64_t threat=1;threat<=10000;++threat)
    {f.impact(threat);CHECK(f.r.occupiedCount()==0);}
}

void inactiveOrAggregatedTicksCannotAdvanceTheMission()
{
    Fixture f;f.collect(0);f.impact();const Snapshot tether=f.r.snapshot(Runtime::TetherEntityId);
    CHECK(f.r.confirmTetherPresented(tether.id,tether.instance,tether.phaseRevision));
    const Tick now=f.r.tick(),deadline=f.r.snapshot(tether.id).deadline;
    CHECK(!f.r.advanceTo(now+2,true));CHECK(!f.r.advanceTo(now+1,false));
    CHECK(!f.r.advanceTo(now-1,true));CHECK(f.r.tick()==now);
    CHECK(f.r.advanceTo(now,false));CHECK(f.r.snapshot(tether.id).deadline==deadline);
    CHECK(!f.r.activateCapsule(f.spec(1)).accepted);CHECK(!f.r.freezeNaturalFinish(1,10,1));
    conservation(f.r);
}
}

int main(int argc,char** argv)
{
    struct Case {const char* name;void(*run)();};
    const Case cases[]={
#define MISSION_CASE(name) {#name,name},
#include "scintilla_mission_cases.inc"
#undef MISSION_CASE
    };
    const bool selected=argc==3 && std::string(argv[1])=="--case";
    if(argc!=1 && !selected) return 2;
    bool found=false;
    for(const Case& c:cases) if(!selected || c.name==std::string(argv[2]))
    {found=true;std::cout<<c.name<<std::endl;c.run();}
    return found?0:2;
}
