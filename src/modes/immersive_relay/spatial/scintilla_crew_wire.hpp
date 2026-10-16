// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Game-thread integration. The authenticated host attests possession; gameplay
// authority, source identities and every effect remain native-owned.
#ifndef HEADER_SCINTILLA_CREW_WIRE_HPP
#define HEADER_SCINTILLA_CREW_WIRE_HPP
namespace {
namespace CrewProtocol=ScintillaCrewProtocol;
CrewProtocol::Receipts crewRequests;
CrewProtocol::Identity crewVerifiedIdentity,crewPreparedIdentity;
struct VerifiedCrewBinding {std::string person,source,fingerprint,continuity;uint64_t loss=0;};
std::vector<VerifiedCrewBinding> crewVerifiedBindings;
std::deque<std::string> crewChallenges;
bool crewBindingsConfirmed=false;
// Zone admission is implemented by the native physical qualification adapter.
bool crewPartyZonesQualified();
CrewProtocol::Identity crewIdentity()
{
    CrewProtocol::Identity i;i.producerUUID=producerID;i.raceEpoch=std::to_string(raceEpoch);
    i.driverTurnId=std::to_string(crew.driverTurnId());i.crewIntentRevision=std::to_string(crew.intentRevision());
    i.rosterRevision=std::to_string(rosterEpoch);i.inputSessionID=consumer.inputSessionID();i.inputEpoch=std::to_string(consumer.inputEpoch());
    i.routeRevision=consumer.activeRouteRevision();i.projectionContextId=consumer.activeProjectionContextId();
    // The selected layout is derivable before the first frame, including a
    // same-grant race reset. Empty layout with a live input epoch is incoherent.
    if(!crew.required()){i.layoutProfile=mission.required()?"front_rear_cargo_v2":"front_rear_readonly_v1";i.layoutRevision=mission.required()?"2":"1";}
    return i;
}
std::string crewPhysicalHash(const BindingState& b)
{
    return missionSHA256(CrewProtocol::physicalSourceCanonical(b.sourceId,b.deviceBindingRevision,
        b.trackingSpaceEpoch,b.roomCalibrationRevision,b.gripCalibrationRevision));
}
bool crewBindingsLive(bool neutral)
{
    if(!crewBindingsConfirmed||crewVerifiedIdentity!=crewIdentity()||!consumer.prepared()||consumer.inputMaintenanceActive())return false;
    const auto bindings=consumer.bindingStates();
    if(bindings.size()!=crewVerifiedBindings.size()||bindings.size()!=lastRoster.size())return false;
    for(const auto& b:bindings) {
        if(!b.tracked||!b.continuityIntact||(neutral&&!b.neutral)||b.trackingLossRevision==UINT64_MAX)return false;
        const auto old=std::find_if(crewVerifiedBindings.begin(),crewVerifiedBindings.end(),[&](const VerifiedCrewBinding& v){return v.person==b.participantId&&v.source==b.sourceId;});
        if(old==crewVerifiedBindings.end()||old->fingerprint!=crewPhysicalHash(b)||old->continuity!=b.poseContinuityId||old->loss!=b.trackingLossRevision)return false;
    }
    return true;
}
bool crewPreparationBound()
{
    return !crew.required()||(!missionTerminal()&&crew.prepared()&&crewPreparedIdentity==crewIdentity()&&
        crewBindingsLive(RelaySessionController::get()->crewHandoffReadyRequired())&&crewHUDAssetsReady()&&missionPreparationBound()&&(!crew.partyEnabled()||crewPartyZonesQualified()));
}
void resetCrewAuthority()
{
    crewBindingsConfirmed=false;crewVerifiedBindings.clear();crewChallenges.clear();
    crewVerifiedIdentity=CrewProtocol::Identity();crewPreparedIdentity=CrewProtocol::Identity();
    // Receipts are bounded across races; old replies cannot mutate new gates.
}
void writeCrewParticipants(std::ostream& out,const Crew::Result& frozen)
{
    if(frozen.valid){out<<'[';for(unsigned i=0;i<frozen.participants.size();++i){if(i)out<<',';
        Crew::writeReportParticipantJSON(out,frozen.participants[i]);}out<<']';return;}
    using ScintillaJSON::quote;out<<'[';const auto assignments=crew.assignments();
    for(unsigned i=0;i<assignments.size();++i) {
        if(i)out<<',';const auto& a=assignments[i];const auto& record=crew.records()[i];
        out<<"{\"participantId\":"<<quote(a.participantId)<<",\"name\":"<<quote(a.name)<<",\"isPilot\":"<<(a.isPilot?"true":"false")
           <<",\"competence\":"<<(a.isPilot?"null":quote(Crew::Runtime::competenceName(a.competence)))<<",\"contributions\":{";
        for(unsigned c=0;c<9;++c){if(c)out<<',';out<<quote(Crew::Runtime::contributionName(Crew::ContributionKind(c)))<<':'<<quote(std::to_string(record.contributions[c]));}
        out<<"},\"opportunityTicks\":{";const char* metrics[]={"noOpportunity","waiting","engaged","notReady","paused"};
        for(unsigned c=0;c<5;++c){if(c)out<<',';out<<quote(metrics[c])<<':'<<quote(std::to_string(record.opportunityTicks[c]));}out<<"}}";
    }out<<']';
}
Reply crewError(int status,const std::string& reason)
{return Reply(status,"{\"ok\":false,\"error\":"+ScintillaJSON::quote(reason)+"}");}
}
bool crewStartPermitted()
{
    const auto state=RelaySessionController::get()->getSnapshot();
    // Stage A completes on the existing native authority; stage B has its own
    // held gate and is never a prerequisite for creating the new turn.
    return !crew.required()||state.m_rotation.m_handoff_active||(crewPreparationBound()&&
        (RelaySessionController::get()->getSpatialGameplayState().m_active||crewBindingsLive(true)));
}
std::string crewStateJSON()
{
    using ScintillaJSON::quote;refreshRoster();synchronizeCrewTurn();const auto frozen=frozenCrewResult();
    const bool prepared=crew.required()&&crewPreparationBound();const bool verified=crew.required()&&crewBindingsLive(false);
    const bool running=prepared&&RelaySessionController::get()->getSpatialGameplayState().m_active;
    std::ostringstream out;out<<"{\"contract\":"<<quote(CrewProtocol::contract())<<",\"schemaVersion\":1,\"ok\":true,\"identity\":"<<CrewProtocol::identityJSON(crewIdentity())
        <<",\"required\":"<<(crew.required()?"true":"false")<<",\"prepared\":"<<(prepared?"true":"false")<<",\"active\":"<<(running?"true":"false")
        <<",\"profileId\":"<<quote(crew.profileId())<<",\"partyEnabled\":"<<(crew.partyEnabled()?"true":"false")<<",\"bindingsVerified\":"<<(verified?"true":"false")
        <<",\"reason\":"<<quote(!crew.required()?"crew_not_required":missionTerminal()?"finished":!verified?"crew_bindings_unverified":!prepared?"preparation_required":RelaySessionController::get()->crewHandoffReadyRequired()?"world_held":"ready")<<",\"participants\":";
    writeCrewParticipants(out,frozen);out<<",\"suggestions\":[";
    if(running){bool first=true;for(const auto& s:crew.suggestions()){
        if(!first)out<<',';first=false;out<<"{\"participantId\":"<<quote(s.participantId)<<",\"taskKind\":"<<quote(Crew::Runtime::taskName(s.kind))
            <<",\"offenseAction\":"<<(s.kind==Crew::TaskKind::Offense?quote(Crew::Runtime::offenseActionName(s.offenseAction)):"null")
            <<",\"targetId\":"<<(s.targetId?quote(std::to_string(s.targetId)):"null")<<",\"instance\":"<<(s.instance?quote(std::to_string(s.instance)):"null")
            <<",\"threatId\":"<<(s.threatId?quote(std::to_string(s.threatId)):"null")<<",\"moduleId\":"<<(s.moduleId?quote(std::to_string(s.moduleId)):"null")
            <<",\"expiresAtTick\":"<<quote(std::to_string(s.expiresAtTick))<<'}';
    }}
    out<<"],\"releaseAdvice\":[";
    if(running){bool first=true;for(const auto& advice:crew.releaseAdvice()){
        if(!first)out<<',';first=false;out<<"{\"participantId\":"<<quote(advice.participantId)
            <<",\"threatId\":"<<quote(std::to_string(advice.threatId))<<",\"expiresAtTick\":"<<quote(std::to_string(advice.expiresAtTick))<<'}';
    }}
    out<<"],\"party\":";writeCrewParty(out);out<<",\"result\":";
    Crew::writeResultJSON(out,frozen);out<<'}';
    return out.str();
}
Reply crewState()
{
    refreshRoster();synchronizeCrewTurn();
    if(crew.required()&&!crew.configured())return crewError(409,"crew_profile_locked");
    return Reply(200,crewStateJSON());
}
Reply crewCommand(const std::string& operation,const std::string& body)
{
    try {
        const auto request=CrewProtocol::request(body,operation);
        const auto match=crewRequests.check(request);
        if(match==CrewProtocol::Receipts::Conflict)return crewError(409,"crew_identity_changed");
        if(match==CrewProtocol::Receipts::Duplicate){const auto* reply=crewRequests.response(request);return Reply(reply->httpStatus,reply->body);}
        refreshRoster();synchronizeCrewTurn();
        auto reject=[&](int status,const std::string& reason){const auto r=crewError(status,reason);crewRequests.commit(request,r.json,r.status);return r;};
        if(request.expected!=crewIdentity())return reject(409,"crew_identity_changed");
        auto* controller=RelaySessionController::get();auto* world=World::getWorld();
        if(!world||!controller->isWorldHeld()||missionTerminal()||missionMeasurementActive())return reject(409,"world_held");
        if(operation=="intent") {
            if(world->getTimeTicks()!=0)return reject(409,"crew_profile_locked");
            const bool changesSelection=!request.required||request.required!=crew.required()||request.profileId!=crew.profileId();
            // A preview can return to SC4. Once any downstream core has been
            // prepared, a profile change needs the next race configuration.
            // Complete every admission check before closing or changing gates.
            if(changesSelection&&(combat.configured()||defense.configured()||mission.configured()))
                return reject(409,"crew_profile_locked");
            const auto native=controller->getSpatialParticipants();if(request.required&&(native.size()<2||native.size()>6))return reject(409,"crew_profile_locked");
            auto nextCrew=crew;auto nextMission=mission;auto nextCombat=combat;auto nextDefense=defense;
            if(!nextCrew.setIntent(request.required,request.profileId)||!nextMission.setRequired(request.required)||
                (request.required&&(!nextCombat.setRequired(true)||!nextDefense.setRequired(true))))return reject(409,"crew_profile_locked");
            if(request.required) {
                std::vector<Crew::Participant> people;
                for(const auto& p:native){Crew::Participant v;v.id=p.m_player_id;v.name=p.m_display_name;people.push_back(v);}
                const auto rotation=controller->getSnapshot().m_rotation;
                if(rotation.m_active_index<0||rotation.m_completed_handoffs<0||
                   !nextCrew.bindRace(raceEpoch,people,unsigned(rotation.m_active_index),uint64_t(rotation.m_completed_handoffs),stk_config->getPhysicsFPS()))
                    return reject(409,"crew_profile_locked");
            }
            controller->setCrewStartState(request.required,false);controller->setScintillaStartPermitted(false);
            crew=std::move(nextCrew);mission=std::move(nextMission);combat=std::move(nextCombat);defense=std::move(nextDefense);
            crew.suspend();resetCrewAuthority();clearCrewCoordinationEvidence();synchronizeCrewTurn();
            std::string reason;if(request.required){prepareMissionPresentationAssets(&reason);loadMissionIntro(&reason);prepareMissionAnchors(&reason);prepareCrewPartyZones(&reason);}
        } else {
            if(!crew.required()||!crew.configured())return reject(409,"crew_not_required");
            if(controller->getSnapshot().m_rotation.m_handoff_active||!consumer.prepared()||consumer.inputMaintenanceActive())return reject(409,"crew_identity_changed");
            if(operation=="bindings-confirm") {
                const auto bindings=consumer.bindingStates();const auto people=crew.supporterIDs();
                if(bindings.size()!=request.bindings.size()||bindings.size()!=people.size()||
                    std::find(crewChallenges.begin(),crewChallenges.end(),request.challengeId)!=crewChallenges.end())return reject(409,"crew_bindings_unverified");
                std::vector<VerifiedCrewBinding> verified;
                for(const auto& proof:request.bindings) {
                    if(proof.bindingRevision!=std::to_string(consumer.inputEpoch())||CrewProtocol::unsignedValue(proof.challengeSequence)==0||
                       std::find(people.begin(),people.end(),proof.participantId)==people.end())return reject(409,"crew_bindings_unverified");
                    const auto source=std::find_if(bindings.begin(),bindings.end(),[&](const BindingState& b){return b.participantId==proof.participantId&&b.sourceId==proof.sourceId;});
                    if(source==bindings.end()||!source->tracked||!source->neutral||!source->continuityIntact||source->trackingLossRevision==UINT64_MAX||crewPhysicalHash(*source)!=proof.physicalFingerprint)return reject(409,"crew_bindings_unverified");
                    if(proof.verification=="retained") {
                        const auto prior=std::find_if(crewVerifiedBindings.begin(),crewVerifiedBindings.end(),[&](const VerifiedCrewBinding& b){return b.person==proof.participantId&&b.source==proof.sourceId&&b.fingerprint==proof.physicalFingerprint&&b.continuity==source->poseContinuityId&&b.loss==source->trackingLossRevision;});
                        if(prior==crewVerifiedBindings.end())return reject(409,"crew_bindings_unverified");
                    }
                    VerifiedCrewBinding v;v.person=source->participantId;v.source=source->sourceId;v.fingerprint=proof.physicalFingerprint;v.continuity=source->poseContinuityId;v.loss=source->trackingLossRevision;verified.push_back(v);
                }
                crewVerifiedBindings=std::move(verified);crewVerifiedIdentity=crewIdentity();crewBindingsConfirmed=true;
                if(crewChallenges.size()==64)crewChallenges.pop_front();crewChallenges.push_back(request.challengeId);
            } else if(operation=="prepare") {
                if(!crewBindingsLive(true))return reject(409,"crew_bindings_unverified");
                if(!crewHUDAssetsReady()||!missionPreparationBound()||!missionIntroComplete()||!consumer.defenseVideoReady(nowUs(),true))return reject(409,"crew_preflight_required");
                if(crew.partyEnabled()&&!crewPartyZonesQualified())return reject(409,"party_zones_unqualified");
                const auto native=controller->getSpatialParticipants();std::vector<Crew::Participant> people;
                for(const auto& p:native){Crew::Participant v;v.id=p.m_player_id;v.name=p.m_display_name;people.push_back(v);}
                const auto rotation=controller->getSnapshot().m_rotation;
                if(!crew.prepare(raceEpoch,consumer.inputEpoch(),rosterEpoch,people,unsigned(rotation.m_active_index),uint64_t(rotation.m_completed_handoffs),stk_config->getPhysicsFPS()))return reject(409,"crew_identity_changed");
                crewPreparedIdentity=crewIdentity();
            }
        }
        refreshCombatInput();const auto result=Reply(200,crewStateJSON());crewRequests.commit(request,result.json,result.status);return result;
    }catch(const std::exception&){return crewError(400,"crew_identity_changed");}
}
#endif
