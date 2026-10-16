// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Exercises production controller/rotation through public APIs. No game, GL,
// input grant, binding proof or prepared native physics is fabricated here.
#include "modes/immersive_relay/relay_session_controller.hpp"
#include "modes/immersive_relay/remote/relay_state_publisher.hpp"
#include "modes/immersive_relay/scintilla_crew_result_json.hpp"
#include "modes/immersive_relay/scintilla_crew_report_file.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <iterator>
#include <sstream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(#x)+":"+std::to_string(__LINE__));}while(false)
namespace {
struct Fixture {
    RelaySessionController c;RelaySessionConfig config;std::vector<RelayPlayer> people;std::string reason;
    explicit Fixture(bool confirmation=true) {
        config.m_track_id="crew-controller-no-assets";config.m_player_count=3;
        config.m_crew_mode="disabled";config.m_countdown_seconds=0;
        config.m_handoff_seconds=6;config.m_min_handoff_seconds=6;
        config.m_handoff_warning_seconds=0;config.m_require_handoff_confirm=confirmation;
        config.m_auto_confirm_handoff=!confirmation;
        people={RelayPlayer("p1","One"),RelayPlayer("p2","Two"),RelayPlayer("p3","Three")};
        c.setRuntimeEnabled(true);c.setOperatorControlled(true);
        CHECK(c.configureArcadeSession(config,people,&reason));c.attachRace(config.m_track_id,3);
        c.setCrewStartState(true,false);
    }
    void start() {c.setCrewStartState(true,true);CHECK(c.operatorCountdown(true,&reason));CHECK(!c.isWorldHeld());}
    void lapHandoff() {c.notifyLap(1,.1f,true);CHECK(c.getSnapshot().m_rotation.m_handoff_active);c.setCrewStartState(true,false);CHECK(c.isWorldHeld());}
};
void initialGateBlocksAllStartsAndAdministrativeTurn() {
    Fixture f;CHECK(f.c.isWorldHeld());CHECK(!f.c.getSnapshot().m_can_start);
    CHECK(!f.c.operatorStart(&f.reason));CHECK(!f.c.operatorCountdown(true,&f.reason));
    CHECK(!f.c.pilotReady("p1",&f.reason));CHECK(!f.c.operatorNextState("countdown",&f.reason));
    CHECK(!f.c.operatorNextState("running",&f.reason));CHECK(!f.c.startSession(f.config,f.people,&f.reason));
    CHECK(!f.c.forceHandoff("p2",&f.reason));CHECK(!f.c.skipHandoff());
    f.c.resumeSession();f.c.setScintillaStartPermitted(true);CHECK(f.c.isWorldHeld());
    CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==0);
    CHECK(f.c.getCrewAdministrativeHandoffs()==0);f.start();
}
void confirmedStageACommitsIntoHeldStageB() {
    Fixture f;f.start();const auto revision=f.c.getSpatialGameplayState().m_resume_revision;
    f.lapHandoff();CHECK(!f.c.startSession(f.config,f.people,&f.reason));
    CHECK(f.c.confirmHandoff(&f.reason));f.c.updateRace(9.1f,.1f,true);
    const auto after=f.c.getSnapshot();CHECK(!after.m_rotation.m_handoff_active);
    CHECK(after.m_rotation.m_active_player_id=="p2");CHECK(after.m_rotation.m_completed_handoffs==1);
    CHECK(f.c.isWorldHeld());CHECK(!f.c.getSpatialGameplayState().m_active);
    CHECK(f.c.getSpatialGameplayState().m_resume_revision==revision);
    f.c.setScintillaStartPermitted(true);CHECK(f.c.isWorldHeld());
    CHECK(!f.c.pilotReady("p2",&f.reason));CHECK(!f.c.operatorNextState("running",&f.reason));
    CHECK(!f.c.forceHandoff("p3",&f.reason));CHECK(!f.c.confirmHandoff(&f.reason));
    CHECK(f.c.getCrewAdministrativeHandoffs()==0);
    f.c.setCrewStartState(true,true);CHECK(f.c.isWorldHeld());CHECK(f.c.crewHandoffReadyRequired());
    CHECK(f.c.getSnapshot().m_can_start);CHECK(!f.c.pilotReady("p1",&f.reason));
    CHECK(!f.c.operatorNextState("running",&f.reason));
    CHECK(!f.c.startSession(f.config,f.people,&f.reason));CHECK(!f.c.forceHandoff("p3",&f.reason));
    f.c.updateRace(2,.1f,true);CHECK(f.c.isWorldHeld());
    CHECK(f.c.getSpatialGameplayState().m_resume_revision==revision);
    CHECK(f.c.pilotReady("p2",&f.reason));CHECK(!f.c.isWorldHeld());
    CHECK(!f.c.crewHandoffReadyRequired());CHECK(f.c.getSnapshot().m_arcade_state==ARCADE_STATE_RUNNING);
    CHECK(f.c.getSpatialGameplayState().m_resume_revision==revision+1);
}
void automaticStageAAlsoClosesBeforePhysics() {
    Fixture f(false);f.start();const auto revision=f.c.getSpatialGameplayState().m_resume_revision;
    f.lapHandoff();f.c.updateRace(6.1f,.1f,true);
    CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==1);CHECK(f.c.isWorldHeld());
    CHECK(f.c.getSpatialGameplayState().m_resume_revision==revision);
    CHECK(f.c.getCrewAdministrativeHandoffs()==0);
}
void pausedStageAAllowsPhoneConfirmationButHealthyPresentationIsRequired() {
    Fixture f;f.start();f.lapHandoff();CHECK(f.c.operatorPause(&f.reason));
    f.c.setPresentationReady(false);CHECK(!f.c.pilotReady("p2",&f.reason));
    f.c.setPresentationReady(true);f.c.setScintillaStartPermitted(false);
    CHECK(!f.c.confirmHandoff(&f.reason));f.c.setScintillaStartPermitted(true);
    CHECK(f.c.pilotReady("p2",&f.reason));CHECK(!f.c.getSnapshot().m_paused);
    f.c.updateRace(9.1f,.1f,true);CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==1);
    CHECK(f.c.isWorldHeld());CHECK(!f.c.operatorResume(&f.reason));
}
void skipCreatesOneAdministrativeTurnAndCannotFarmStageB() {
    Fixture f;f.start();f.lapHandoff();CHECK(!f.c.skipHandoff());
    f.c.updateRace(6.1f,.1f,true);CHECK(f.c.getSnapshot().m_rotation.m_handoff_active);
    CHECK(f.c.skipHandoff());CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==1);
    CHECK(f.c.getCrewAdministrativeHandoffs()==1);CHECK(f.c.isWorldHeld());
    CHECK(!f.c.skipHandoff());CHECK(!f.c.forceHandoff("p3",&f.reason));
    CHECK(f.c.getCrewAdministrativeHandoffs()==1);
    f.c.setCrewStartState(true,true);CHECK(f.c.isWorldHeld());
    CHECK(f.c.pilotReady("p2",&f.reason));CHECK(f.c.forceHandoff("p3",&f.reason));
    f.c.setCrewStartState(true,false);CHECK(f.c.confirmHandoff(&f.reason));f.c.updateRace(9.1f,.1f,true);
    CHECK(f.c.getSnapshot().m_rotation.m_active_player_id=="p3");
    CHECK(f.c.getCrewAdministrativeHandoffs()==2);CHECK(f.c.isWorldHeld());
}
void preparedStageBWaitsForEveryExplicitReadyResumeIngress() {
    for(unsigned ingress=0;ingress<5;++ingress) {
        Fixture f;f.start();f.lapHandoff();CHECK(f.c.confirmHandoff(&f.reason));
        f.c.updateRace(9.1f,.1f,true);CHECK(f.c.crewHandoffReadyRequired());
        const auto revision=f.c.getSpatialGameplayState().m_resume_revision;
        if(ingress==2)CHECK(f.c.operatorPause(&f.reason));
        f.c.setCrewStartState(true,true);CHECK(f.c.isWorldHeld());
        if(ingress==0)CHECK(f.c.operatorStart(&f.reason));
        else if(ingress==1)CHECK(f.c.operatorCountdown(false,&f.reason));
        else if(ingress==2)CHECK(f.c.operatorResume(&f.reason));
        else if(ingress==3)f.c.resumeSession();
        else CHECK(f.c.pilotReady("p2",&f.reason));
        CHECK(!f.c.isWorldHeld());CHECK(!f.c.crewHandoffReadyRequired());
        CHECK(f.c.getSnapshot().m_arcade_state==ARCADE_STATE_RUNNING);
        CHECK(f.c.getSnapshot().m_countdown_remaining_ms==0);
        CHECK(f.c.getSpatialGameplayState().m_resume_revision==revision+1);
    }
}
void resetsPreserveRequirementAndCloseOldAdmission() {
    Fixture f;f.start();CHECK(f.c.operatorReset(&f.reason));f.c.setScintillaStartPermitted(true);
    CHECK(f.c.isWorldHeld());CHECK(!f.c.operatorCountdown(true,&f.reason));
    CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==0);
    CHECK(f.c.getCrewAdministrativeHandoffs()==0);
    f.start();f.c.resetSession();f.c.setScintillaStartPermitted(true);
    CHECK(!f.c.startSession(f.config,f.people,&f.reason));CHECK(f.c.isWorldHeld());
}
void gameplayResumeRequiresStageBAndLegacyRemainsIndependent() {
    Fixture f;f.start();CHECK(f.c.operatorPause(&f.reason));f.c.setCrewStartState(true,false);
    CHECK(!f.c.operatorResume(&f.reason));CHECK(!f.c.pilotReady("p1",&f.reason));
    f.c.resumeSession();CHECK(f.c.getSnapshot().m_paused);
    f.c.setCrewStartState(true,true);CHECK(f.c.operatorResume(&f.reason));CHECK(!f.c.isWorldHeld());
    f.c.setScintillaStartPermitted(false);f.c.setCrewStartState(false,true);CHECK(f.c.isWorldHeld());
    f.c.setScintillaStartPermitted(true);CHECK(!f.c.isWorldHeld());
}
std::string crewJSON(const ScintillaCrew::Result& result) {
    std::ostringstream out;ScintillaCrew::writeResultJSON(out,result);return out.str();
}
struct ReportFixture:Fixture {
    ScintillaCrew::Runtime crew;ScintillaMission::Runtime mission;uint64_t race;
    const std::string producer="01234567-89ab-4cde-8fab-0123456789ab";
    explicit ReportFixture(bool party=true,uint64_t epoch=1):Fixture(),race(epoch) {
        CHECK(crew.setIntent(true,party?"sc5_crew_party_v1":"sc5_crew_standard_v1"));
        configureRace();c.setScintillaStartPermitted(true);start();CHECK(mission.advanceTo(1,true));
    }
    void configureRace() {
        std::vector<ScintillaCrew::Participant> roster;
        for(const auto& p:people){ScintillaCrew::Participant a;a.id=p.m_player_id;a.name=p.m_display_name;roster.push_back(a);}
        CHECK(crew.prepare(race,1,1,roster,0,0,120));CHECK(mission.setRequired(true));
        CHECK(mission.prepare(race,1,1,1,{"p2","p3"},120));
        const auto observation=missionObservation();c.beginMissionRace(true,observation,crewObservation(observation));
    }
    ScintillaMission::Result missionObservation()const {
        ScintillaMission::Result r;r.raceEpoch=race;r.missionRevision=1;r.finishTick=mission.tick();
        r.slots=mission.slots();r.securedCount=mission.securedCount();return r;
    }
    ScintillaCrew::Observation crewObservation(const ScintillaMission::Result& missionResult)const {
        return ScintillaCrew::observeResult(crew,producer,race,1,missionResult.finishTick,false,crew.partyEnabled()?std::string(64,'a'):"");
    }
    void finishNatural() {
        CHECK(mission.freezeNaturalFinish(2,12.5,1));const auto terminal=mission.result();auto observation=crewObservation(terminal);
        observation.snapshot=ScintillaCrew::freezeResult(observation,terminal);
        c.observeMission(true,terminal,observation);c.notifyLap(c.getSnapshot().m_total_laps,12.5f,true,&terminal);
    }
};
void naturalCrewReportUsesExactMissionTupleAndSharedPublisherEnvelope() {
    ReportFixture f;CHECK(f.crew.credit("p2",ScintillaCrew::ContributionKind::NormalBoost,1));f.finishNatural();
    const auto report=f.c.getSnapshot().m_report;const auto& result=report.m_scintilla_crew;
    CHECK(report.m_available&&result.valid&&result.comparable&&result.partyEnabled&&result.partyActivations==0);
    CHECK(ScintillaCrew::matchesMissionResult(result,report.m_mission));CHECK(result.bucket=="scintilla_sc5_party_v1");
    const auto published=ScintillaJSON::Parser(RelayStatePublisher::stateJson(f.c.getSnapshot())).parse();
    const auto& session=ScintillaJSON::at(published,"arcade_session");
    for(const char* field:{"report","last_report"}) {
        const auto& publishedReport=ScintillaJSON::at(session,field);
        CHECK(ScintillaJSON::serialize(ScintillaJSON::at(publishedReport,"crew"))==ScintillaJSON::serialize(ScintillaJSON::Parser(crewJSON(result)).parse()));
        CHECK(ScintillaJSON::at(publishedReport,"leaderboard_bucket").string==result.bucket);
    }
    const auto bytes=crewJSON(result);CHECK(f.crew.credit("p2",ScintillaCrew::ContributionKind::NormalBoost,2));
    f.crew.suspend();CHECK(f.crew.accountPaused(500));f.c.observeMission(true,f.missionObservation(),f.crewObservation(f.missionObservation()));
    f.c.finishMissionArtificial("late_abort");CHECK(crewJSON(f.c.getCrewResult())==bytes);
    CHECK(crewJSON(f.c.getSnapshot().m_report.m_scintilla_crew)==bytes);
}
void artificialCrewReportUsesCopiedObservationAndCannotAcquireLateCredits() {
    ReportFixture f;CHECK(f.crew.credit("p2",ScintillaCrew::ContributionKind::Opening,1));
    const auto mission=f.missionObservation();auto observation=f.crewObservation(mission);f.c.observeMission(true,mission,observation);
    observation.snapshot.participants[1].contributions[0]=999;CHECK(f.crew.credit("p2",ScintillaCrew::ContributionKind::Opening,2));
    CHECK(f.c.operatorAbort(&f.reason));const auto result=f.c.getCrewResult();const auto report=f.c.getSnapshot().m_report;
    CHECK(result.valid&&!result.comparable&&result.participants[1].contributions[0]==1);
    CHECK(result.bucket=="scintilla_sc5_party_v1"&&result.partyEnabled&&result.partyActivations==0);
    CHECK(!report.m_mission.natural&&report.m_mission.terminationReason=="operator_abort");
    CHECK(ScintillaCrew::matchesMissionResult(result,report.m_mission));
    f.c.observeMission(true,mission,observation);CHECK(crewJSON(f.c.getCrewResult())==crewJSON(result));
}
void administrativeNativeHandoffMakesNaturalCrewReportNonComparable() {
    ReportFixture f;CHECK(f.c.forceHandoff("p2",&f.reason));f.c.setCrewStartState(true,false);
    CHECK(f.c.confirmHandoff(&f.reason));f.c.updateRace(9.1f,.1f,true);CHECK(f.c.getCrewAdministrativeHandoffs()==1);
    std::vector<ScintillaCrew::Participant> roster;for(const auto& p:f.people){ScintillaCrew::Participant a;a.id=p.m_player_id;a.name=p.m_display_name;roster.push_back(a);}
    // Deliberately do not label the core observation administrative: the native
    // controller's committed handoff remains an independent comparison guard.
    CHECK(f.crew.observeTurn(f.race,1,1));CHECK(f.crew.prepare(f.race,2,2,roster,1,1,120));
    f.c.setCrewStartState(true,true);CHECK(f.c.pilotReady("p2",&f.reason));f.finishNatural();
    const auto result=f.c.getCrewResult();CHECK(result.valid&&!result.comparable&&result.rotationAnomalies==1);
    CHECK(result.completedHandoffs==1&&f.c.getSnapshot().m_report.m_mission.natural);
}
void resetKeepsBothImmutableSessionArchivesAndTheSamePublishedEnvelope() {
    const char* output=std::getenv("SCINTILLA_TEST_OUTPUT_DIR");CHECK(output&&output[0]);
    struct Environment {bool had;std::string old;Environment(const char* directory):had(std::getenv("STK_RELAY_REPORT_DIR")!=nullptr),old(had?std::getenv("STK_RELAY_REPORT_DIR"):"") {
#ifdef _WIN32
        CHECK(_putenv_s("STK_RELAY_REPORT_DIR",directory)==0);
#else
        CHECK(setenv("STK_RELAY_REPORT_DIR",directory,1)==0);
#endif
    }~Environment(){
#ifdef _WIN32
        _putenv_s("STK_RELAY_REPORT_DIR",had?old.c_str():"");
#else
        if(had)setenv("STK_RELAY_REPORT_DIR",old.c_str(),1);else unsetenv("STK_RELAY_REPORT_DIR");
#endif
    }} environment(output);
    const auto epoch=uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());ReportFixture f(true,epoch);f.finishNatural();
    const auto first=f.c.getSnapshot().m_report;const std::string firstPath=std::string(output)+"/"+ScintillaCrew::reportFileName(first.m_session_id,first.m_scintilla_crew);
    struct Cleanup {std::string a,b;~Cleanup(){std::remove(a.c_str());if(!b.empty())std::remove(b.c_str());}} cleanup{firstPath,""};
    const auto read=[](const std::string& path){std::ifstream file(path.c_str());CHECK(file.good());return std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());};
    const auto bytes=read(firstPath);CHECK(f.c.operatorReset(&f.reason));
    f.crew.reset();f.mission.reset();++f.race;f.configureRace();f.c.setScintillaStartPermitted(true);f.start();CHECK(f.mission.advanceTo(1,true));f.finishNatural();
    const auto second=f.c.getSnapshot().m_report;cleanup.b=std::string(output)+"/"+ScintillaCrew::reportFileName(second.m_session_id,second.m_scintilla_crew);
    CHECK(firstPath!=cleanup.b&&read(firstPath)==bytes);
    for(const auto& entry:std::vector<std::pair<std::string,ScintillaCrew::Result> >{{firstPath,first.m_scintilla_crew},{cleanup.b,second.m_scintilla_crew}}) {
        const auto persisted=ScintillaJSON::Parser(read(entry.first)).parse();
        CHECK(ScintillaJSON::serialize(ScintillaJSON::at(persisted,"crew"))==ScintillaJSON::serialize(ScintillaJSON::Parser(crewJSON(entry.second)).parse()));
        CHECK(ScintillaJSON::at(persisted,"leaderboard_bucket").string==entry.second.bucket);
    }
}
void newRacePreviewAbortHasCompleteCrewEnvelopeAndMissingRosterCannotBecomeLegacy() {
    Fixture f;ScintillaCrew::Runtime crew;CHECK(crew.setIntent(true,"sc5_crew_party_v1"));
    std::vector<ScintillaCrew::Participant> roster;for(const auto& p:f.people){ScintillaCrew::Participant a;
        a.id=p.m_player_id;a.name=p.m_display_name;roster.push_back(a);}
    CHECK(crew.bindRace(1,roster,0,0,120));ScintillaMission::Result mission;mission.raceEpoch=1;mission.missionRevision=1;
    const auto observation=ScintillaCrew::observeResult(crew,"01234567-89ab-4cde-8fab-0123456789ab",1,1,0,false,"");
    CHECK(observation.available&&!observation.prepared);f.c.beginMissionRace(true,mission,observation);
    f.c.finishMissionArtificial("world_destroyed");const auto old=f.c.getSnapshot().m_report;
    CHECK(old.m_available&&old.m_scintilla_crew.valid&&!old.m_scintilla_crew.comparable);
    CHECK(old.m_scintilla_crew.partyEnabled&&old.m_scintilla_crew.bucket=="scintilla_sc5_party_v1");
    CHECK(old.m_scintilla_crew.participants.size()==3&&old.m_scintilla_crew.parameters.partyZoneManifestSHA256.empty());
    CHECK(ScintillaCrew::matchesMissionResult(old.m_scintilla_crew,old.m_mission));
    const auto bytes=crewJSON(old.m_scintilla_crew);
    f.c.beginMissionRace(true,mission,observation);CHECK(crewJSON(f.c.getCrewResult())==bytes); // Duplicate cannot reopen.
    mission.raceEpoch=2;f.c.beginMissionRace(true,mission);f.c.finishMissionArtificial("world_destroyed");
    CHECK(!f.c.getSnapshot().m_report.m_available&&!f.c.getCrewResult().valid&&f.c.isWorldHeld());
    CHECK(crewJSON(old.m_scintilla_crew)==bytes);
}
void committedHandoffThenAbortBeforeEngineObservationUsesNativeAuthorityOnly() {
    for(bool administrative:{false,true}) {
        ReportFixture f;CHECK(f.crew.credit("p2",ScintillaCrew::ContributionKind::Opening,1));
        const auto mission=f.missionObservation();const auto before=f.crewObservation(mission);f.c.observeMission(true,mission,before);
        if(administrative){CHECK(f.c.forceHandoff("p2",&f.reason));f.c.setCrewStartState(true,false);}
        else f.lapHandoff();
        CHECK(f.c.confirmHandoff(&f.reason));f.c.updateRace(9.1f,.1f,true);
        CHECK(f.c.getSnapshot().m_rotation.m_completed_handoffs==1&&f.crew.driverTurnId()==0);
        CHECK(f.c.operatorAbort(&f.reason));const auto result=f.c.getCrewResult();
        CHECK(result.valid&&!result.comparable&&result.completedHandoffs==1);
        CHECK(result.rotationAnomalies==(administrative?1u:0u));
        CHECK(result.participants[1].assignment.isPilot&&!result.participants[0].assignment.isPilot);
        for(unsigned i=0;i<result.participants.size();++i){
            CHECK(result.participants[i].contributions==before.snapshot.participants[i].contributions);
            CHECK(result.participants[i].opportunityTicks==before.snapshot.participants[i].opportunityTicks);
        }
        CHECK(result.participants[1].contributions[0]==1&&ScintillaCrew::matchesMissionResult(result,f.c.getSnapshot().m_report.m_mission));
    }
}
}
int main(int argc,char**argv) {
    const std::map<std::string,std::function<void()> > cases={
        {"initialGateBlocksAllStartsAndAdministrativeTurn",initialGateBlocksAllStartsAndAdministrativeTurn},
        {"confirmedStageACommitsIntoHeldStageB",confirmedStageACommitsIntoHeldStageB},
        {"automaticStageAAlsoClosesBeforePhysics",automaticStageAAlsoClosesBeforePhysics},
        {"pausedStageAAllowsPhoneConfirmationButHealthyPresentationIsRequired",pausedStageAAllowsPhoneConfirmationButHealthyPresentationIsRequired},
        {"skipCreatesOneAdministrativeTurnAndCannotFarmStageB",skipCreatesOneAdministrativeTurnAndCannotFarmStageB},
        {"preparedStageBWaitsForEveryExplicitReadyResumeIngress",preparedStageBWaitsForEveryExplicitReadyResumeIngress},
        {"resetsPreserveRequirementAndCloseOldAdmission",resetsPreserveRequirementAndCloseOldAdmission},
        {"gameplayResumeRequiresStageBAndLegacyRemainsIndependent",gameplayResumeRequiresStageBAndLegacyRemainsIndependent},
        {"naturalCrewReportUsesExactMissionTupleAndSharedPublisherEnvelope",naturalCrewReportUsesExactMissionTupleAndSharedPublisherEnvelope},
        {"artificialCrewReportUsesCopiedObservationAndCannotAcquireLateCredits",artificialCrewReportUsesCopiedObservationAndCannotAcquireLateCredits},
        {"administrativeNativeHandoffMakesNaturalCrewReportNonComparable",administrativeNativeHandoffMakesNaturalCrewReportNonComparable},
        {"resetKeepsBothImmutableSessionArchivesAndTheSamePublishedEnvelope",resetKeepsBothImmutableSessionArchivesAndTheSamePublishedEnvelope},
        {"newRacePreviewAbortHasCompleteCrewEnvelopeAndMissingRosterCannotBecomeLegacy",newRacePreviewAbortHasCompleteCrewEnvelopeAndMissingRosterCannotBecomeLegacy},
        {"committedHandoffThenAbortBeforeEngineObservationUsesNativeAuthorityOnly",committedHandoffThenAbortBeforeEngineObservationUsesNativeAuthorityOnly}};
    if(argc!=3||std::string(argv[1])!="--case")return 2;
    auto found=cases.find(argv[2]);if(found==cases.end())return 2;
    try{found->second();std::cout<<found->first<<" passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<found->first<<": "<<e.what()<<'\n';return 1;}
}
