// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_INPUT_HPP
#define HEADER_SCINTILLA_INPUT_HPP
#include "modes/immersive_relay/scintilla_combat.hpp"
#include "modes/immersive_relay/scintilla_defense.hpp"
#include "modes/immersive_relay/scintilla_mission.hpp"
#include "modes/immersive_relay/scintilla_crew.hpp"
#include "modes/immersive_relay/scintilla_tuning.hpp"
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Single game/render-thread consumer. Transport must enter through the existing
// authenticated Relay command queue; none of these methods performs network I/O.
namespace ScintillaSpatial
{
// Absolute consumer resolution window; GPU cache retention must cover it too.
constexpr int64_t PendingTimeoutUs=250000;
struct Vec3
{
    double x, y, z;
    Vec3(double a=0, double b=0, double c=0): x(a), y(b), z(c) {}
};
struct Projection
{
    std::string kind;
    unsigned width=0, height=0;
    double domeTiltDeg=0;
};
enum class TargetKind { Rival=0, DiagnosticPickup=1, MissionCapsule=2, MissionTether=3 };
struct Target
{
    TargetKind kind=TargetKind::Rival;
    uint64_t id=0, instance=0;
    Vec3 center;
    double radius=0;
    ScintillaCombat::Snapshot combat;
    ScintillaMission::Snapshot mission;
    // SC1 registers rendered marker primitives; no invisible aim-assist volumes.
};
struct Frame
{
    std::string stream;
    uint64_t id=0, generation=0, queryOwner=0;
    int64_t captureUs=0;
    std::string routeRevision, projectionContextId;
    Projection projection;
    Vec3 cameraPosition, cameraRight, cameraUp, cameraForward;
    double nearPlane=0.1, farPlane=1000, faceFovDeg=90;
    // Combat range rules of this frame, applied to its historical targets.
    double combatMaxRange=30, combatMinAngularSine=std::sin(0.75*3.14159265358979323846/180.0);
    std::vector<Target> targets;
    ScintillaDefense::View defense;
    ScintillaDefense::Layout layout;
    bool crewEnabled=false,partyEnabled=false;
    uint64_t crewRaceEpoch=0,crewDriverTurnId=0,crewIntentRevision=0,crewProposalId=0,crewInputEpoch=0,crewRosterRevision=0;
    ScintillaCrew::PartyView crewParty;
    std::vector<ScintillaCrew::Assignment> crewAssignments;
    std::vector<ScintillaCrew::Suggestion> crewSuggestions;
    std::vector<ScintillaCrew::ReleaseAdvice> crewReleaseAdvice;
    bool missionEnabled=false;
    uint64_t missionRaceEpoch=0,missionRevision=0;
    int64_t missionTick=0;
    std::array<ScintillaMission::Slot,ScintillaMission::Profile::SlotCount> missionSlots;
    uint64_t missionHumanInstance=0;
    std::array<Vec3,2> missionTetherCandidates;
    Vec3 missionCableStart,missionCableEnd;
    double missionCableRadius=0;
    bool missionHUDEnabled=false,missionResultAtlasReady=false,missionDecorations=false;
    double missionTetherSeconds=-1;
    ScintillaMission::Result missionResult;
    std::array<Vec3,3> missionBeacons,missionNucleus;
    Vec3 defenseSource;double defenseSourceRadius=0;bool defenseSourceLineClear=false;
    // Only immutable pending candidate copies own a producer visibility lease.
    std::shared_ptr<void> visibilityLease;
};
enum class Visibility { Pending, Visible, Occluded, Unavailable };
enum class FeedbackKind { None=0, Pending=1, Hit=2, Miss=3, Rejected=4, Contribution=5, Stagger=6, Rebound=7 };
struct Feedback
{
    FeedbackKind kind=FeedbackKind::None;
    double u=0.5,v=0.5;
    // False renders a source badge, never an invented spatial hit location.
    bool hasPoint=false;
    int64_t startedUs=0,untilUs=0;
    uint64_t actionSequence=0;
    std::string reason;
};
struct Reticle
{
    std::string sourceId, participantId, result;
    unsigned slot=0;
    double u=0.5, v=0.5;
    bool valid=false, primaryHeld=false, toolHeld=false;
    int64_t updatedUs=0;
    uint64_t toolEpoch=0;
    Feedback feedback;
    ScintillaCombat::ToolSnapshot toolFeedback;
    ScintillaDefense::ToolFeedback defenseFeedback;
    ScintillaMission::Feedback missionFeedback;
    ScintillaCrew::PartyFeedback partyFeedback;
    TargetKind targetKind=TargetKind::Rival;
    ScintillaDefense::Surface intent;
};
struct BindingState {
    std::string sourceId,participantId,deviceBindingRevision,trackingSpaceEpoch,poseContinuityId,roomCalibrationRevision,gripCalibrationRevision;
    uint64_t trackingLossRevision=0;bool tracked=false,neutral=false;
    // Native-only: a loss/recovery cannot validate a possession gesture from
    // this input epoch. Only a genuine new prepare/rebind resets the baseline.
    bool continuityIntact=false;
};
struct HitEvent
{
    TargetKind kind=TargetKind::Rival;
    std::string sourceId, participantId, eventId, control, result;
    uint64_t targetId=0, targetInstance=0, frameId=0;
    int64_t evaluatedUs=0;
    uint64_t serial=0, raceEpoch=0, inputEpoch=0, actionSequence=0;
    int64_t admittedTick=0;
    std::vector<ScintillaCombat::Snapshot> admittedTargets;
    ScintillaCombat::Snapshot historical;
    std::vector<ScintillaMission::Snapshot> admittedMissionTargets;
    ScintillaMission::Snapshot historicalMission;
    uint64_t missionRevision=0;
    bool historicalEligible=false, historicalOpen=false;
};
struct BeamProof {
    std::string sourceId, participantId;
    uint64_t target=0, instance=0, controlEpoch=0;
    bool held=false, visible=false;
};
struct PresentedFrame {Frame frame;int64_t low=0,high=0;};
struct DefenseAdmission {std::string evidence;std::vector<double> regions;};
struct Reply
{
    int status;
    std::string json;
    Reply(int s=200, const std::string& j="{}"):status(s),json(j){}
};
class Runtime
{
public:
    typedef std::function<Visibility(const Frame&, const Target&, const Vec3&, double)> VisibilityOracle;
    typedef std::function<bool(uint64_t, uint64_t)> LiveTargetOracle;
    typedef std::function<std::shared_ptr<void>(const Frame&)> FramePinOracle;
    typedef std::function<std::shared_ptr<void>(uint64_t)> QueryLeaseOracle;
    Runtime();
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Reply route(const std::string& path, const std::string& body, int64_t nowUs);
    void addFrame(const Frame& frame);
    // Lifecycle revision catches a pause/resume pair between two render/input ticks.
    // Without controller evidence, standalone consumers still gate observed resumes.
    void tick(int64_t nowUs, bool gameplayActive,
              uint64_t resumeRevision=0, int64_t resumedAtUs=0);
    void reset(const std::string& reason);
    // Same producer/route: revoke gameplay authority, retain the live SI5 grant.
    bool resetForRace();
    const std::vector<Reticle>& reticles() const;
    const std::deque<HitEvent>& events() const;
    void setVisibilityOracle(const VisibilityOracle& oracle);
    void setFramePinOracle(const FramePinOracle& oracle);
    void setQueryLeaseOracle(const QueryLeaseOracle& oracle);
    void setLiveTargetOracle(const LiveTargetOracle& oracle);
    void setProducerProjection(const Projection& projection);
    // Race tuning; applied to edges admitted from now on.
    void setTuning(const ScintillaTuning::Input& tuning);
    const ScintillaTuning::Input& tuning() const;
    // Authoritative Relay crew IDs, excluding the current driver. A change
    // invalidates admission; SC3 can retain its physical grant for an explicit
    // held-world logical rebind of the SAME sources. Tracking loss changes no crew.
    void setParticipants(const std::vector<std::string>& allowedCrewIDs);
    std::string stateJSON() const;
    Reply validateCombatPrepare(const std::string& body,const std::string& raceEpoch,const std::string& rosterRevision) const;
    Reply validateCombatIntent(const std::string& body,const std::string& raceEpoch,const std::string& intentRevision,bool* required) const;
    // Explicit held-world SC4 preflight: temporarily disarm the same physical
    // grant without calling release/prepare or resetting sequence/replay guards.
    Reply inputMaintenance(const std::string& operation,const std::string& body,int64_t nowUs);
    bool inputMaintenanceActive() const;
    bool inputMaintenanceMatches(const std::string& operationID) const;
    bool hasPreparedGrant() const;
    bool prepared() const;
    void allowParticipantRebind(bool enabled);
    Reply rebindParticipants(const std::string&,const std::string& rosterRevision,int64_t nowUs);
    uint64_t inputEpoch() const;
    const std::string& inputSessionID() const;
    std::vector<std::string> participants() const;
    std::vector<BindingState> bindingStates() const;
    void setCombatContext(bool enabled, int64_t simulationTick,
                          const std::vector<ScintillaCombat::Snapshot>& targets, uint64_t raceEpoch=0);
    void setMissionContext(bool enabled,uint64_t revision,
                           const std::vector<ScintillaMission::Snapshot>& targets,
                           const std::vector<ScintillaMission::Feedback>& feedback);
    // Intent-owned: SC3 preparation needs the SC4 masks before mission.prepare.
    // A change disarms controls; it never reinterprets a historical layout.
    void setMissionLayoutRequired(bool required);
    void setCrewContext(bool required,bool partyEnabled,uint64_t race,uint64_t turn,
                        uint64_t intent,uint64_t proposal,uint64_t rosterRevision);
    std::vector<ScintillaCrew::PartyBeam> partyBeams(int64_t nowUs) const;
    void setPartyFeedback(const std::vector<ScintillaCrew::PartyFeedback>&);
    // Empty identity/epoch0 means no current tool feedback. Shared by source
    // JSON and the producer; never reuse progress after UP, lost point or epoch.
    static ScintillaCrew::PartyFeedback partyFeedbackForRendering(const Reticle&);
    std::vector<HitEvent> drainCombatShots();
    // Observational read. A resolved loss/target change after a positive proof
    // stays invisible for this press, through any reads and newer positive aims.
    // Only an admitted secondary release/press can establish a new beam.
    std::vector<BeamProof> beamProofs(int64_t nowUs) const;
    std::vector<ScintillaMission::Beam> missionBeams(int64_t nowUs) const;
    std::vector<PresentedFrame> presentedMissionFrames(int64_t nowUs) const;
    static ScintillaMission::Feedback missionFeedbackForRendering(const Reticle&);
    std::vector<ScintillaDefense::Beam> defenseBeams(int64_t nowUs) const;
    std::vector<PresentedFrame> presentedDefenseFrames(int64_t nowUs,bool allowHeld=false) const;
    bool defenseVideoReady(int64_t nowUs,bool allowHeld=false) const;
    void setDefenseContext(bool enabled,const std::vector<ScintillaDefense::ToolFeedback>&);
    Reply validateDefenseIntent(const std::string&,const std::string& race,const std::string& revision,bool*) const;
    Reply validateDefensePrepare(const std::string&,const std::string& race,const std::string& roster,DefenseAdmission*,bool missionLayout=false) const;
    // Published after authoritative update/effects. Matched by participant and
    // tool epoch; an intervening input loss has precedence over core feedback.
    void setCombatToolFeedback(const std::vector<ScintillaCombat::ToolSnapshot>&);
    static ScintillaCombat::ToolSnapshot toolFeedbackForRendering(const Reticle&);
    static const char* toolFeedbackReason(ScintillaCombat::ToolStatus);
    static const char* toolFeedbackInstruction(ScintillaCombat::ToolStatus);
    void combatFeedback(const HitEvent&, FeedbackKind, const std::string&, int64_t nowUs);
    const std::string& activeRouteRevision() const;
    const std::string& activeProjectionContextId() const;
    // Public for regression fixtures, exactly the producer shader's top-left UV.
    // Defensive expiry for rendering even when no active-world tick occurred.
    static Feedback feedbackForRendering(const Reticle& reticle, int64_t nowUs);
    static bool projectionDirection(const Projection& projection, double u, double v, Vec3* direction);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
}
#endif
