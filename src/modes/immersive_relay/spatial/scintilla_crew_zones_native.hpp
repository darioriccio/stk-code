// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_ZONES_NATIVE_HPP
#define HEADER_SCINTILLA_CREW_ZONES_NATIVE_HPP
// Included by the engine, on the owning game thread. Asset reads happen only
// while held; the gameplay path reads this bounded immutable qualification.
namespace {
namespace CrewZones=ScintillaCrewZones;
CrewZones::Manifest crewZoneManifest;
CrewZones::Tracker crewZoneTracker;
std::string crewZoneHash,crewZoneReason="party_zones_unqualified";
uint64_t crewZoneRace=0;
bool crewZoneQualified=false;
void resetCrewPartyZones() {
    crewZoneManifest=CrewZones::Manifest();crewZoneTracker.reset();crewZoneHash.clear();
    crewZoneRace=0;crewZoneQualified=false;crewZoneReason="party_zones_unqualified";
}
std::string crewPartyZoneManifestHash(){return crewZoneHash;}
bool prepareCrewPartyZones(std::string* reason) {
    auto fail=[&](const std::string& r){crewZoneQualified=false;crewZoneReason=r;if(reason)*reason=r;return false;};
    if(!RelaySessionController::get()->isWorldHeld())return fail("party_zone_load_requires_held_world");
    resetCrewPartyZones();
#ifndef SERVER_ONLY
    auto* graph=DriveGraph::get();auto* track=Track::getCurrentTrack();
    if(!graph||!track)return fail("party_zone_native_track");
    std::string bytes,quads,edges;
    if(!readMissionFile(file_manager->getAsset("immersive_relay/scintilla/snowtuxpeak_party_zones_v1.json"),16384,&bytes)||
       !readMissionFile(track->getTrackFile("quads.xml"),2*1024*1024,&quads)||!readMissionFile(track->getTrackFile("graph.xml"),2*1024*1024,&edges))return fail("party_zone_asset_unavailable");
    CrewZones::Manifest candidate;std::string error;
    if(!CrewZones::load(bytes,&candidate,&error))return fail(error);
    std::vector<double> nodes;for(unsigned n=0;n<graph->getNumNodes();++n)nodes.push_back(graph->getDistanceFromStart(n));
    if(!CrewZones::validateNative(candidate,RaceManager::get()->getTrackName(),missionSHA256(quads+edges),graph->isReverse(),graph->getLapLength(),nodes,&error))return fail(error);
    crewZoneManifest=candidate;crewZoneHash=missionSHA256(bytes);crewZoneRace=raceEpoch;
    if(candidate.status!="authored")return fail("party_zones_unqualified");
    std::string report,evidence;
    if(!readMissionFile(file_manager->getAsset("immersive_relay/scintilla/qualification_v2/party_zones_v1.json"),16384,&report)||
       !readMissionFile(file_manager->getAsset("immersive_relay/scintilla/qualification_v2/party_zones_evidence_v1.json"),65536,&evidence))return fail("party_zone_native_evidence_unavailable");
    if(!loadMeasurementFingerprints(&error))return fail(error);
    std::vector<std::string> proofs;
    if(!CrewZones::nativeEvidence(candidate,evidence,crewZoneHash,measurementExecutableHash,measurementAssetsHash,stk_config->getPhysicsFPS(),&proofs,&error))return fail(error);
    for(const auto& hash:proofs) {
        std::string capture;
        if(!readMissionFile(file_manager->getAsset("immersive_relay/scintilla/qualification_v2/party_zone_proofs/"+hash+".proof"),1024*1024,&capture)||missionSHA256(capture)!=hash)
            return fail("party_zone_native_capture_unavailable");
    }
    if(!CrewZones::qualify(candidate,report,crewZoneHash,missionSHA256(evidence),measurementExecutableHash,measurementAssetsHash,stk_config->getPhysicsFPS(),&error))return fail(error);
    crewZoneQualified=true;crewZoneReason="ready";if(reason)reason->clear();return true;
#else
    return fail("party_zone_renderer_unavailable");
#endif
}
bool crewPartyZonesQualified(){return crewZoneQualified&&crewZoneRace==raceEpoch;}
double crewKartExtent(AbstractKart* kart) {
    if(!kart||!kart->getBody()||!kart->getBody()->getCollisionShape())return -1;
    btVector3 lo,hi;kart->getBody()->getCollisionShape()->getAabb(btTransform::getIdentity(),lo,hi);
    double radius2=0;for(unsigned axis=0;axis<3;++axis){const double x=std::max(std::abs(double(lo[axis])),std::abs(double(hi[axis])));if(!std::isfinite(x))return -1;radius2+=x*x;}
    // Circumsphere about the actual Bullet origin includes compound offsets
    // and every chassis orientation, rather than assuming a centred kart box.
    return std::sqrt(radius2);
}
CrewPartyPhysicalStatus crewPartyPhysicalStatus() {
    CrewPartyPhysicalStatus out;auto* human=defenseHuman();
    const auto native=ScintillaCrewPhysics::inspect(human);out.humanEligible=native.eligible;
    out.driverVeto=native.driverVeto;out.nativeBoostActive=native.nativeBoostActive;
    auto* graph=DriveGraph::get();auto* world=dynamic_cast<LinearWorld*>(World::getWorld());
    if(!crewPartyZonesQualified()||!human||!human->getBody()||!human->getKartProperties()||!graph||!world||
       !RelaySessionController::get()->getSpatialGameplayState().m_active||missionTerminal()||
       double(human->getKartProperties()->getEngineMaxSpeed())+1>crewZoneManifest.speedBound||
       !std::isfinite(human->getCurrentMaxSpeed())||human->getCurrentMaxSpeed()>crewZoneManifest.speedBound+0.001) {
        crewZoneTracker.reset();return out;
    }
    CrewZones::Sample s;s.tick=simulationTick;s.lap=world->getLapForKart(human->getWorldKartId());
    s.distance=world->getDistanceDownTrackForKart(human->getWorldKartId(),true);
    const auto p=human->getBody()->getWorldTransform().getOrigin();s.x=p.x();s.y=p.y();s.z=p.z();
    s.speed=human->getBody()->getLinearVelocity().length();s.onRoad=world->isOnRoad(human->getWorldKartId());s.animated=human->getKartAnimation()!=nullptr;
    crewZoneTracker.update(s,stk_config->getPhysicsFPS(),graph->getLapLength(),crewZoneManifest.speedBound);
    out.zoneAvailable=crewZoneTracker.available(crewZoneManifest,graph->getLapLength(),RaceManager::get()->getNumLaps(),crewKartExtent(human));return out;
}
bool applyCrewPartyBoost() {
    const auto status=crewPartyPhysicalStatus();
    return crewPartyZonesQualified()&&status.humanEligible&&!status.driverVeto&&!status.nativeBoostActive&&status.zoneAvailable&&
        ScintillaCrewPhysics::apply(defenseHuman(),stk_config->getPhysicsFPS());
}
}
#endif
