// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Included by scintilla_engine.cpp on its owning game/GL thread. Facts and
// effects keep the same native owners; this file adds no transport or world.
#ifndef HEADER_SCINTILLA_CREW_ADAPTER_HPP
#define HEADER_SCINTILLA_CREW_ADAPTER_HPP
namespace {
void synchronizeCrewTurn()
{
    if(!crew.required())return;
    const auto state=RelaySessionController::get()->getSnapshot();
    const auto native=RelaySessionController::get()->getSpatialParticipants();
    if(native.size()<2||native.size()>6||state.m_rotation.m_active_index<0||
        state.m_rotation.m_completed_handoffs<0){crew.suspend();return;}
    std::vector<Crew::Participant> people;
    for(const auto& person:native){Crew::Participant p;p.id=person.m_player_id;p.name=person.m_display_name;people.push_back(p);}
    if(!crew.configured()) {
        crew.bindRace(raceEpoch,people,unsigned(state.m_rotation.m_active_index),
            uint64_t(state.m_rotation.m_completed_handoffs),stk_config->getPhysicsFPS());
        return;
    }
    const auto administrative=RelaySessionController::get()->getCrewAdministrativeHandoffs();
    if(!crew.observeTurn(raceEpoch,unsigned(state.m_rotation.m_active_index),uint64_t(state.m_rotation.m_completed_handoffs),administrative!=crewObservedAdministrative))crew.suspend();
    crewObservedAdministrative=administrative;
    if(crew.prepared()&&(crew.inputEpoch()!=consumer.inputEpoch()||crew.rosterRevision()!=rosterEpoch))crew.suspend();
}
void recordCrewCredit(const std::string& participant,Crew::ContributionKind kind)
{
    if(!crew.required()||!crew.prepared()||missionTerminal())return;
    if(crewCreditSerial==UINT64_MAX)return;
    crew.credit(participant,kind,++crewCreditSerial);
}
Crew::Bundle crewOffenseBundle(const ScintillaCombat::Snapshot& state)
{
    Crew::Bundle b;b.kind=Crew::TaskKind::Offense;b.targetId=state.target;b.instance=state.instance;b.phaseRevision=state.window;
    const auto& people=combat.crew();
    if((state.phase==ScintillaCombat::Phase::Acquiring||state.phase==ScintillaCombat::Phase::Breached)&&
       state.opener>=0&&size_t(state.opener)<people.size())b.offenseOwner=people[size_t(state.opener)];
    b.offenseBreached=state.phase==ScintillaCombat::Phase::Breached;
    for(unsigned i=0;i<people.size()&&i<5;++i)if(state.contributors&(1u<<i))b.offenseContributors.push_back(people[i]);
    return b;
}
#ifndef SERVER_ONLY
struct CrewVisibilityQuery {
    uint64_t entity=0,instance=0;
    std::shared_ptr<Frame> frame;
    std::shared_ptr<void> lease;
};
std::array<CrewVisibilityQuery,4> crewVisibility;
// Query a genuinely presented immutable frame. A pending older query is polled
// until ready; choosing a new frame on every tick would starve asynchronous GPU.
bool crewSamePhase(const Target& a,const Target& b)
{
    return a.kind==b.kind&&a.id==b.id&&a.instance==b.instance&&
        (a.kind==TargetKind::Rival?(a.combat.instance==b.combat.instance&&a.combat.window==b.combat.window&&a.combat.eligible&&b.combat.eligible):
        (a.mission.phaseRevision==b.mission.phaseRevision&&a.mission.eligible&&b.mission.eligible));
}
struct CrewVisibleFact {Target target;uint64_t frame=0;std::string stream;int64_t low=0;};
std::vector<CrewVisibleFact> crewRivalVisibility;
unsigned crewRivalCursor=0;
Visibility crewTargetVisible(unsigned slot,const Target& currentTarget,int64_t now)
{
    auto& q=crewVisibility[slot];
    const auto records=consumer.presentedMissionFrames(now);
    if(q.entity!=currentTarget.id||q.instance!=currentTarget.instance||
       (q.frame&&!std::any_of(q.frame->targets.begin(),q.frame->targets.end(),[&](const Target& t){return crewSamePhase(t,currentTarget);})))q=CrewVisibilityQuery();
    if(q.frame) {
        const bool retained=std::any_of(records.begin(),records.end(),[&](const PresentedFrame& r){return r.frame.id==q.frame->id&&r.frame.stream==q.frame->stream&&r.low<=now&&now-r.low<=50000;});
        if(!retained)q=CrewVisibilityQuery();
    }
    if(!q.frame)for(const auto& record:records) {
        if(record.low>now||now-record.low>50000)continue;
        const auto target=std::find_if(record.frame.targets.begin(),record.frame.targets.end(),[&](const Target& t){return crewSamePhase(t,currentTarget);});
        if(target==record.frame.targets.end())continue;
        auto frame=std::make_shared<Frame>(record.frame);
        frame->queryOwner=(uint64_t(1)<<63)+100+slot;frame->visibilityLease=pinFrame(*frame);
        if(!frame->visibilityLease)continue;
        q.entity=target->id;q.instance=target->instance;q.frame=frame;q.lease=queryLease(frame->queryOwner);break;
    }
    if(!q.frame||!q.lease)return Visibility::Unavailable;
    const auto historical=std::find_if(q.frame->targets.begin(),q.frame->targets.end(),[&](const Target& t){return t.id==q.entity&&t.instance==q.instance;});
    if(historical==q.frame->targets.end())return Visibility::Unavailable;
    return defensePointVisibility(*q.frame,historical->center,historical->radius,false);
}
#endif
void clearCrewCoordinationEvidence()
{
#ifndef SERVER_ONLY
    for(auto& q:crewVisibility)q=CrewVisibilityQuery();
    crewRivalVisibility.clear();crewRivalCursor=0;
#endif
    crewPausedObservedUs=crewPausedRemainder=0;
}
void updateCrewCoordination(bool admitted)
{
    if(!crew.required()||!crew.configured())return;
    const int64_t now=nowUs();
    admitted=admitted&&crew.prepared()&&crewPreparationBound()&&RelaySessionController::get()->getSpatialGameplayState().m_active;
    std::vector<Crew::Bundle> bundles;
    std::vector<std::string> ready;
    std::vector<Crew::Occupation> occupied;
    if(admitted) {
        for(const auto& r:consumer.reticles())if(r.valid&&r.updatedUs<=now&&now-r.updatedUs<=60000)ready.push_back(r.participantId);
        const auto worldBeams=consumer.beamProofs(now);
        for(const auto& b:worldBeams)if(b.held&&b.visible&&b.target&&b.instance) {
            Crew::Occupation o;o.participantId=b.participantId;o.kind=Crew::TaskKind::Offense;o.targetId=b.target;o.instance=b.instance;o.phaseRevision=combat.snapshot(b.target).window;occupied.push_back(o);
        }
        for(const auto& b:consumer.missionBeams(now))if(b.held&&b.visible&&!b.interrupted&&b.entity&&b.instance) {
            Crew::Occupation o;o.participantId=b.participant;o.kind=b.kind==ScintillaMission::Kind::Tether?Crew::TaskKind::TetherRecover:Crew::TaskKind::CapsuleCollect;
            o.targetId=b.entity;o.instance=b.instance;o.phaseRevision=b.phaseRevision;
            const auto target=mission.snapshot(b.entity);o.moduleId=target.module;occupied.push_back(o);
        }
        for(const auto& b:consumer.defenseBeams(now))if(b.held&&b.visible&&(b.context==Defense::Context::ServiceBoost||
            (b.context==Defense::Context::DefenseSector&&b.threat))) {
            Crew::Occupation o;o.participantId=b.participant;o.kind=b.context==Defense::Context::ServiceBoost?Crew::TaskKind::NormalBoost:Crew::TaskKind::Defense;o.threatId=b.context==Defense::Context::DefenseSector?b.threat:0;occupied.push_back(o);
        }
        if(crew.partyEnabled())for(const auto& b:currentCrewPartyBeams(now))if(b.held&&b.visible&&!b.interrupted&&b.partyContext) {
            Crew::Occupation o;o.participantId=b.participantId;o.kind=Crew::TaskKind::PartyBoost;occupied.push_back(o);
        }
        // A held control remains a voluntary occupation even after a negative
        // proof. Never assign a second task or fabricate a release for it.
        for(const auto& r:consumer.reticles())if((r.toolHeld||r.primaryHeld)&&std::none_of(occupied.begin(),occupied.end(),[&](const Crew::Occupation& o){return o.participantId==r.participantId;})) {
            Crew::Occupation o;o.participantId=r.participantId;o.unassigned=true;occupied.push_back(o);
        }
        auto offer=[&](Crew::Bundle b) {
            b.eligibleParticipantIds=ready;
            if(!b.deadline){const int64_t ttl=(crew.profile().fps*3+4)/5;b.deadline=simulationTick<=INT64_MAX-ttl?simulationTick+ttl:INT64_MAX;}
            if(b.deadline>simulationTick&&bundles.size()<7)bundles.push_back(b);
        };
        const auto& threat=defense.view();
        if(threat.phase==Defense::Phase::Telegraph) {
            Crew::Bundle b;b.kind=Crew::TaskKind::Defense;b.threatId=threat.threat;b.imminent=true;b.deadline=defense.deadline();offer(b);
        }
#ifndef SERVER_ONLY
        std::vector<Target> rivals;
        for(const auto& target:current.targets)if(target.kind==TargetKind::Rival&&TargetLifetime::isLive(registered,target.id,target.instance,currentTarget)) {
            const auto state=combat.snapshot(target.id);if(state.eligible&&target.combat.eligible&&state.instance==target.combat.instance&&state.window==target.combat.window)rivals.push_back(target);
        }
        auto distance=[](const Target& t){const SpatialVec3 d(t.center.x-current.cameraPosition.x,t.center.y-current.cameraPosition.y,t.center.z-current.cameraPosition.z);return dot(d,d);};
        std::sort(rivals.begin(),rivals.end(),[&](const Target& a,const Target& b){
            const auto aa=combat.snapshot(a.id),bb=combat.snapshot(b.id);
            const bool underwayA=aa.phase==ScintillaCombat::Phase::Acquiring||aa.phase==ScintillaCombat::Phase::Breached||aa.count;
            const bool underwayB=bb.phase==ScintillaCombat::Phase::Acquiring||bb.phase==ScintillaCombat::Phase::Breached||bb.count;
            return underwayA!=underwayB?underwayA:distance(a)!=distance(b)?distance(a)<distance(b):a.id<b.id;
        });
        const auto presented=consumer.presentedMissionFrames(now);
        crewRivalVisibility.erase(std::remove_if(crewRivalVisibility.begin(),crewRivalVisibility.end(),[&](const CrewVisibleFact& f){
            return now<f.low||now-f.low>50000||std::none_of(rivals.begin(),rivals.end(),[&](const Target& t){return crewSamePhase(t,f.target);})||
                std::none_of(presented.begin(),presented.end(),[&](const PresentedFrame& r){return r.frame.id==f.frame&&r.frame.stream==f.stream;});
        }),crewRivalVisibility.end());
        if(!rivals.empty()) {
            // One rival query at a time, bounded to the 32 rendered targets.
            // Preserve a pending query; definite failures advance the cursor.
            auto pending=std::find_if(rivals.begin(),rivals.end(),[&](const Target& t){return t.id==crewVisibility[0].entity&&t.instance==crewVisibility[0].instance;});
            const unsigned index=pending!=rivals.end()?unsigned(pending-rivals.begin()):crewRivalCursor%rivals.size();
            const auto visibility=crewTargetVisible(0,rivals[index],now);
            if(visibility!=Visibility::Pending) {
                if(visibility==Visibility::Visible&&crewVisibility[0].frame) {
                    const auto& q=crewVisibility[0];
                    crewRivalVisibility.erase(std::remove_if(crewRivalVisibility.begin(),crewRivalVisibility.end(),[&](const CrewVisibleFact& f){return f.target.id==rivals[index].id;}),crewRivalVisibility.end());
                    for(const auto& r:presented)if(r.frame.id==q.frame->id&&r.frame.stream==q.frame->stream) {
                        CrewVisibleFact fact;fact.target=rivals[index];fact.frame=r.frame.id;fact.stream=r.frame.stream;fact.low=r.low;
                        if(crewRivalVisibility.size()<32)crewRivalVisibility.push_back(fact);break;
                    }
                }
                crewVisibility[0]=CrewVisibilityQuery();crewRivalCursor=(index+1)%rivals.size();
            }
            for(const auto& target:rivals)if(std::any_of(crewRivalVisibility.begin(),crewRivalVisibility.end(),[&](const CrewVisibleFact& f){return crewSamePhase(f.target,target);})) {
                offer(crewOffenseBundle(combat.snapshot(target.id)));break;
            }
        }
        unsigned capsuleSlot=1;
        for(const auto& state:mission.snapshots())if(state.eligible) {
            const auto target=std::find_if(current.targets.begin(),current.targets.end(),[&](const Target& t){return t.id==state.id&&t.instance==state.instance;});
            if(target==current.targets.end()||!target->mission.eligible||target->mission.phaseRevision!=state.phaseRevision)continue;
            const bool tether=state.kind==ScintillaMission::Kind::Tether;
            const unsigned slot=tether?3:capsuleSlot++;
            if(slot>=4||(!tether&&slot>2)||crewTargetVisible(slot,*target,now)!=Visibility::Visible)continue;
            Crew::Bundle b;b.kind=tether?Crew::TaskKind::TetherRecover:state.phase==ScintillaMission::Phase::Shell?Crew::TaskKind::CapsuleShell:Crew::TaskKind::CapsuleCollect;
            b.targetId=state.id;b.instance=state.instance;b.moduleId=state.module;b.phaseRevision=state.phaseRevision;
            if(state.deadline>0&&missionClockBase>=0&&missionClockBase<=INT64_MAX-state.deadline)b.deadline=state.deadline+missionClockBase;
            b.imminent=tether&&b.deadline>simulationTick&&b.deadline-simulationTick<=(crew.profile().fps*6+4)/5;
            offer(b);
        }
#endif
        if(defense.boostStatus(ScintillaDefensePhysics::eligibleHuman(defenseHuman()))==Defense::Status::BoostReady) {
            Crew::Bundle b;b.kind=Crew::TaskKind::NormalBoost;offer(b);
        }
        const auto& party=crew.partyView();
        if(crew.partyEnabled()&&party.zoneQualified&&party.zoneAvailable&&
            (party.state==Crew::PartyState::Idle||party.state==Crew::PartyState::Collecting)) {
            Crew::Bundle b;b.kind=Crew::TaskKind::PartyBoost;b.deadline=party.deadlineTick;offer(b);
        }
    }
    crew.update(simulationTick,admitted,bundles,ready,occupied);
    if(!admitted&&crewPausedObservedUs>0&&now>=crewPausedObservedUs) {
        const int64_t elapsed=now-crewPausedObservedUs;
        if(elapsed<=(INT64_MAX-crewPausedRemainder)/crew.profile().fps) {
            const int64_t scaled=elapsed*crew.profile().fps+crewPausedRemainder;
            crew.accountPaused(scaled/1000000);crewPausedRemainder=scaled%1000000;
        }
    }
    crewPausedObservedUs=admitted?0:now;
}
}
bool crewRequired(){return crew.required();}
#endif
