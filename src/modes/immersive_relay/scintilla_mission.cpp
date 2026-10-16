// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_mission.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ScintillaMission
{
const uint64_t Runtime::TetherEntityId=UINT64_C(0x5343340000000001);
const uint64_t Runtime::CapsuleEntityBase=UINT64_C(0x5343330000000000);
namespace
{
bool identifier(const std::string& value)
{
    if(value.empty() || value.size()>128) return false;
    for(unsigned char c:value) if(c<32 || c==127) return false;
    return true;
}
Outcome outcome(Status status,bool accepted=false,bool changed=false)
{
    Outcome value;value.status=status;value.accepted=accepted;value.changed=changed;
    return value;
}
}

bool Profile::create(int frequency,Profile* out)
{
    if(!out || frequency<1 || frequency>1000) return false;
    Profile p;p.fps=frequency;
    p.collect=p.recover=(INT64_C(600000)*frequency+999999)/1000000;
    p.recoveryWindow=(INT64_C(3000000)*frequency)/1000000;
    if(p.collect<=0 || p.collect>=p.recoveryWindow) return false;
    *out=p;return true;
}

bool Profile::inRange(Kind kind,double distanceSquared,double radius)
{
    if(kind!=Kind::Capsule && kind!=Kind::Tether) return false;
    const double radiusSquared=radius*radius;
    if(!std::isfinite(distanceSquared) || !std::isfinite(radius) ||
       !std::isfinite(radiusSquared) || radius<=0 || distanceSquared<=radiusSquared)
        return false;
    const double nearDistance=kind==Kind::Capsule?8.0:0.75;
    const double farDistance=kind==Kind::Capsule?75.0:15.0;
    const double sine=std::sin(1.5*3.14159265358979323846/180.0);
    return distanceSquared>=nearDistance*nearDistance &&
           distanceSquared<=farDistance*farDistance &&
           distanceSquared<=radiusSquared/(sine*sine);
}

bool Runtime::setRequired(bool value)
{
    if((m_active || configured()) && !value) return false;
    if(value==m_required) return true;
    if(m_intentRevision==std::numeric_limits<uint64_t>::max()) return false;
    m_required=value;++m_intentRevision;return true;
}

bool Runtime::prepare(uint64_t race,uint64_t revision,uint64_t input,
                      uint64_t human,const std::vector<std::string>& people,int fps)
{
    Profile p;
    if(!race || !revision || !input || !human || people.empty() ||
       people.size()>Profile::MaxCrew || !Profile::create(fps,&p)) return false;
    for(std::size_t i=0;i<people.size();++i)
    {
        if(!identifier(people[i])) return false;
        for(std::size_t j=0;j<i;++j) if(people[i]==people[j]) return false;
    }
    const bool preserve=configured();
    if(preserve && (race!=m_race || revision!=m_revision || human!=m_humanInstance ||
                    fps!=m_profile.fps || people.size()!=m_crew.size() ||
                    input<m_input || m_result.valid)) return false;
    // Crew rebinding must come from a new consumer input epoch. Otherwise a
    // release predating the rebind could incorrectly satisfy its fresh gate.
    if(preserve && people!=m_crew && input==m_input) return false;
    // An exact retry is observationally idempotent: do not cancel a live hold.
    if(preserve && input==m_input && people==m_crew) return true;
    if(preserve) suspendInput();
    if(input!=m_input) m_lastShotSerial=0;
    m_race=race;m_revision=revision;m_input=input;m_humanInstance=human;
    m_profile=p;m_crew=people;m_active=false;
    for(std::size_t i=0;i<m_tools.size();++i)
    {
        m_tools[i]=Tool();
        if(i<people.size())
        {
            m_tools[i].feedback.participant=people[i];
            // A rebind must observe a release in the new input epoch.
            m_tools[i].mustRelease=preserve;
            m_tools[i].blocked=preserve;
            if(preserve) m_tools[i].feedback.status=Status::ReleaseRequired;
        }
    }
    ++m_stateVersion;return true;
}

void Runtime::reset(bool preserveIntent)
{
    const bool required=preserveIntent&&m_required;
    const uint64_t intent=m_intentRevision;
    *this=Runtime();m_required=required;m_intentRevision=intent;
}

void Runtime::suspendInput()
{
    bool changed=m_active;
    for(unsigned i=0;i<m_crew.size();++i)
        if(m_tools[i].held || !m_tools[i].mustRelease) changed=true;
    for(unsigned i=0;i<m_crew.size();++i) closeTool(i,true);
    m_active=false;
    if(changed) ++m_stateVersion;
}

bool Runtime::advanceTo(Tick next,bool active)
{
    if(!configured() || m_result.valid || next<0 || next<m_now) return false;
    if(!active)
    {
        if(next!=m_now) return false;
        suspendInput();return true;
    }
    if(next-m_now>1) return false;
    if(!m_active) ++m_stateVersion;
    m_active=true;
    if(next!=m_now)
    {
        m_now=next;expire();++m_stateVersion;
    }
    return true;
}

int Runtime::participantSlot(const std::string& person) const
{
    for(std::size_t i=0;i<m_crew.size();++i) if(m_crew[i]==person) return int(i);
    return -1;
}

bool Runtime::sameContext(uint64_t race,uint64_t revision,uint64_t input) const
{
    return configured() && race==m_race && revision==m_revision && input==m_input;
}

Runtime::Capsule* Runtime::capsule(uint64_t entity)
{
    for(std::size_t i=0;i<m_capsuleCount;++i)
        if(m_capsules[i].spec.id==entity) return &m_capsules[i];
    return nullptr;
}
const Runtime::Capsule* Runtime::capsule(uint64_t entity) const
{
    for(std::size_t i=0;i<m_capsuleCount;++i)
        if(m_capsules[i].spec.id==entity) return &m_capsules[i];
    return nullptr;
}

unsigned Runtime::securedCount() const
{
    unsigned count=0;
    for(const Slot& slot:m_slots) if(slot.state==SlotState::Secured) ++count;
    return count;
}
unsigned Runtime::occupiedCount() const
{
    unsigned count=0;
    for(const Slot& slot:m_slots) if(slot.state!=SlotState::Empty) ++count;
    return count;
}

Outcome Runtime::activateCapsule(const CapsuleSpec& spec)
{
    if(!active() || spec.id<CapsuleEntityBase || spec.id>=TetherEntityId ||
       !spec.instance || !spec.module || spec.occurrence>=Profile::MaxOpportunities ||
       spec.expires<=m_now || m_occurrences.test(spec.occurrence) ||
       m_capsuleCount>=Profile::MaxOpportunities) return outcome(Status::Rejected);
    unsigned activeCount=0;
    for(std::size_t i=0;i<m_capsuleCount;++i)
    {
        const Capsule& c=m_capsules[i];
        if(c.spec.id==spec.id || c.spec.module==spec.module) return outcome(Status::Rejected);
        if(c.phase==Phase::Shell || c.phase==Phase::Open) ++activeCount;
    }
    if(occupiedCount()==Profile::SlotCount)
    {
        m_occurrences.set(spec.occurrence);++m_stateVersion;
        return outcome(Status::OccurrenceSkipped,true,true);
    }
    const unsigned maximum=m_crew.size()==1?1:2;
    if(activeCount>=maximum) return outcome(Status::Rejected);
    Capsule c;c.spec=spec;
    m_capsules[m_capsuleCount++]=c;m_occurrences.set(spec.occurrence);
    ++m_stateVersion;
    Outcome result=outcome(Status::None,true,true);
    result.entity=spec.id;result.module=spec.module;return result;
}

bool Runtime::skipOccurrence(unsigned ordinal)
{
    if(!active() || ordinal>=Profile::MaxOpportunities || m_occurrences.test(ordinal))
        return false;
    m_occurrences.set(ordinal);++m_stateVersion;return true;
}

void Runtime::closeTool(unsigned index,bool releaseRequired)
{
    if(index>=m_crew.size()) return;
    Tool& tool=m_tools[index];
    for(std::size_t i=0;i<m_capsuleCount;++i)
    {
        Capsule& c=m_capsules[i];
        if(c.owner==int(index)) {c.owner=-1;c.started=c.lastSample=0;}
    }
    if(m_tether.owner==int(index))
    {m_tether.owner=-1;m_tether.started=m_tether.lastSample=0;}
    tool.held=false;tool.blocked=releaseRequired;tool.mustRelease=releaseRequired;
    tool.feedback.owner=-1;
    tool.feedback.status=releaseRequired?Status::ReleaseRequired:Status::None;
}

void Runtime::invalidateEntity(uint64_t entity)
{
    for(unsigned i=0;i<m_crew.size();++i)
        if(m_tools[i].entity==entity) closeTool(i,true);
}

bool Runtime::expireCapsule(uint64_t entity,uint64_t instance)
{
    if(!active()) return false;
    Capsule* c=capsule(entity);
    if(!c || c->spec.instance!=instance ||
       (c->phase!=Phase::Shell && c->phase!=Phase::Open)) return false;
    invalidateEntity(entity);c->owner=-1;c->phase=Phase::Expired;++c->phaseRevision;
    ++m_stateVersion;return true;
}

void Runtime::expire()
{
    for(std::size_t i=0;i<m_capsuleCount;++i)
    {
        Capsule& c=m_capsules[i];
        if((c.phase==Phase::Shell || c.phase==Phase::Open) && c.spec.expires<=m_now)
            expireCapsule(c.spec.id,c.spec.instance);
    }
    if(m_tether.exists && m_tether.phase==Phase::TetherRecoverable &&
       m_tether.deadline<=m_now)
    {
        invalidateEntity(TetherEntityId);
        Slot& slot=m_slots[m_tether.slot];slot.state=SlotState::Empty;slot.module=0;
        for(unsigned i=0;i<m_crew.size();++i)
            if(m_tools[i].entity==TetherEntityId)
                m_tools[i].feedback.outcome=Status::ModuleLost;
        m_tether.exists=false;m_tether.phase=Phase::Expired;
        m_tether.owner=-1;++m_stateVersion;
    }
}

Outcome Runtime::openCapsule(const Shot& shot)
{
    Capsule* c=capsule(shot.entity);
    if(!c || !sameContext(shot.race,shot.revision,shot.inputEpoch) ||
       c->spec.instance!=shot.instance || !shot.serial ||
       participantSlot(shot.participant)<0 || !identifier(shot.event))
        return outcome(Status::Rejected);
    // The successful receipt is retained with its bounded occurrence. Replays
    // return the original semantic outcome without repeating any mutation.
    if(c->openSerial==shot.serial && c->openInput==shot.inputEpoch &&
       c->openParticipant==shot.participant && c->openEvent==shot.event)
    {
        Outcome result=outcome(Status::ShellOpened,true,false);
        result.entity=c->spec.id;result.module=c->spec.module;return result;
    }
    if(!active() || !shot.historicalEligible || !shot.admittedEligible ||
       c->spec.expires<=m_now || c->phaseRevision!=shot.phaseRevision ||
       shot.serial<=m_lastShotSerial) return outcome(Status::Rejected);
    m_lastShotSerial=shot.serial;
    if(c->phase==Phase::Open) return outcome(Status::UseTool,true,false);
    if(c->phase!=Phase::Shell) return outcome(Status::Rejected);
    invalidateEntity(c->spec.id);c->phase=Phase::Open;++c->phaseRevision;
    c->openSerial=shot.serial;c->openInput=shot.inputEpoch;
    c->openParticipant=shot.participant;c->openEvent=shot.event;
    ++m_stateVersion;
    Outcome result=outcome(Status::ShellOpened,true,true);
    result.entity=c->spec.id;result.module=c->spec.module;return result;
}

bool Runtime::validBeam(const Beam& beam) const
{
    if(!sameContext(beam.race,beam.revision,beam.inputEpoch) || !beam.controlEpoch ||
       !beam.serial || !beam.visible || !beam.historicalEligible || beam.interrupted)
        return false;
    if(beam.kind==Kind::Tether)
        return beam.entity==TetherEntityId && m_tether.exists &&
               beam.instance==m_tether.instance &&
               beam.phaseRevision==m_tether.phaseRevision &&
               m_tether.phase==Phase::TetherRecoverable && m_now<m_tether.deadline;
    if(beam.kind!=Kind::Capsule) return false;
    const Capsule* c=capsule(beam.entity);
    return c && c->spec.instance==beam.instance &&
           c->phaseRevision==beam.phaseRevision && c->phase==Phase::Open &&
           c->spec.expires>m_now &&
           (c->openInput!=m_input || beam.serial>c->openSerial);
}

void Runtime::updateBeams(const std::vector<Beam>& beams)
{
    if(!active()) return;
    ++m_stateVersion;
    // There are at most five sources. Duplicate or missing source samples are
    // fail-closed and cannot choose a winner by callback/vector order.
    std::array<const Beam*,Profile::MaxCrew> samples={};
    std::array<bool,Profile::MaxCrew> duplicates={};
    if(beams.size()>Profile::MaxCrew)
    { for(unsigned i=0;i<m_crew.size();++i) closeTool(i,true);return; }
    for(const Beam& b:beams)
    {
        const int index=participantSlot(b.participant);
        if(index<0) continue;
        if(samples[index]) duplicates[index]=true;
        else samples[index]=&b;
    }
    std::vector<unsigned> ordered;
    for(unsigned i=0;i<m_crew.size();++i)
    {
        Tool& tool=m_tools[i];const Beam* b=samples[i];
        if(!b || duplicates[i])
        { if(tool.held || tool.entity) closeTool(i,true);continue; }
        if(!sameContext(b->race,b->revision,b->inputEpoch) || b->controlEpoch<tool.epoch)
        { closeTool(i,true);continue; }
        if(!b->held)
        {
            closeTool(i,false);tool.epoch=b->controlEpoch;tool.serial=0;
            tool.entity=tool.instance=tool.phaseRevision=0;
            tool.feedback.controlEpoch=tool.epoch;tool.feedback.entity=0;
            tool.feedback.outcome=Status::None;continue;
        }
        // SC1 release receipts survive coalescing. A new epoch alone is never
        // proof of release, and a release before this press cannot heal a
        // negative that already occurred within this same control epoch.
        if(b->controlEpoch>tool.epoch && b->releaseSerial>tool.lastPressSerial &&
           b->releaseSerial<b->serial)
            closeTool(i,false);
        if(tool.mustRelease || tool.blocked)
        {
            tool.epoch=b->controlEpoch;tool.serial=b->serial;
            tool.lastPressSerial=std::max(tool.lastPressSerial,b->serial);
            tool.feedback.controlEpoch=b->controlEpoch;
            tool.feedback.status=Status::ReleaseRequired;continue;
        }
        if(tool.held && (b->controlEpoch!=tool.epoch || b->serial!=tool.serial ||
                         b->entity!=tool.entity || b->instance!=tool.instance ||
                         b->phaseRevision!=tool.phaseRevision || b->kind!=tool.kind))
        {
            tool.epoch=b->controlEpoch;tool.serial=b->serial;
            tool.lastPressSerial=std::max(tool.lastPressSerial,b->serial);
            closeTool(i,true);continue;
        }
        if(!validBeam(*b))
        {
            tool.entity=b->entity;tool.epoch=b->controlEpoch;tool.serial=b->serial;
            tool.lastPressSerial=std::max(tool.lastPressSerial,b->serial);
            tool.feedback.entity=b->entity;tool.feedback.controlEpoch=b->controlEpoch;
            closeTool(i,true);continue;
        }
        if(!tool.held && (b->controlEpoch<=tool.epoch || b->serial<=tool.lastPressSerial))
        {closeTool(i,true);continue;}
        ordered.push_back(i);
    }
    std::sort(ordered.begin(),ordered.end(),[&](unsigned a,unsigned b)
    {
        if(samples[a]->serial!=samples[b]->serial) return samples[a]->serial<samples[b]->serial;
        return m_crew[a]<m_crew[b];
    });
    for(unsigned index:ordered) processBeam(*samples[index],index);
}

void Runtime::processBeam(const Beam& beam,unsigned index)
{
    Tool& tool=m_tools[index];
    if(!validBeam(beam)) {closeTool(index,true);return;}
    tool.epoch=beam.controlEpoch;tool.serial=beam.serial;
    tool.lastPressSerial=beam.serial;
    tool.entity=beam.entity;tool.instance=beam.instance;tool.kind=beam.kind;
    tool.phaseRevision=beam.phaseRevision;tool.held=true;
    tool.feedback.controlEpoch=beam.controlEpoch;tool.feedback.entity=beam.entity;
    tool.feedback.owner=-1;tool.feedback.status=Status::None;tool.feedback.outcome=Status::None;
    int* owner=nullptr;Tick* started=nullptr;Tick* lastSample=nullptr;
    Capsule* c=nullptr;
    if(beam.kind==Kind::Capsule)
    {c=capsule(beam.entity);owner=&c->owner;started=&c->started;lastSample=&c->lastSample;}
    else {owner=&m_tether.owner;started=&m_tether.started;lastSample=&m_tether.lastSample;}
    if(*owner>=0 && *owner!=int(index))
    {
        tool.feedback.status=Status::TargetBusy;tool.feedback.owner=*owner;return;
    }
    if(*owner==int(index) && m_now-*lastSample>1)
    { closeTool(index,true);return; }
    if(*owner<0) {*owner=int(index);*started=m_now;}
    *lastSample=m_now;
    const Tick duration=beam.kind==Kind::Capsule?m_profile.collect:m_profile.recover;
    if(m_now-*started<duration) return;
    if(c) completeCapsule(*c,index);else completeTether(index);
}

void Runtime::completeCapsule(Capsule& c,unsigned tool)
{
    int empty=-1;
    for(unsigned i=0;i<m_slots.size();++i)
        if(m_slots[i].state==SlotState::Empty) {empty=int(i);break;}
    if(empty<0)
    {
        closeTool(tool,true);m_tools[tool].feedback.outcome=Status::CargoFull;return;
    }
    Slot& slot=m_slots[empty];slot.state=SlotState::Secured;
    slot.module=c.spec.module;slot.everAcquired=true;
    c.phase=Phase::Collected;++c.phaseRevision;invalidateEntity(c.spec.id);
    m_tools[tool].feedback.outcome=Status::ModuleCollected;++m_stateVersion;
}

void Runtime::completeTether(unsigned tool)
{
    Slot& slot=m_slots[m_tether.slot];
    // The slot is reserved throughout the transfer. Never allocate a new module.
    if(slot.state!=SlotState::Tethered || slot.module!=m_tether.module)
    {closeTool(tool,true);return;}
    slot.state=SlotState::Secured;invalidateEntity(TetherEntityId);
    m_tether.exists=false;m_tether.phase=Phase::Collected;m_tether.owner=-1;
    m_tools[tool].feedback.outcome=Status::ModuleRecovered;++m_stateVersion;
}

Outcome Runtime::consumeImpact(const Impact& impact)
{
    if(!active() || impact.race!=m_race || impact.humanInstance!=m_humanInstance ||
       !impact.threat || impact.threat<=m_lastImpact) return outcome(Status::Rejected);
    // Consume even with empty cargo or an existing cable; neither can be replayed
    // later after the inventory changes.
    m_lastImpact=impact.threat;++m_stateVersion;
    if(m_tether.exists) return outcome(Status::None,true,false);
    int slot=-1;
    for(int i=Profile::SlotCount-1;i>=0;--i)
        if(m_slots[i].state==SlotState::Secured) {slot=i;break;}
    if(slot<0) return outcome(Status::None,true,false);
    if(m_tetherSequence==std::numeric_limits<uint64_t>::max())
        return outcome(Status::Rejected); // Fail closed without changing cargo.
    Tether tether;tether.exists=true;tether.instance=++m_tetherSequence;
    tether.phaseRevision=1;tether.module=m_slots[slot].module;tether.slot=slot;
    m_slots[slot].state=SlotState::Tethered;m_tether=tether;
    Outcome result=outcome(Status::None,true,true);
    result.entity=TetherEntityId;result.module=tether.module;result.slot=slot;return result;
}

bool Runtime::confirmTetherPresented(uint64_t entity,uint64_t instance,uint64_t phase)
{
    if(!configured() || m_result.valid || !m_tether.exists || entity!=TetherEntityId ||
       instance!=m_tether.instance) return false;
    // A duplicate receipt for the first presented phase never restarts time.
    if(m_tether.phase==Phase::TetherRecoverable)
        return phase==m_tether.phaseRevision || phase==m_tether.phaseRevision-1;
    if(phase!=m_tether.phaseRevision) return false;
    if(m_tether.phase!=Phase::TetherAwaitingPresentation ||
       m_now>std::numeric_limits<Tick>::max()-m_profile.recoveryWindow) return false;
    invalidateEntity(TetherEntityId);
    m_tether.phase=Phase::TetherRecoverable;++m_tether.phaseRevision;
    m_tether.deadline=m_now+m_profile.recoveryWindow;++m_stateVersion;return true;
}

bool Runtime::awaitingTetherPresentation() const
{
    return m_tether.exists && m_tether.phase==Phase::TetherAwaitingPresentation;
}

bool Runtime::crossConfirmation(unsigned ordinal,unsigned slot)
{
    if(!active() || ordinal>=Profile::MaxConfirmations || slot>=m_slots.size() ||
       m_confirmations.test(ordinal)) return false;
    m_confirmations.set(ordinal);++m_stateVersion;
    if(!m_slots[slot].everAcquired || m_slots[slot].beaconLit) return false;
    m_slots[slot].beaconLit=true;return true;
}

bool Runtime::skipConfirmation(unsigned ordinal)
{
    if(!active() || ordinal>=Profile::MaxConfirmations || m_confirmations.test(ordinal))
        return false;
    m_confirmations.set(ordinal);++m_stateVersion;return true;
}

void Runtime::freezeResult(const std::string& reason,uint64_t sequence)
{
    m_result.valid=true;m_result.raceEpoch=m_race;m_result.missionRevision=m_revision;
    m_result.finishSequence=sequence;m_result.finishTick=m_now;
    m_result.slots=m_slots;m_result.securedCount=securedCount();
    m_result.cargoCompleteAtFinish=m_result.securedCount==Profile::SlotCount;
    m_result.terminationReason=reason;suspendInput();++m_stateVersion;
}

bool Runtime::freezeNaturalFinish(unsigned position,double time,uint64_t sequence)
{
    if(!active() || !sequence || position<1 || position>4 ||
       !std::isfinite(time) || time<0) return false;
    freezeResult("natural_finish",sequence);m_result.natural=true;
    m_result.officialTeamRacePosition=position;m_result.nativeFinishTime=time;
    if(position==1)
        m_result.outcome=m_result.cargoCompleteAtFinish?"mission_victory":"race_won_cargo_incomplete";
    else
        m_result.outcome=m_result.cargoCompleteAtFinish?"cargo_delivered_race_lost":"mission_failed";
    return true;
}

bool Runtime::abort(const std::string& reason,uint64_t sequence)
{
    if(!configured() || m_result.valid || !sequence || !identifier(reason) ||
       reason=="natural_finish") return false;
    freezeResult(reason,sequence);m_result.outcome="not_completed";return true;
}

Snapshot Runtime::snapshot(uint64_t entity) const
{
    Snapshot result;
    if(entity==TetherEntityId)
    {
        result.kind=Kind::Tether;
        if(!m_tether.exists) return result;
        result.id=TetherEntityId;result.instance=m_tether.instance;
        result.module=m_tether.module;result.phaseRevision=m_tether.phaseRevision;
        result.phase=m_tether.phase;result.deadline=m_tether.deadline;
        result.owner=m_tether.owner;result.slot=m_tether.slot;
        result.eligible=active() && m_tether.phase==Phase::TetherRecoverable &&
                        m_now<m_tether.deadline;
        if(m_tether.owner>=0)
            result.progress=std::min(1.0,double(m_now-m_tether.started)/m_profile.recover);
        return result;
    }
    const Capsule* c=capsule(entity);
    if(!c) return result;
    result.id=c->spec.id;result.instance=c->spec.instance;result.module=c->spec.module;
    result.phaseRevision=c->phaseRevision;result.phase=c->phase;result.deadline=c->spec.expires;
    result.owner=c->owner;result.eligible=active() &&
        (c->phase==Phase::Shell || c->phase==Phase::Open) && c->spec.expires>m_now;
    if(c->owner>=0) result.progress=std::min(1.0,double(m_now-c->started)/m_profile.collect);
    return result;
}

std::vector<Snapshot> Runtime::snapshots() const
{
    std::vector<Snapshot> result;result.reserve(3);
    for(std::size_t i=0;i<m_capsuleCount;++i)
        if(m_capsules[i].phase==Phase::Shell || m_capsules[i].phase==Phase::Open)
            result.push_back(snapshot(m_capsules[i].spec.id));
    if(m_tether.exists) result.push_back(snapshot(TetherEntityId));
    return result;
}

std::vector<Feedback> Runtime::feedback() const
{
    std::vector<Feedback> result;result.reserve(m_crew.size());
    for(unsigned i=0;i<m_crew.size();++i) result.push_back(m_tools[i].feedback);
    return result;
}

const char* Runtime::statusReason(Status status)
{
    switch(status)
    {
    case Status::None:return "none";
    case Status::ReleaseRequired:return "release_required";
    case Status::TargetBusy:return "target_busy";
    case Status::CargoFull:return "cargo_full";
    case Status::ShellOpened:return "shell_opened";
    case Status::UseTool:return "use_tool";
    case Status::ModuleCollected:return "module_collected";
    case Status::ModuleRecovered:return "module_recovered";
    case Status::ModuleLost:return "module_lost";
    case Status::Rejected:return "rejected";
    case Status::OccurrenceSkipped:return "occurrence_skipped";
    }
    return "rejected";
}
}
