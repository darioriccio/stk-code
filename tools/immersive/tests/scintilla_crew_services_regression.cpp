// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Exercises the production commit adapter and real resource/consensus cores.
// Only world, consumer and native effect boundaries are controlled here.
#include "modes/immersive_relay/scintilla_crew.hpp"
#include "modes/immersive_relay/scintilla_defense.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
namespace Crew=ScintillaCrew;
namespace Defense=ScintillaDefense;
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);}}while(false)
namespace {
Crew::Runtime crew;Defense::Runtime defense;
bool active=true,terminal=false,bound=true,worldActive=true,nativePartyOK=true,nativeNormalOK=true;
unsigned normalWrites=0,partyWrites=0;uint64_t creditSerial=0,raceEpoch=1,rosterEpoch=1;
int64_t simulationTick=0,crewDefenseThreatTick=-1;
std::vector<std::string> lastRoster;
std::vector<Crew::PartyBeam> partyInput;
struct Consumer {
    std::vector<Defense::Beam> beams;
    bool prepared()const{return true;}
    uint64_t inputEpoch()const{return 1;}
    std::string activeRouteRevision()const{return "route-1";}
    std::string activeProjectionContextId()const{return "projection-1";}
    std::vector<Defense::Beam> defenseBeams(int64_t)const{return beams;}
} consumer;
struct Combat {
    bool configured()const{return true;}
    uint64_t inputEpoch()const{return 1;}
    std::vector<std::string> crew()const{return lastRoster;}
} combat;
bool missionTerminal(){return terminal;}bool defenseBound(){return bound;}
bool missionStartPermitted(){return bound;}bool crewPreparationBound(){return bound;}
bool crewPartyZonesQualified(){return true;}
int64_t nowUs(){return 1000+simulationTick*16667;}
int* defenseHuman(){static int human=1;return &human;}
void recordCrewCredit(const std::string& person,Crew::ContributionKind kind){crew.credit(person,kind,++creditSerial);}
}
struct RelaySessionController {
    struct State{bool m_active;};
    static RelaySessionController* get(){static RelaySessionController value;return &value;}
    State getSpatialGameplayState()const{State s;s.m_active=worldActive;return s;}
};
namespace ScintillaDefensePhysics {bool applyBoost(int*,int){++normalWrites;return nativeNormalOK;}}
#include "modes/immersive_relay/spatial/scintilla_crew_services.hpp"
namespace {
CrewPartyPhysicalStatus crewPartyPhysicalStatus(){CrewPartyPhysicalStatus p;p.humanEligible=p.zoneAvailable=true;return p;}
bool applyCrewPartyBoost(){++partyWrites;return nativePartyOK;}
std::vector<Crew::PartyBeam> currentCrewPartyBeams(int64_t){return partyInput;}
Defense::Observation observation(){Defense::Observation o;o.geometryValid=o.humanEligible=o.videoContinuous=true;return o;}
void setup() {
    crew=Crew::Runtime();defense=Defense::Runtime();active=bound=worldActive=nativePartyOK=nativeNormalOK=true;terminal=false;
    normalWrites=partyWrites=0;creditSerial=0;simulationTick=0;crewDefenseThreatTick=-1;lastRoster={"A","B"};consumer.beams.clear();partyInput.clear();
    std::vector<Crew::Participant> people;for(const auto* id:{"driver","A","B"}){Crew::Participant p;p.id=p.name=id;people.push_back(p);}
    CHECK(crew.setIntent(true,"sc5_crew_party_v1")&&crew.prepare(1,1,1,people,0,0,60));
    CHECK(defense.prepare(1,1,lastRoster,60));defense.update(0,nowUs(),true,observation(),{});
    for(const auto& id:lastRoster){Crew::PartyBeam b;b.participantId=id;b.authority=crewPartyAuthority();b.neutral=true;b.releaseSerial=1;b.controlEpoch=1;partyInput.push_back(b);}
    Crew::PartyGates gates;gates.live=gates.zoneQualified=gates.zoneAvailable=gates.humanEligible=gates.reserveAvailable=true;
    CHECK(crew.updateParty(0,crewPartyAuthority(),gates,partyInput));
    for(auto& b:partyInput){b.neutral=false;b.held=b.visible=b.partyContext=true;b.serial=2;b.controlEpoch=2;b.admittedTick=0;}
}
void matureParty() {
    Crew::PartyGates gates;gates.live=gates.zoneQualified=gates.zoneAvailable=gates.humanEligible=gates.reserveAvailable=true;
    for(simulationTick=0;simulationTick<=45;++simulationTick)CHECK(crew.updateParty(simulationTick,crewPartyAuthority(),gates,partyInput));
    simulationTick=45;CHECK(crew.partyWanted());
}
void matureNormal() {
    Defense::Beam b;b.participant="A";b.epoch=2;b.serial=2;b.context=Defense::Context::ServiceBoost;b.held=b.visible=true;
    consumer.beams={b};
    for(int64_t tick=0;tick<=45;++tick)defense.update(tick,1000+tick*16667,true,observation(),consumer.beams);
    CHECK(defense.wantsBoost()&&defense.canApplyService(Defense::ServiceKind::NormalBoost));
}
void normalWinsAtTheProductionCommitPointAndCannotChargeTwice() {
    setup();matureParty();matureNormal();const auto before=defense.energyUnits();
    commitOptionalService(true);CHECK(normalWrites==1&&partyWrites==0);
    CHECK(defense.energyUnits()==before-25*defense.profile().q&&defense.boostUntil()==405);
    CHECK(!crew.partyView().usedThisTurn&&crew.partyActivations()==0&&!crew.partyWanted());
    CHECK(crew.records()[1].contributions[unsigned(Crew::ContributionKind::NormalBoost)]==1);
    commitOptionalService(true);CHECK(normalWrites==1&&partyWrites==0&&defense.energyUnits()==before-25*defense.profile().q);
}
void partySuccessChargesOnceAndCreditsOnlyTheCommittedQuorum() {
    setup();matureParty();for(int64_t tick=1;tick<=45;++tick)defense.update(tick,1000+tick*16667,true,observation(),{});
    const auto before=defense.energyUnits();commitOptionalService(true);
    CHECK(partyWrites==1&&normalWrites==0&&crew.partyActivations()==1&&crew.partyView().usedThisTurn);
    CHECK(defense.energyUnits()==before-15*defense.profile().q&&defense.boostUntil()==405);
    for(unsigned person=1;person<=2;++person)CHECK(crew.records()[person].contributions[unsigned(Crew::ContributionKind::PartyConsent)]==1);
    commitOptionalService(true);CHECK(partyWrites==1&&defense.energyUnits()==before-15*defense.profile().q&&crew.partyActivations()==1);
}
void rejectedPhysicsHasNeitherChargeCooldownQuotaNorLateRetry() {
    setup();matureParty();for(int64_t tick=1;tick<=45;++tick)defense.update(tick,1000+tick*16667,true,observation(),{});
    nativePartyOK=false;const auto energy=defense.energyUnits();commitOptionalService(true);
    CHECK(partyWrites==1&&defense.energyUnits()==energy&&defense.boostUntil()==0&&crew.partyActivations()==0);
    nativePartyOK=true;commitOptionalService(true);CHECK(partyWrites==1&&!crew.partyWanted());
    for(const auto& person:crew.records())CHECK(person.contributions[unsigned(Crew::ContributionKind::PartyConsent)]==0);
}
void lateHoldTerminalAndSameTickThreatWinBeforeOptionalPhysics() {
    for(unsigned cause=0;cause<4;++cause){setup();matureParty();for(int64_t tick=1;tick<=45;++tick)defense.update(tick,1000+tick*16667,true,observation(),{});
        const auto energy=defense.energyUnits();if(cause==0)worldActive=false;if(cause==1)terminal=true;if(cause==2)bound=false;if(cause==3)crewDefenseThreatTick=simulationTick;
        commitOptionalService(true);CHECK(normalWrites==0&&partyWrites==0&&defense.energyUnits()==energy&&defense.boostUntil()==0&&crew.partyActivations()==0&&!crew.partyWanted());
    }
    setup();matureNormal();worldActive=false;const auto energy=defense.energyUnits();commitOptionalService(true);
    CHECK(normalWrites==0&&partyWrites==0&&!defense.wantsBoost()&&defense.energyUnits()==energy&&defense.boostUntil()==0);
}
struct Case{const char* name;void(*run)();};const Case cases[]={
    {"normalWinsAtTheProductionCommitPointAndCannotChargeTwice",normalWinsAtTheProductionCommitPointAndCannotChargeTwice},
    {"partySuccessChargesOnceAndCreditsOnlyTheCommittedQuorum",partySuccessChargesOnceAndCreditsOnlyTheCommittedQuorum},
    {"rejectedPhysicsHasNeitherChargeCooldownQuotaNorLateRetry",rejectedPhysicsHasNeitherChargeCooldownQuotaNorLateRetry},
    {"lateHoldTerminalAndSameTickThreatWinBeforeOptionalPhysics",lateHoldTerminalAndSameTickThreatWinBeforeOptionalPhysics}};
}
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--list"){for(const auto& c:cases)std::cout<<c.name<<'\n';return 0;}
if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;bool found=false;for(const auto& c:cases)if(argc==1||argv[2]==std::string(c.name)){c.run();found=true;}return found?0:2;}
