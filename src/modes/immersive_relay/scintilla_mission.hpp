// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_MISSION_HPP
#define HEADER_SCINTILLA_MISSION_HPP

#include <array>
#include <bitset>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

// Pure mission transactions. The adapter alone establishes optical proofs,
// live geometry, crossing provenance and the native natural-finish barrier.
// No renderer, physics, filesystem, transport or global clock is accessed here.
namespace ScintillaMission
{
using Tick = int64_t;
enum class Kind { Capsule=0, Tether=1 };
enum class Phase { Shell=0, Open=1, Collected=2, Expired=3,
                   TetherAwaitingPresentation=4, TetherRecoverable=5 };
enum class SlotState { Empty=0, Secured=1, Tethered=2 };
enum class Status { None=0, ReleaseRequired=1, TargetBusy=2, CargoFull=3,
                    ShellOpened=4, UseTool=5, ModuleCollected=6,
                    ModuleRecovered=7, ModuleLost=8, Rejected=9,
                    OccurrenceSkipped=10 };

struct Profile
{
    enum { MaxCrew=5, SlotCount=3, MaxOpportunities=288, MaxConfirmations=36 };
    int fps=0;
    Tick collect=0, recover=0, recoveryWindow=0;
    static bool create(int fps, Profile* out);
    static bool inRange(Kind kind, double distanceSquared, double radius);
};

struct Slot
{
    SlotState state=SlotState::Empty;
    uint64_t module=0;
    bool everAcquired=false, beaconLit=false;
};

struct CapsuleSpec
{
    uint64_t id=0, instance=0, module=0;
    unsigned occurrence=Profile::MaxOpportunities;
    Tick expires=0;
};

struct Snapshot
{
    Kind kind=Kind::Capsule;
    uint64_t id=0, instance=0, module=0, phaseRevision=0;
    Phase phase=Phase::Expired;
    Tick deadline=0;
    int owner=-1, slot=-1;
    double progress=0;
    bool eligible=false;
};

struct Shot
{
    uint64_t race=0, revision=0, inputEpoch=0, serial=0;
    uint64_t entity=0, instance=0, phaseRevision=0;
    std::string participant, event;
    bool historicalEligible=false, admittedEligible=false;
};

struct Beam
{
    uint64_t race=0, revision=0, inputEpoch=0;
    uint64_t entity=0, instance=0, phaseRevision=0, controlEpoch=0;
    // Serial of this tool press, retained unchanged across its aim samples.
    uint64_t serial=0;
    // Latest admitted secondary release in this input epoch. This proves an
    // actual release even when release+press both arrive between gameplay ticks.
    uint64_t releaseSerial=0;
    Kind kind=Kind::Capsule;
    std::string participant;
    bool held=false, visible=false, historicalEligible=false;
    // Sticky SC1 interruption: a later hit in the same press cannot repair it.
    bool interrupted=false;
};

struct Feedback
{
    std::string participant;
    uint64_t controlEpoch=0, entity=0;
    int owner=-1;
    Status status=Status::None, outcome=Status::None;
};

struct Outcome
{
    bool accepted=false, changed=false;
    Status status=Status::Rejected;
    uint64_t entity=0, module=0;
    int slot=-1;
};

struct Impact { uint64_t race=0, threat=0, humanInstance=0; };

struct Result
{
    bool valid=false, natural=false, cargoCompleteAtFinish=false;
    uint64_t raceEpoch=0, missionRevision=0, finishSequence=0;
    Tick finishTick=0;
    double nativeFinishTime=0;
    unsigned officialTeamRacePosition=0, securedCount=0;
    std::array<Slot, Profile::SlotCount> slots;
    std::string terminationReason, outcome;
};

class Runtime
{
public:
    // Disjoint from native kart IDs and diagnostic pickups. IDs stay integers;
    // shader array slots are never mission identities.
    static const uint64_t TetherEntityId;
    static const uint64_t CapsuleEntityBase;
    bool setRequired(bool value);
    bool required() const { return m_required || configured(); }
    uint64_t intentRevision() const { return m_intentRevision; }
    bool permitsStart(bool prerequisitesReady) const
    { return !required() || (configured() && !m_result.valid && prerequisitesReady); }
    bool prepare(uint64_t race, uint64_t revision, uint64_t inputEpoch,
                 uint64_t humanInstance, const std::vector<std::string>& crew,
                 int fps);
    void reset(bool preserveIntent=true);
    void suspendInput();
    // Called before native physics. Active time advances by at most one tick;
    // inactive calls must retain the current tick. False means adapter must hold.
    bool advanceTo(Tick tick, bool active);
    Outcome activateCapsule(const CapsuleSpec& spec);
    bool skipOccurrence(unsigned ordinal);
    bool expireCapsule(uint64_t entity, uint64_t instance);
    Outcome openCapsule(const Shot& shot);
    // Exactly one current sample per participant. Omitted samples invalidate
    // that participant's hold. All releases/interruptions precede admissions.
    void updateBeams(const std::vector<Beam>& beams);
    Outcome consumeImpact(const Impact& impact);
    bool confirmTetherPresented(uint64_t entity, uint64_t instance,
                                uint64_t phaseRevision);
    // The adapter supplies a unique physical crossing ordinal, in path order.
    // Even ineligible crossings consume their occurrence, preventing replay.
    bool crossConfirmation(unsigned ordinal, unsigned slot);
    bool skipConfirmation(unsigned ordinal);
    // Only the native finish adapter can establish that this is a natural
    // finish. Artificial termination uses abort and never receives a position.
    bool freezeNaturalFinish(unsigned officialPosition, double nativeFinishTime,
                             uint64_t finishSequence);
    bool abort(const std::string& reason, uint64_t finishSequence);

