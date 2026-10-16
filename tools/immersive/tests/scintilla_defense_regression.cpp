// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_defense.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);}}while(false)
using namespace ScintillaDefense;
namespace {
Observation observation(){Observation o;o.geometryValid=o.humanEligible=o.videoContinuous=true;return o;}
void activate(Runtime&r){CHECK(r.prepare(1,1,{"A","B"},120));r.update(0,1000,true,observation(),{});}
void warning(Runtime&r){Candidate c;c.id=2;c.instance=3;c.humanInstance=4;c.sector=Sector::Rear;CHECK(r.begin(c,1000));}
void telegraph(Runtime&r){warning(r);auto o=observation();o.presentedThreat=r.view().threat;o.presentedPhase=Phase::AwaitWarning;o.presentedLow=1000;o.presentedHigh=2000;r.update(0,2000,true,o,{});CHECK(r.view().phase==Phase::Telegraph);}
Beam beam(const Runtime&r,uint64_t epoch=2){Beam b;b.participant="A";b.epoch=epoch;b.context=Context::DefenseSector;b.sector=Sector::Rear;b.threat=r.view().threat;b.held=b.visible=true;return b;}
void profileBudgets(){for(int f:{60,120,144})for(unsigned c=1;c<=5;++c){Profile p;CHECK(Profile::create(f,c,&p));CHECK(p.reserve==int64_t(c==1?70:60)*p.q);CHECK(p.hold*1000000>=int64_t(600000)*f);}Profile p;CHECK(!Profile::create(0,2,&p));CHECK(!Profile::create(120,0,&p));}
void warningPresentationBoundary(){Runtime r;activate(r);warning(r);auto o=observation();o.presentedThreat=r.view().threat;o.presentedPhase=Phase::AwaitWarning;o.presentedHigh=2001;r.update(1,2000,true,o,{});CHECK(r.view().phase==Phase::AwaitWarning);r.update(2,2001,true,o,{});CHECK(r.deadline()==302);}
void completeDefense(){Runtime r;activate(r);telegraph(r);const Beam b=beam(r);for(int i=1;i<=300;++i)r.update(i,2000+i*8334,true,observation(),{b});CHECK(r.reason()=="blocked");CHECK(!r.readyForCandidate());CHECK(r.energy()>44&&r.energy()<46);}
void releaseAtDeadlineWins(){Runtime r;activate(r);telegraph(r);const Beam b=beam(r);for(int i=1;i<300;++i)r.update(i,2000+i*8334,true,observation(),{b});r.update(300,2502000,true,observation(),{});CHECK(r.view().phase==Phase::LaunchPending);}
void insufficientHold(){Runtime r;activate(r);telegraph(r);for(int i=1;i<230;++i)r.update(i,2000+i*8334,true,observation(),{});for(int i=230;i<=300;++i)r.update(i,2000+i*8334,true,observation(),{beam(r)});CHECK(r.view().phase==Phase::LaunchPending);}
void invalidProofLatches(){Runtime r;activate(r);telegraph(r);auto b=beam(r);r.update(1,11000,true,observation(),{b});b.visible=false;r.update(2,20000,true,observation(),{b});b.visible=true;r.update(3,29000,true,observation(),{b});CHECK(r.feedback()[0].status==Status::ReleaseRequired);b.epoch=4;r.update(4,38000,true,observation(),{b});CHECK(r.feedback()[0].status==Status::Reinforcing);b.epoch=2;r.update(5,47000,true,observation(),{b});CHECK(r.feedback()[0].epoch==4);}
void ownerDoesNotMultiply(){Runtime r;activate(r);telegraph(r);auto a=beam(r),b=a;b.participant="B";r.update(1,11000,true,observation(),{b,a});CHECK(r.feedback()[1].status==Status::Busy);CHECK(std::abs(r.energy()-95)<1e-9);r.update(2,20000,true,observation(),{b});CHECK(r.feedback()[1].status==Status::Busy);}
void preserveAcrossReprepare(){Runtime r;activate(r);telegraph(r);r.update(1,11000,true,observation(),{beam(r)});r.update(2,20000,true,observation(),{beam(r)});const double e=r.energy(),h=r.heat();r.suspend("pause");CHECK(r.prepare(1,2,{"B","A"},120));CHECK(r.energy()==e&&r.heat()==h);CHECK(r.required());CHECK(!r.setRequired(false));r.reset(true);CHECK(r.required()&&!r.configured());CHECK(r.prepare(2,3,{"A"},120));CHECK(r.energy()==100&&r.heat()==0);}
void timeoutAndVideoGap(){Runtime r;activate(r);warning(r);r.update(1,501001,true,observation(),{});CHECK(r.reason()=="presentation_timeout");Runtime q;activate(q);telegraph(q);auto o=observation();o.videoContinuous=false;q.update(1,11000,true,o,{});CHECK(q.reason()=="presentation_gap");}
void ballistics(){Ballistic b;CHECK(Ballistic::solve(Vec(0,0,20),Vec(10,0,0),120,&b));CHECK(std::abs(b.seconds-.5163977794943222)<1e-10);CHECK(std::abs(b.direction.x-.25)<1e-10);CHECK(!Ballistic::solve(Vec(0,0,20),Vec(0,0,40),120,&b));CHECK(!Ballistic::solve(Vec(0,0,20),Vec(0,0,-40),120,&b));CHECK(!Ballistic::solve(Vec(NAN,0,20),Vec(),120,&b));}
void sectorsAndLayout(){Vec f(0,0,1),u(0,1,0);auto d=[](double x){return Vec(std::sin(x*3.141592653589793/180),0,std::cos(x*3.141592653589793/180));};CHECK(Runtime::sector(d(39),f,u)==Sector::Front);CHECK(Runtime::sector(d(44),f,u)==Sector::None);CHECK(Runtime::sector(d(49),f,u,Sector::Front)==Sector::Front);CHECK(Runtime::sector(d(51),f,u,Sector::Front)==Sector::None);for(unsigned w:{1024u,2048u}){auto l=Layout::create(w,1024);CHECK(l.valid());CHECK(l.classify(.5,1).context==Context::WorldOffense);double x,y;Vec ray=l.rearDirection(.5,.5);CHECK(l.rearUV(ray,&x,&y));CHECK(l.classify(x,y).context==Context::ReadOnlyRear);const double s=1024;CHECK(l.classify(.5,.5+.85*s/(2*1024)).context==Context::ServiceBoost);}}
void impactIdempotency(){Runtime r;activate(r);telegraph(r);for(int i=1;i<=300;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(r.launch(Vec(),Vec(0,0,40)));r.contact(true,Vec(0,0,1),2600000);auto o=observation();o.presentedThreat=r.view().threat;o.presentedContact=r.view().contact;o.presentedPhase=Phase::AwaitImpact;o.presentedLow=2600000;o.presentedHigh=2610000;r.update(301,2610000,true,o,{});CHECK(r.wantsImpact());CHECK(r.confirmImpact(true));CHECK(!r.confirmImpact(true));CHECK(r.protectionUntil()==901);}

void newEpochNeverInheritsContinuity(){Runtime r;activate(r);telegraph(r);for(int i=1;i<299;++i)r.update(i,2000+i*8334,true,observation(),{beam(r)});const double e=r.energy();r.update(299,2500000,true,observation(),{beam(r,4)});CHECK(r.energy()<e-4.9);r.update(300,2502000,true,observation(),{beam(r,4)});CHECK(r.view().phase==Phase::LaunchPending);}
void releaseBeforeOtherAdmission(){Runtime r;CHECK(r.prepare(1,1,{"B","A"},120));r.update(0,1000,true,observation(),{});telegraph(r);auto a=beam(r);a.admitted=1;r.update(1,11000,true,observation(),{a});CHECK(r.owner()==1);auto b=a;b.participant="B";b.admitted=2;r.update(2,20000,true,observation(),{b});CHECK(r.owner()==0);CHECK(r.feedback()[0].status==Status::Reinforcing);}
void admissionOrderAndStaleSerial(){Runtime r;activate(r);telegraph(r);auto a=beam(r),b=a;a.admitted=2;b.admitted=1;b.participant="B";r.update(2,20000,true,observation(),{a,b});CHECK(r.owner()==1);CHECK(r.feedback()[0].status==Status::Busy);b.serial=2;r.update(3,29000,true,observation(),{b});b.serial=1;b.visible=false;r.update(4,38000,true,observation(),{b});CHECK(r.owner()==1);}
void standbyPreservesReserveAndLatches(){Runtime r;CHECK(r.prepare(1,1,{"A"},120));r.update(0,1000,true,observation(),{});auto b=beam(r);b.threat=0;r.update(0,1000,true,observation(),{b});for(int i=1;i<=3000;++i)r.update(i,1000+i*8334,true,observation(),{b});CHECK(r.energy()>=70);CHECK(r.heat()<=18.75);CHECK(r.owner()<0);CHECK(r.feedback()[0].status==Status::ReserveProtected);CHECK(!r.wantsBoost());}
void fixedPointSegmentedStandby(){for(int f:{60,120,144}){Runtime a,b;CHECK(a.prepare(1,1,{"A","B"},f));CHECK(b.prepare(1,1,{"A","B"},f));auto x=beam(a);x.threat=0;a.update(0,1000,true,observation(),{x});b.update(0,1000,true,observation(),{x});for(int i=1;i<=2*f;++i)a.update(i,1000+int64_t(i)*1000000/f,true,observation(),{x});b.update(2*f,2001000,true,observation(),{x});CHECK(a.energy()==b.energy()&&a.heat()==b.heat());CHECK(std::abs(a.energy()-91)<1e-9&&std::abs(a.heat()-4)<1e-9);}}
void boostCostCooldownAndRearm(){Runtime r;activate(r);auto b=beam(r);b.context=Context::ServiceBoost;b.sector=Sector::None;b.threat=0;r.update(0,1000,true,observation(),{b});for(int i=1;i<=60;++i)r.update(i,1000+i*8334,true,observation(),{b});CHECK(r.wantsBoost());CHECK(r.canApplyBoost());CHECK(r.confirmBoost(true));CHECK(!r.confirmBoost(true));CHECK(r.energy()==75);CHECK(r.boostUntil()==780);const auto until=r.boostUntil();CHECK(r.prepare(1,2,{"A","B"},120));CHECK(r.boostUntil()==until&&r.energy()==75);r.update(60,501100,true,observation(),{b});CHECK(!r.wantsBoost());for(int i=61;i<=900;++i)r.update(i,1000+i*8334,true,observation(),{b});CHECK(!r.wantsBoost());b.epoch=4;for(int i=901;i<=961;++i)r.update(i,1000+i*8334,true,observation(),{b});CHECK(r.wantsBoost());const auto e=r.energy();CHECK(r.confirmBoost(false));CHECK(r.energy()==e&&r.boostUntil()==until);}
void exactBoostReserveBoundary(){Runtime r;activate(r);auto b=beam(r);b.threat=0;for(int i=0;i<3;++i){b.epoch=2+i*2;r.update(0,1000,true,observation(),{b});r.update(0,1000,true,observation(),{});}CHECK(r.energy()==85);CHECK(r.boostStatus(true)==Status::BoostReady);b.epoch=8;r.update(0,1000,true,observation(),{b});CHECK(r.energy()==80);r.update(0,1000,true,observation(),{});CHECK(r.boostStatus(true)==Status::ReserveProtected);}


void twoAttackersRespectCooldownAndCrewRearm(){Runtime r;activate(r);telegraph(r);r.update(1,11000,true,observation(),{beam(r)});r.cancel("source_occluded");for(int i=2;i<=961;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(!r.candidateReady(2,3));CHECK(r.candidateReady(3,5));Candidate other;other.id=3;other.instance=5;other.humanInstance=4;other.sector=Sector::Front;const int64_t now=2000+961*8334;CHECK(r.begin(other,now));auto o=observation();o.presentedThreat=r.view().threat;o.presentedPhase=Phase::AwaitWarning;o.presentedLow=now;o.presentedHigh=now+1;r.update(961,now+1,true,o,{});auto b=beam(r);b.participant="B";b.sector=Sector::Front;b.admitted=962;r.update(962,now+8334,true,observation(),{b});CHECK(r.owner()==1);auto a=b;a.participant="A";a.epoch=4;a.admitted=963;r.update(963,now+16668,true,observation(),{a,b});CHECK(r.feedback()[0].status==Status::Busy);CHECK(r.statistics().cancelled==1);}

void explicitVisibilityLossCancelsPendingLaunch(){Runtime r;activate(r);telegraph(r);for(int i=1;i<=300;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(r.view().phase==Phase::LaunchPending);auto o=observation();o.visibilityLost=true;o.presentedThreat=r.view().threat;o.presentedPhase=Phase::LaunchPending;o.presentedHigh=2600000;r.update(301,2600000,true,o,{});CHECK(r.reason()=="source_visibility_lost"&&r.view().phase==Phase::Cooldown);CHECK(!r.launch(Vec(),Vec(0,0,40)));}
// Tuned policies. Defaults stay covered by every case above.
void tunedActivate(Runtime&r,const ScintillaTuning::Defense&t){CHECK(r.prepare(1,1,{"A","B"},120,t));r.update(0,1000,true,observation(),{});}
void pausePolicyFreezesOnlyTheWarning(){ScintillaTuning::Defense t;t.boostPolicy=ScintillaTuning::BoostPolicy::Pause;t.maxPauseS=1.5;Runtime r;tunedActivate(r,t);telegraph(r);CHECK(r.deadline()==300);auto o=observation();o.humanTransient=true;for(int i=1;i<=60;++i)r.update(i,2000+i*8334,true,o,{});CHECK(r.deadline()==360&&r.view().phase==Phase::Telegraph);for(int i=61;i<360;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(r.view().phase==Phase::Telegraph);r.update(360,2000+360*8334,true,observation(),{});CHECK(r.view().phase==Phase::LaunchPending);
    Runtime q;tunedActivate(q,t);telegraph(q);for(int i=1;i<=180;++i)q.update(i,2000+i*8334,true,o,{});CHECK(q.view().phase==Phase::Telegraph);q.update(181,2000+181*8334,true,o,{});CHECK(q.reason()=="pause_exceeded");
    Runtime d;activate(d);telegraph(d);for(int i=1;i<=60;++i)d.update(i,2000+i*8334,true,o,{});CHECK(d.deadline()==300);}
void technicalRetryShortensOnlyTechnicalCancels(){ScintillaTuning::Defense t;t.technicalRetryS=2;Runtime r;tunedActivate(r,t);telegraph(r);r.update(1,11000,true,observation(),{});r.cancel("opportunity_lost");CHECK(r.nextAttack()==241);for(int i=2;i<241;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(!r.candidateReady(2,3));r.update(241,2000+241*8334,true,observation(),{});CHECK(r.candidateReady(2,3));
    Runtime b;tunedActivate(b,t);telegraph(b);const Beam held=beam(b);for(int i=1;i<=300;++i)b.update(i,2000+i*8334,true,observation(),{held});CHECK(b.reason()=="blocked"&&b.nextAttack()==1260);
    Runtime d;activate(d);telegraph(d);d.update(1,11000,true,observation(),{});d.cancel("opportunity_lost");CHECK(d.nextAttack()==961);}
void inheritedVelocityLaunchAndTunedBallistics(){Runtime r;activate(r);telegraph(r);for(int i=1;i<=300;++i)r.update(i,2000+i*8334,true,observation(),{});CHECK(!r.launch(Vec(),Vec(0,0,60)));
    ScintillaTuning::Defense t;t.inheritAttackerVelocity=true;Runtime q;tunedActivate(q,t);telegraph(q);for(int i=1;i<=300;++i)q.update(i,2000+i*8334,true,observation(),{});CHECK(q.launch(Vec(),Vec(0,0,60)));
    t.projectileSpeed=20;t.minFlightS=.2;Profile p;CHECK(Profile::create(120,2,t,&p));Ballistic b;CHECK(Ballistic::solve(Vec(0,0,10),Vec(),p,&b));CHECK(std::abs(b.seconds-.5)<1e-12&&std::abs(b.direction.z-1)<1e-12);CHECK(!Ballistic::solve(Vec(0,0,3),Vec(),p,&b));}
Candidate candidate(uint64_t id,double distance){Candidate c;c.id=id;c.instance=id+10;c.humanInstance=4;c.sector=Sector::Front;c.distance=distance;return c;}
void candidatePriorityWaitsForVisibility(){
    Runtime r;activate(r);const auto far=candidate(1,24),near=candidate(2,16);
    for(const auto& order:{std::vector<Candidate>{far,near},std::vector<Candidate>{near,far}}){
        Candidate selected;unsigned queries=0;
        // A cached positive for the farther AI must not bypass the nearer
        // candidate's still-running GPU query; input iteration order is irrelevant.
        CHECK(!r.chooseCandidate(order,[&](const Candidate&c){++queries;CHECK(c.id==2);return CandidateVisibility::Pending;},&selected));
        CHECK(queries==1&&selected.id==0);
        CHECK(r.chooseCandidate(order,[](const Candidate&){return CandidateVisibility::Visible;},&selected));
        CHECK(selected.id==2);
    }
}
void candidatePriorityUsesHistoryDistanceAndId(){
    Runtime r;activate(r);Candidate selected;
    auto visible=[](const Candidate&){return CandidateVisibility::Visible;};
    CHECK(r.chooseCandidate({candidate(3,16),candidate(2,16)},visible,&selected));CHECK(selected.id==2);
    Candidate attacked=candidate(2,16);CHECK(r.begin(attacked,1000));r.cancel("test_cancel");
    // A never-used attacker outranks the nearer but more recently used one.
    CHECK(r.chooseCandidate({attacked,candidate(3,24)},visible,&selected));CHECK(selected.id==3);
    // The same ID with a new instance has no inherited cooldown history.
    ++attacked.instance;
    CHECK(r.chooseCandidate({candidate(3,24),attacked},visible,&selected));CHECK(selected.id==2);
}
void candidateRejectionAllowsNextAndInvalidInputFails(){
    Runtime r;activate(r);Candidate selected;std::vector<uint64_t> queries;
    CHECK(r.chooseCandidate({candidate(1,24),candidate(2,16)},[&](const Candidate&c){queries.push_back(c.id);return c.id==2?CandidateVisibility::Rejected:CandidateVisibility::Visible;},&selected));
    CHECK((queries==std::vector<uint64_t>{2,1})&&selected.id==1);
    unsigned calls=0;auto visible=[&](const Candidate&){++calls;return CandidateVisibility::Visible;};
    CHECK(!r.chooseCandidate({candidate(2,NAN)},visible,&selected));CHECK(calls==0);
    CHECK(!r.chooseCandidate(std::vector<Candidate>(33,candidate(2,16)),visible,&selected));CHECK(calls==0);
    CHECK(!r.chooseCandidate({},visible,&selected));CHECK(calls==0);
}

void partyServiceCostReserveAndFixedPointBoundary() {
    for(int fps:{60,61,120,144,1000})for(unsigned crew=1;crew<=5;++crew) {
        Profile p;CHECK(Profile::create(fps,crew,&p));
        CHECK(p.serviceCost(ServiceKind::NormalBoost)==25*p.q&&p.serviceCost(ServiceKind::PartyBoost)==15*p.q);
        for(const auto kind:{ServiceKind::NormalBoost,ServiceKind::PartyBoost}) {
            const int64_t boundary=p.reserve+p.serviceCost(kind);
            CHECK(p.admitsService(kind,boundary));CHECK(!p.admitsService(kind,boundary-1));
            CHECK(!p.admitsService(kind,-1));CHECK(p.admitsService(kind,100*p.q));
        }
        CHECK(!p.admitsService(ServiceKind(2),100*p.q));
    }
    Profile invalid;CHECK(invalid.serviceCost(ServiceKind::PartyBoost)<0&&!invalid.admitsService(ServiceKind::PartyBoost,INT64_MAX));
}
void partyServiceChargesOnlySuccessAndSharesNormalCooldown() {
    Runtime r;activate(r);CHECK(r.canApplyService(ServiceKind::PartyBoost));const auto full=r.energyUnits();
    CHECK(r.confirmService(ServiceKind::PartyBoost,false));CHECK(r.energyUnits()==full&&r.boostUntil()==0);
    CHECK(r.confirmService(ServiceKind::PartyBoost,true));CHECK(r.energy()==85&&r.boostUntil()==720);
    CHECK(!r.confirmService(ServiceKind::PartyBoost,true)&&r.energy()==85);
    CHECK(r.prepare(1,2,{"A","B"},120));CHECK(r.energy()==85&&r.boostUntil()==720);
    for(int tick=0;tick<=720;++tick)r.update(tick,1000+int64_t(tick)*8334,true,observation(),{});
    CHECK(r.energy()==100&&r.canApplyService(ServiceKind::PartyBoost));
    auto b=beam(r,4);b.context=Context::ServiceBoost;b.sector=Sector::None;b.threat=0;
    r.update(720,6001000,true,observation(),{b});CHECK(r.serviceStatus(ServiceKind::PartyBoost)==Status::Busy);
    for(int tick=721;tick<=780;++tick)r.update(tick,6001000+int64_t(tick-720)*8334,true,observation(),{b});
    CHECK(r.wantsBoost()&&r.canApplyService(ServiceKind::NormalBoost));CHECK(!r.canApplyService(ServiceKind::PartyBoost));
    CHECK(!r.confirmService(ServiceKind::PartyBoost,true)&&r.energy()==100);
    CHECK(r.confirmService(ServiceKind::NormalBoost,true));CHECK(r.energy()==75&&r.boostUntil()==1500);
    CHECK(!r.confirmBoost(true)&&!r.canApplyService(ServiceKind::PartyBoost));
}
void partyServiceExactLiveEnergyAndRejectedEffectHaveNoCharge() {
    Runtime r;activate(r);auto standby=beam(r);standby.threat=0;
    // Five distinct standby admissions at a frozen tick cost25; their genuine
    // releases free the channel. The test uses production transactions only.
    for(unsigned i=0;i<5;++i){standby.epoch=2+2*i;r.update(0,1000,true,observation(),{standby});r.update(0,1000,true,observation(),{});}
    CHECK(r.energy()==75&&r.energyUnits()==r.profile().reserve+15*r.profile().q);
    CHECK(r.canApplyService(ServiceKind::PartyBoost));CHECK(!r.confirmBoost(true));
    const auto before=r.energyUnits();CHECK(r.confirmService(ServiceKind::PartyBoost,false));CHECK(r.energyUnits()==before);
    CHECK(r.confirmService(ServiceKind::PartyBoost,true));CHECK(r.energyUnits()==r.profile().reserve&&r.boostUntil()==720);
    CHECK(!r.canApplyService(ServiceKind::PartyBoost));
    Runtime q;activate(q);auto pending=beam(q);pending.context=Context::ServiceBoost;pending.sector=Sector::None;pending.threat=0;
    q.update(0,1000,true,observation(),{pending});for(int tick=1;tick<=60;++tick)q.update(tick,1000+tick*8334,true,observation(),{pending});
    CHECK(q.wantsBoost());const auto energy=q.energyUnits();CHECK(q.confirmService(ServiceKind::NormalBoost,false));
    CHECK(q.energyUnits()==energy&&q.boostUntil()==0&&!q.wantsBoost());
}
void partyServiceRejectsUnpreparedThreatReinforcementAndSameTickImpact() {
    Runtime r;CHECK(!r.canApplyService(ServiceKind::PartyBoost));CHECK(r.prepare(1,1,{"A","B"},120));
    CHECK(!r.canApplyService(ServiceKind::PartyBoost));r.update(0,1000,true,observation(),{});
    auto standby=beam(r);standby.threat=0;r.update(0,1000,true,observation(),{standby});
    CHECK(r.serviceStatus(ServiceKind::PartyBoost)==Status::Busy&&!r.confirmService(ServiceKind::PartyBoost,true));
    r.update(0,1000,true,observation(),{});telegraph(r);CHECK(r.serviceStatus(ServiceKind::PartyBoost)==Status::ThreatActive);
    const auto energy=r.energyUnits();CHECK(!r.confirmService(ServiceKind::PartyBoost,true)&&r.energyUnits()==energy);
    for(int tick=1;tick<=300;++tick)r.update(tick,2000+tick*8334,true,observation(),{});
    CHECK(r.launch(Vec(),Vec(0,0,40)));r.contact(true,Vec(0,0,1),2600000);
    auto o=observation();o.presentedThreat=r.view().threat;o.presentedContact=r.view().contact;o.presentedPhase=Phase::AwaitImpact;
    o.presentedLow=2600000;o.presentedHigh=2610000;r.update(301,2610000,true,o,{});CHECK(r.confirmImpact(true));
    CHECK(r.view().phase==Phase::Cooldown&&r.serviceStatus(ServiceKind::PartyBoost)==Status::ThreatActive);
    CHECK(!r.confirmService(ServiceKind::PartyBoost,true));r.update(302,2618334,true,observation(),{});
    CHECK(r.canApplyService(ServiceKind::PartyBoost));r.suspend("pause");CHECK(!r.canApplyService(ServiceKind::PartyBoost));
}
struct Case{const char*name;void(*run)();};
const Case cases[]={
#define DEFENSE_CASE(name) {#name,name},
#include "scintilla_defense_cases.inc"
#undef DEFENSE_CASE
};
}
int main(int argc,char** argv) {
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto& c:cases)std::cout<<c.name<<'\n';return 0;}
    if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;
    bool found=false;for(const auto& c:cases){if(argc==3&&std::string(argv[2])!=c.name)continue;c.run();found=true;}
    return found?0:2;
}
