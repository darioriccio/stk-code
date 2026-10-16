// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_combat.hpp"
#include <cstdlib>
#include <iostream>
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);} } while(false)
#include <cmath>
#include <limits>
#include <random>
#include <string>
using namespace ScintillaCombat;
namespace {
Shot shot(uint64_t serial,Tick tick,const std::string& who="B") {
    Shot s;s.race=1;s.serial=serial;s.inputEpoch=1;s.target=1;s.instance=11;s.admitted=tick;
    s.participant=who;s.event="event"+std::to_string(serial);s.historicalEligible=true;return s;
}
void activate(Runtime& r,unsigned crew) {
    std::vector<std::string> people={"B","C","A","D","E"};people.resize(crew);
    CHECK(r.prepare(1,1,people,120));r.observeTarget(1,11,true);r.update(0,true,{});
}
Beam opener() {Beam b;b.participant="A";b.target=1;b.instance=11;b.controlEpoch=1;b.held=b.visible=true;return b;}
void acquire(Runtime& r) {r.update(0,true,{opener()});for(int i=1;i<=71;i++)r.update(i,true,{opener()});
    CHECK(r.snapshot(1).phase==Phase::Acquiring);r.update(72,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Breached);}
Shot openShot(Runtime& r,uint64_t serial,Tick tick,const std::string& person) {
    Shot s=shot(serial,tick,person);s.window=r.snapshot(1).window;s.historicalOpen=s.admittedOpen=true;return s;
}
void soloFiveAndRecovery() {
    Runtime r;activate(r,1);Outcome out;
    for(int i=0;i<5;i++){r.update(i*30,true,{});out=r.shoot(shot(i+1,i*30));CHECK(out.contributed);CHECK(out.requestEffect==(i==4));}
    CHECK(r.confirmEffect(out.effect,true));CHECK(!r.shoot(shot(6,120)).requestEffect);
    r.update(240,true,{});r.animationFinished(1,11);CHECK(r.snapshot(1).phase==Phase::Recovery);
    r.update(719,true,{});CHECK(r.snapshot(1).phase==Phase::Recovery);r.update(720,true,{});CHECK(r.snapshot(1).phase==Phase::Shielded);
}
void soloDeadlineAndMissIsolation() {
    Runtime r;activate(r,1);
    for(int i=0;i<4;i++){r.update(i*240,true,{});CHECK(r.shoot(shot(i+1,i*240)).contributed);}
    r.update(960,true,{});CHECK(!r.shoot(shot(5,959)).contributed);CHECK(r.snapshot(1).count==0);
    CHECK(r.shoot(shot(6,960)).contributed);CHECK(r.snapshot(1).count==1);
    r.observeTarget(2,22,true);Shot other=shot(7,960);other.target=2;other.instance=22;CHECK(r.shoot(other).contributed);
    CHECK(r.snapshot(1).count==1&&r.snapshot(2).count==1);
}
void distinctShootersAndBoundaries() {
    Runtime r;activate(r,3);acquire(r);
    r.update(172,true,{opener()});CHECK(r.shoot(openShot(r,1,172,"B")).contributed);
    CHECK(!r.shoot(openShot(r,2,172,"B")).contributed);CHECK(!r.shoot(openShot(r,3,172,"A")).contributed);
    r.update(321,true,{opener()});auto result=r.shoot(openShot(r,4,321,"C"));CHECK(result.requestEffect);
    CHECK(r.confirmEffect(result.effect,true));CHECK(!r.confirmEffect(result.effect,true));
    Runtime late;activate(late,3);acquire(late);late.update(172,true,{opener()});late.shoot(openShot(late,1,172,"B"));
    auto pending=openShot(late,2,322,"C");late.update(322,true,{opener()});CHECK(!late.shoot(pending).requestEffect);
}
void releaseAndReprepareDoNotResurrect() {
    Runtime r;activate(r,3);acquire(r);auto s=openShot(r,1,72,"B");
    r.update(73,true,{});CHECK(!r.shoot(s).contributed);r.update(74,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Acquiring);
    for(int i=75;i<=146;i++)r.update(i,true,{opener()});
    auto a=r.shoot(openShot(r,2,146,"B"));CHECK(a.contributed);
    auto b=r.shoot(openShot(r,3,146,"C"));CHECK(b.requestEffect);r.confirmEffect(b.effect,true);
    r.suspendInput();CHECK(r.snapshot(1).phase==Phase::Stagger);
    CHECK(r.prepare(1,2,{"B","C","A"},120));CHECK(r.snapshot(1).phase==Phase::Stagger);
    CHECK(!r.shoot(s).requestEffect);
}
ToolSnapshot toolState(const Runtime& r,const std::string& person) {
    for(const auto& s:r.toolSnapshots())if(s.participant==person)return s;
    CHECK(false);return ToolSnapshot();
}
void toolReleaseFeedbackPersistsAcrossEveryLockClosure() {
    for(unsigned cause=0;cause<5;++cause){
        Runtime r;activate(r,3);acquire(r);Tick now=73;
        if(cause==0){now=492;r.update(now,true,{opener()});} // Expiry while still held.
        if(cause==1){auto lost=opener();lost.visible=false;r.update(now,true,{lost});}
        if(cause==2){CHECK(r.shoot(openShot(r,1,72,"B")).contributed);
            auto effect=r.shoot(openShot(r,2,72,"C"));CHECK(effect.requestEffect);CHECK(r.confirmEffect(effect.effect,true));}
        if(cause==3)r.observeTarget(1,11,false);
        if(cause==4)r.retireTarget(1);
        auto feedback=toolState(r,"A");CHECK(feedback.status==ToolStatus::ReleaseRequired&&feedback.controlEpoch==1&&feedback.target==1);
        r.update(now+1,true,{opener()});CHECK(toolState(r,"A").status==ToolStatus::ReleaseRequired);
        auto released=opener();released.held=released.visible=false;released.controlEpoch=2;
        r.update(now+2,true,{released});CHECK(toolState(r,"A").status==ToolStatus::None);
    }
}
void toolBusyFeedbackUsesTheActualOwnerAndNeverStealsALock() {
    for(bool reversed:{false,true}){
        Runtime r;activate(r,3);auto b=opener();b.participant="B";b.controlEpoch=9;
        const std::vector<Beam> beams=reversed?std::vector<Beam>{opener(),b}:std::vector<Beam>{b,opener()};
        // Canonical crew order admits B first regardless of delivery ordering.
        r.update(0,true,beams);CHECK(r.snapshot(1).opener==0);
        auto busy=toolState(r,"A");CHECK(busy.status==ToolStatus::TargetBusy&&busy.owner==0&&busy.controlEpoch==1&&busy.target==1);
        CHECK(toolState(r,"B").status==ToolStatus::None);
        for(int tick=1;tick<=72;++tick)r.update(tick,true,beams);
        CHECK(r.snapshot(1).phase==Phase::Breached&&toolState(r,"A").status==ToolStatus::TargetBusy);
        b.visible=false;r.update(73,true,{opener(),b});
        CHECK(r.snapshot(1).opener==2&&r.snapshot(1).phase==Phase::Acquiring);
        CHECK(toolState(r,"A").status==ToolStatus::None&&toolState(r,"B").status==ToolStatus::ReleaseRequired);
    }
}
void toolFeedbackClearsForSuspendReprepareResetAndSolo() {
    Runtime r;activate(r,3);acquire(r);r.update(492,true,{opener()});
    CHECK(toolState(r,"A").status==ToolStatus::ReleaseRequired);
    r.update(492,false,{});for(const auto& t:r.toolSnapshots())CHECK(t.status==ToolStatus::None);
    CHECK(r.prepare(1,2,{"B","C","A"},120));for(const auto& t:r.toolSnapshots())CHECK(t.status==ToolStatus::None);
    r.reset();CHECK(r.toolSnapshots().empty());
    activate(r,1);r.update(1,true,{opener()});for(const auto& t:r.toolSnapshots())CHECK(t.status==ToolStatus::None);
}
void toolFeedbackReconcilesCompletionBeforeTheNextTick() {
    Runtime r;activate(r,3);acquire(r);auto b=opener();b.participant="B";b.controlEpoch=2;
    r.update(73,true,{opener(),b});CHECK(toolState(r,"B").status==ToolStatus::TargetBusy);
    CHECK(r.shoot(openShot(r,1,73,"B")).contributed);
    const auto out=r.shoot(openShot(r,2,73,"C"));CHECK(out.requestEffect);
    // The adapter publishes after draining shots: no stale busy or silent block.
    CHECK(toolState(r,"A").status==ToolStatus::ReleaseRequired);
    CHECK(toolState(r,"B").status==ToolStatus::None);
    CHECK(r.confirmEffect(out.effect,false));CHECK(toolState(r,"A").status==ToolStatus::ReleaseRequired);
}
void geometryAndCurve() {
    Profile p;CHECK(Profile::create(120,&p));CHECK(p.acquire==72&&p.breach==420&&p.cooperation==150&&p.recovery==480);
    CHECK(Profile::inRange(20*20,0.35));CHECK(!Profile::inRange(30*30,0.35));CHECK(!Profile::inRange(0,0.35));
    CHECK(Profile::staggerCurve(0)==0&&Profile::staggerCurve(1)==0&&Profile::staggerCurve(0.5)==1);
    CHECK(std::abs(Profile::staggerCurve(0.25)-0.5625)<1e-12);
}
void rejectsRosterDuplicatesAndSustainedLoad() {
    Runtime invalid;CHECK(!invalid.prepare(1,1,{"same","same"},120));
    Runtime r;activate(r,1);
    for(unsigned i=1;i<=100000;i++){r.update(i*1000,true,{});CHECK(r.shoot(shot(i,i*1000)).contributed);CHECK(r.snapshot(1).count==1);}
    CHECK(r.snapshots().size()==1);
}
void twoCrewNeedsOneNonOpener() {
    Runtime r;activate(r,2);Beam b=opener();b.participant="B";
    r.update(0,true,{b});for(int tick=1;tick<=72;++tick)r.update(tick,true,{b});
    CHECK(r.snapshot(1).required==1&&r.snapshot(1).phase==Phase::Breached);
    CHECK(!r.shoot(openShot(r,1,72,"B")).contributed);
    CHECK(r.shoot(openShot(r,2,72,"C")).requestEffect);
}
void noPrebankAndNoMixedWindows() {
    Runtime r;activate(r,3);auto closed=shot(1,0);closed.historicalOpen=false;
    CHECK(!r.shoot(closed).contributed);acquire(r);CHECK(r.snapshot(1).count==0);
    auto stale=openShot(r,2,72,"B");++stale.window;CHECK(!r.shoot(stale).contributed);
    auto historical=openShot(r,3,72,"B");historical.historicalEligible=false;CHECK(!r.shoot(historical).contributed);
    CHECK(r.snapshot(1).count==0);
}
void repeatedEventAndSameTickCompleteOnce() {
    Runtime r;activate(r,3);acquire(r);
    auto b=openShot(r,1,72,"B");CHECK(r.shoot(b).contributed);
    auto duplicate=openShot(r,2,72,"C");duplicate.event=b.event;CHECK(!r.shoot(duplicate).contributed);
    auto result=r.shoot(openShot(r,3,72,"C"));CHECK(result.requestEffect);
    CHECK(!r.shoot(openShot(r,4,72,"C")).requestEffect);
    CHECK(r.confirmEffect(result.effect,false));CHECK(r.snapshot(1).phase==Phase::Shielded);
    CHECK(!r.confirmEffect(result.effect,true));
}
void breachDeadlineAndReleaseLatch() {
    Runtime r;activate(r,3);acquire(r);const auto old=r.snapshot(1).window;
    r.update(491,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Breached);
    r.update(492,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Shielded);
    r.update(493,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Shielded);
    r.update(494,true,{});Beam b=opener();b.controlEpoch=2;r.update(495,true,{b});
    CHECK(r.snapshot(1).phase==Phase::Acquiring&&r.snapshot(1).window!=old);
}
void absentAndIncompatibleTargetCannotContribute() {
    Runtime r;activate(r,3);acquire(r);auto s=openShot(r,1,72,"B");
    r.observeTarget(1,11,false);CHECK(r.snapshot(1).phase==Phase::Shielded);CHECK(!r.shoot(s).contributed);
    r.retireTarget(1);r.observeTarget(1,12,true);CHECK(!r.shoot(s).contributed);
    CHECK(r.snapshot(1).count==0&&r.snapshot(1).instance==12);
}
void lossDoesNotReduceCrewAndTargetsDoNotMerge() {
    Runtime r;activate(r,3);acquire(r);CHECK(r.shoot(openShot(r,1,72,"B")).contributed);
    r.observeTarget(2,22,true);auto other=openShot(r,2,72,"C");other.target=2;other.instance=22;
    CHECK(!r.shoot(other).requestEffect&&r.snapshot(1).count==1);
    Beam lost=opener();lost.visible=false;r.update(73,true,{lost});
    CHECK(r.snapshot(1).phase==Phase::Shielded&&r.snapshot(1).count==0&&r.snapshot(1).required==2);
    r.update(74,true,{opener()});CHECK(r.snapshot(1).phase==Phase::Shielded);
}
void cannotBackfillAcquisitionAcrossUpdateGap() {
    Runtime r;activate(r,3);r.update(0,true,{opener()});r.update(72,true,{opener()});
    CHECK(r.snapshot(1).phase==Phase::Acquiring&&r.snapshot(1).progress==0);
    for(int tick=73;tick<=144;++tick)r.update(tick,true,{opener()});
    CHECK(r.snapshot(1).phase==Phase::Breached);
}
void pauseAndGrantChangesPreserveRecovery() {
    Runtime r;activate(r,1);Outcome effect;
    for(int i=0;i<5;i++){r.update(i*30,true,{});effect=r.shoot(shot(i+1,i*30));}
    CHECK(r.confirmEffect(effect.effect,true));r.update(240,true,{});r.animationFinished(1,11);
    const auto deadline=r.snapshot(1).deadline;
    for(int i=0;i<100;++i)r.update(240,false,{});
    CHECK(r.snapshot(1).deadline==deadline&&r.snapshot(1).phase==Phase::Recovery);
    CHECK(r.prepare(1,2,{"B"},120));CHECK(r.snapshot(1).deadline==deadline);
    CHECK(r.prepare(1,2,{"B"},120));CHECK(r.snapshot(1).deadline==deadline);
    r.update(deadline-1,true,{});CHECK(r.snapshot(1).phase==Phase::Recovery);
    r.update(deadline,false,{});CHECK(r.snapshot(1).phase==Phase::Shielded);
}
void boundedTargetsAndReset() {
    Runtime r;activate(r,1);for(unsigned id=1;id<=100;++id)r.observeTarget(id,id*11,true);
    CHECK(r.snapshots().size()==32);r.reset();CHECK(r.snapshots().empty()&&!r.configured());
    CHECK(r.prepare(2,3,{"new-crew"},144));CHECK(r.profile().acquire==87);
}
void requiredIntentBlocksEveryUnpreparedStart() {
    Runtime r;CHECK(!r.required()&&!r.configured()&&r.permitsStart(false)&&r.intentRevision()==0);
    CHECK(r.setRequired(true));CHECK(r.required()&&!r.configured());
    CHECK(r.intentRevision()==1);CHECK(r.setRequired(true)&&r.intentRevision()==2);
    CHECK(!r.permitsStart(false)&&!r.permitsStart(true));
    CHECK(!r.prepare(1,1,{"same","same"},120));CHECK(!r.permitsStart(true));
    CHECK(r.prepare(1,1,{"B"},120));
    CHECK(!r.permitsStart(false)&&r.permitsStart(true));
    CHECK(!r.setRequired(false)&&r.required()&&r.intentRevision()==2);
    r.suspendInput();CHECK(r.required()&&!r.permitsStart(false));
}
void worldResetPreservesIntentWithoutPreservingReadiness() {
    Runtime r;activate(r,1);CHECK(r.required()&&r.permitsStart(true));
    // A producer reset (including Ready from results) must need preparation again.
    r.reset(true);CHECK(r.required()&&!r.configured()&&!r.permitsStart(true)&&r.intentRevision()==0);
    CHECK(r.snapshots().empty());
    CHECK(r.setRequired(true)&&!r.permitsStart(true));
    CHECK(r.setRequired(false)&&r.permitsStart(false));
    CHECK(r.setRequired(true));r.reset(true);CHECK(!r.permitsStart(true));
    r.reset();CHECK(!r.required()&&r.permitsStart(false));
}
void staggerEnvelopeIncludesTheShiftedChassisBelowACeiling() {
    const double lo[3]={-0.5,0,-1},hi[3]={0.5,1,1};StaggerEnvelope e;
    CHECK(StaggerEnvelope::create(lo,hi,0.25,8.0*3.14159265358979323846/180.0,&e));
    const double ceiling=1.15;
    const double oldTop=0.5+std::sin(8.0*3.14159265358979323846/180.0)+0.02+0.25;
    CHECK(oldTop<ceiling); // The original centered-at-origin test missed it.
    CHECK(e.center[1]+e.halfExtents[1]<ceiling);
    CHECK(e.center[1]+e.halfExtents[1]+0.25>ceiling); // Sweep must encounter the roof.
    CHECK(std::cos(e.pitch)+std::sin(e.pitch)+0.25>ceiling);
    CHECK(std::abs(e.radius-std::sqrt(2.0))<1e-12);
}
void staggerEnvelopeContainsAllOffsetBoundsAndPitchPoses() {
    std::mt19937 random(1267);std::uniform_real_distribution<double> shift(-4,4),size(0.05,3);
    for(unsigned fixture=0;fixture<100;++fixture){
        double lo[3],hi[3];for(unsigned axis=0;axis<3;++axis){lo[axis]=shift(random);hi[axis]=lo[axis]+size(random);}
        StaggerEnvelope e;CHECK(StaggerEnvelope::create(lo,hi,0.25,0.13962634015954636,&e));
        for(unsigned mask=0;mask<8;++mask)for(unsigned sample=0;sample<=100;++sample){
            const double x=(mask&1)?hi[0]:lo[0],y=(mask&2)?hi[1]:lo[1],z=(mask&4)?hi[2]:lo[2];
            const double a=e.pitch*sample/100.0,c=std::cos(a),s=std::sin(a);
            const double p[3]={x,y*c-z*s,y*s+z*c};
            for(unsigned axis=0;axis<3;++axis)CHECK(std::abs(p[axis]-e.center[axis])<=e.halfExtents[axis]);
        }
    }
}
void staggerMotionLeavesOnlyProvenSupportingFaces() {
    CHECK(StaggerEnvelope::supportingFace(1,0,0));
    CHECK(StaggerEnvelope::supportingFace(0.500001,-0.01,0));
    CHECK(!StaggerEnvelope::supportingFace(0.5,0,0));
    CHECK(!StaggerEnvelope::supportingFace(1,0.001,0)); // An upward ledge is not the ground.
    CHECK(!StaggerEnvelope::supportingFace(-1,-1,0));
    CHECK(!StaggerEnvelope::supportingFace(std::numeric_limits<double>::quiet_NaN(),0,0));
    const double lo[3]={-0.5,-0.2,-2},hi[3]={0.5,1.8,1};StaggerEnvelope e;
    CHECK(StaggerEnvelope::create(lo,hi,0.25,0.13962634015954636,&e));
    for(double alignment:{0.500001,0.75,1.0})for(unsigned mask=0;mask<4;++mask){
        const double y=(mask&1)?hi[1]:lo[1],z=(mask&2)?hi[2]:lo[2];
        for(unsigned sample=0;sample<=100;++sample){
            const double w=Profile::staggerCurve(sample/100.0),a=e.pitch*w;
            const double dy=y*(std::cos(a)-1)-z*std::sin(a)+0.25*w;
            const double dz=y*std::sin(a)+z*(std::cos(a)-1);
            // Worst supporting normal at this alignment, in the pitch plane.
            CHECK(alignment*dy-std::sqrt(1-alignment*alignment)*std::abs(dz)>=-1e-12);
        }
    }
}
void staggerEnvelopeRejectsInvalidBoundsWithoutPublishingGeometry() {
    double lo[3]={-0.5,0,-1},hi[3]={0.5,1,1};StaggerEnvelope e;e.pitch=0.123;
    CHECK(!StaggerEnvelope::create(nullptr,hi,0.25,0.1,&e));
    CHECK(!StaggerEnvelope::create(lo,hi,0.25,0.1,nullptr));
    CHECK(!StaggerEnvelope::create(lo,hi,0,0.1,&e));
    CHECK(!StaggerEnvelope::create(lo,hi,0.25,2,&e));
    lo[1]=hi[1];CHECK(!StaggerEnvelope::create(lo,hi,0.25,0.1,&e));
    lo[1]=2;CHECK(!StaggerEnvelope::create(lo,hi,0.25,0.1,&e));
    lo[1]=std::numeric_limits<double>::quiet_NaN();CHECK(!StaggerEnvelope::create(lo,hi,0.25,0.1,&e));
    lo[1]=-std::numeric_limits<double>::infinity();CHECK(!StaggerEnvelope::create(lo,hi,0.25,0.1,&e));
    CHECK(e.pitch==0.123);
}

}
int main(int argc,char** argv){
    struct Case{const char* name;void(*run)();};
    const Case cases[]={
#define COMBAT_CASE(name) {#name,name},
#include "scintilla_combat_cases.inc"
#undef COMBAT_CASE
    };
    const bool selected=argc==3&&std::string(argv[1])=="--case";
    if(argc!=1&&!selected)return 2;
    bool found=false;
    for(const auto& c:cases)if(!selected||c.name==std::string(argv[2])){found=true;std::cout<<c.name<<std::endl;c.run();}
    return found?0:2;
}