    bool configured() const { return m_race!=0; }
    bool active() const { return m_active && !m_result.valid; }
    uint64_t race() const { return m_race; }
    uint64_t revision() const { return m_revision; }
    uint64_t inputEpoch() const { return m_input; }
    uint64_t humanInstance() const { return m_humanInstance; }
    uint64_t stateVersion() const { return m_stateVersion; }
    Tick tick() const { return m_now; }
    const Profile& profile() const { return m_profile; }
    const std::vector<std::string>& crew() const { return m_crew; }
    const std::array<Slot,Profile::SlotCount>& slots() const { return m_slots; }
    const Result& result() const { return m_result; }
    Snapshot snapshot(uint64_t entity) const;
    std::vector<Snapshot> snapshots() const;
    std::vector<Feedback> feedback() const;
    bool awaitingTetherPresentation() const;
    unsigned securedCount() const;
    unsigned occupiedCount() const;
    std::size_t opportunityReceiptCount() const { return m_occurrences.count(); }
    std::size_t confirmationReceiptCount() const { return m_confirmations.count(); }
    static const char* statusReason(Status status);

private:
    struct Capsule
    {
        CapsuleSpec spec;
        Phase phase=Phase::Shell;
        uint64_t phaseRevision=1, openSerial=0, openInput=0;
        std::string openParticipant, openEvent;
        int owner=-1;
        Tick started=0, lastSample=0;
    };
    struct Tether
    {
        bool exists=false;
        uint64_t instance=0, phaseRevision=0, module=0;
        Phase phase=Phase::TetherAwaitingPresentation;
        int slot=-1, owner=-1;
        Tick deadline=0, started=0, lastSample=0;
    };
    struct Tool
    {
        uint64_t epoch=0, serial=0, entity=0, instance=0, phaseRevision=0;
        uint64_t lastPressSerial=0;
        Kind kind=Kind::Capsule;
        bool held=false, blocked=false, mustRelease=false;
        Feedback feedback;
    };
    bool m_required=false, m_active=false;
    uint64_t m_intentRevision=0, m_race=0, m_revision=0, m_input=0;
    uint64_t m_humanInstance=0, m_lastImpact=0, m_tetherSequence=0;
    uint64_t m_lastShotSerial=0, m_stateVersion=0;
    Tick m_now=0;
    Profile m_profile;
    std::vector<std::string> m_crew;
    std::array<Slot,Profile::SlotCount> m_slots;
    std::array<Capsule,Profile::MaxOpportunities> m_capsules;
    std::size_t m_capsuleCount=0;
    std::array<Tool,Profile::MaxCrew> m_tools;
    std::bitset<Profile::MaxOpportunities> m_occurrences;
    std::bitset<Profile::MaxConfirmations> m_confirmations;
    Tether m_tether;
    Result m_result;
    int participantSlot(const std::string&) const;
    Capsule* capsule(uint64_t entity);
    const Capsule* capsule(uint64_t entity) const;
    bool sameContext(uint64_t race,uint64_t revision,uint64_t input) const;
    void closeTool(unsigned tool, bool releaseRequired);
    void invalidateEntity(uint64_t entity);
    void expire();
    void freezeResult(const std::string& reason,uint64_t sequence);
    bool validBeam(const Beam& beam) const;
    void processBeam(const Beam& beam,unsigned tool);
    void completeCapsule(Capsule& capsule,unsigned tool);
    void completeTether(unsigned tool);
};
}
#endif
