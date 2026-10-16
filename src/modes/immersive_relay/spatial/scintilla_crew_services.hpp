// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Included on the engine's game thread. Impacts remain ahead of mission pickup;
// both optional service requests meet at this one final commit point.
#ifndef HEADER_SCINTILLA_CREW_SERVICES_HPP
#define HEADER_SCINTILLA_CREW_SERVICES_HPP
namespace {
struct CrewPartyPhysicalStatus {
    bool humanEligible=false,driverVeto=false,nativeBoostActive=false,zoneAvailable=false;
};
CrewPartyPhysicalStatus crewPartyPhysicalStatus();
bool applyCrewPartyBoost();
std::vector<Crew::PartyBeam> currentCrewPartyBeams(int64_t now);
Crew::PartyAuthority crewPartyAuthority()
{
    Crew::PartyAuthority a;a.race=raceEpoch;a.inputEpoch=consumer.inputEpoch();a.rosterRevision=rosterEpoch;
    a.driverTurnId=crew.driverTurnId();a.crewIntentRevision=crew.intentRevision();
    a.routeRevision=consumer.activeRouteRevision();a.projectionContextId=consumer.activeProjectionContextId();return a;
}
void commitOptionalService(bool admitted)
{
    // Earlier mission crossings/visibility can establish a hold in this same
    // step. An admission snapshot from before those effects is insufficient.
    const bool live=admitted&&!missionTerminal()&&RelaySessionController::get()->getSpatialGameplayState().m_active&&
        active&&consumer.prepared()&&combat.configured()&&combat.inputEpoch()==consumer.inputEpoch()&&combat.crew()==lastRoster&&
        (!defense.required()||defenseBound())&&missionStartPermitted()&&(!crew.required()||crewPreparationBound());
    const bool normalPriority=defense.wantsBoost();
    if(crew.required()) {
        Crew::PartyGates gates;gates.live=live&&crewPreparationBound();
        gates.zoneQualified=crewPartyZonesQualified();gates.normalBoostPriority=normalPriority;
        gates.threatActive=crewDefenseThreatTick==simulationTick||(defense.view().phase!=Defense::Phase::Idle&&defense.view().phase!=Defense::Phase::Cooldown);
        gates.impactThisTick=defense.impactEvent().race==raceEpoch&&defense.impactEvent().tick==simulationTick&&defense.impactEvent().threat!=0;
        gates.reinforcing=defense.owner()>=0;gates.cooldownUntilTick=defense.boostUntil();
        gates.serviceBusy=!defense.configured()||defense.boostOwner()>=0;
        gates.reserveAvailable=defense.configured()&&defense.profile().admitsService(Defense::ServiceKind::PartyBoost,defense.energyUnits());
        if(crew.partyEnabled()) {
            const auto physical=crewPartyPhysicalStatus();gates.humanEligible=physical.humanEligible;
            gates.driverVeto=physical.driverVeto;gates.nativeBoostActive=physical.nativeBoostActive;gates.zoneAvailable=physical.zoneAvailable;
        }
        crew.updateParty(simulationTick,crewPartyAuthority(),gates,currentCrewPartyBeams(nowUs()));
    }
    if(!live) {
        if(normalPriority)defense.confirmService(Defense::ServiceKind::NormalBoost,false);
        return;
    }
    if(normalPriority) {
        const int owner=defense.boostOwner();
        const auto beams=consumer.defenseBeams(nowUs());
        const bool inputCurrent=owner>=0&&size_t(owner)<defense.crew().size()&&size_t(owner)<defense.feedback().size()&&
            std::any_of(beams.begin(),beams.end(),[&](const Defense::Beam& b){return b.participant==defense.crew()[owner]&&
                b.held&&b.visible&&b.context==Defense::Context::ServiceBoost&&b.epoch==defense.feedback()[owner].epoch;});
        const bool applied=inputCurrent&&defense.canApplyService(Defense::ServiceKind::NormalBoost)&&
            ScintillaDefensePhysics::applyBoost(defenseHuman(),defense.profile().fps);
        defense.confirmService(Defense::ServiceKind::NormalBoost,applied);
        if(applied&&owner>=0&&size_t(owner)<defense.crew().size())recordCrewCredit(defense.crew()[owner],Crew::ContributionKind::NormalBoost);
    } else if(crew.required()&&crew.partyWanted()) {
        const auto consenting=crew.partyView().consentingParticipantIds;
        // No await, HTTP callback or input drain can interleave these checks,
        // the native preflight/write and the two owning-core confirmations.
        const bool applied=crewPreparationBound()&&defense.canApplyService(Defense::ServiceKind::PartyBoost)&&applyCrewPartyBoost();
        const bool charged=defense.confirmService(Defense::ServiceKind::PartyBoost,applied);
        const bool committed=crew.confirmParty(applied&&charged);
        if(committed&&applied&&charged)for(const auto& participant:consenting)recordCrewCredit(participant,Crew::ContributionKind::PartyConsent);
    }
}
void writeCrewParty(std::ostream& out)
{
    using ScintillaJSON::quote;const auto& p=crew.partyView();
    out<<"{\"state\":"<<quote(Crew::Runtime::partyStateName(p.state))<<",\"reason\":"<<quote(Crew::Runtime::partyReasonName(p.reason))
        <<",\"proposalId\":"<<(p.proposalId?quote(std::to_string(p.proposalId)):"null")
        <<",\"deadlineTick\":"<<(p.proposalId?quote(std::to_string(p.deadlineTick)):"null")<<",\"consentingParticipantIds\":[";
    for(unsigned i=0;i<p.consentingParticipantIds.size();++i){if(i)out<<',';out<<quote(p.consentingParticipantIds[i]);}
    out<<"],\"requiredCount\":"<<p.requiredCount<<",\"cost\":15,\"usedThisTurn\":"<<(p.usedThisTurn?"true":"false")
        <<",\"zoneQualified\":"<<(p.zoneQualified?"true":"false")<<",\"zoneAvailable\":"<<(p.zoneAvailable?"true":"false")
        <<",\"cooldownUntilTick\":"<<quote(std::to_string(p.cooldownUntilTick))<<'}';
}
}
#endif
