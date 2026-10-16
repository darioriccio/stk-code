// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_COMBAT_HPP
#define HEADER_SCINTILLA_COMBAT_HPP
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Pure, bounded, authoritative rules. No graphics, transport or physics access.
namespace ScintillaCombat {
using Tick = int64_t;
enum class Phase { Shielded=0, Acquiring=1, Breached=2, StaggerPending=3, Stagger=4, Recovery=5 };
struct Profile {
    Tick acquire=0, breach=0, cooperation=0, soloGap=0, soloTotal=0, stagger=0, recovery=0;
    static bool create(int fps, Profile* out);
    static bool inRange(double distanceSquared, double radius);
    static double staggerCurve(double u);
};
// Local-body bounds include compound child transforms, scaling and margins.
// This fixed-orientation box contains every permitted local-X pitch pose.
struct StaggerEnvelope {
    static constexpr double MinimumSupportAlignment=0.5;
    double center[3]={0,0,0}, halfExtents[3]={0,0,0};
    double pitch=0, radius=0;
    static bool create(const double lower[3],const double upper[3],
                       double lift,double maximumPitch,StaggerEnvelope* out);
    static bool supportingFace(double normalUp,double contactHeight,double bodyBottom);
};
struct Snapshot {
    uint64_t target=0, instance=0, window=0;
    Phase phase=Phase::Shielded;
    unsigned count=0, required=0, contributors=0;
    int opener=-1;
    double progress=0;
    Tick deadline=0;
    bool eligible=false;
};
struct Beam {
    std::string participant;
    uint64_t target=0, instance=0, controlEpoch=0;
    bool held=false, visible=false;
};
// Persistent source feedback, independent of transient shot outcomes.
// Numeric values are shared with the projection shader; never renumber.
enum class ToolStatus { None=0, ReleaseRequired=1, TargetBusy=2 };
struct ToolSnapshot {
    std::string participant;
    uint64_t controlEpoch=0, target=0;
    ToolStatus status=ToolStatus::None;
    int owner=-1; // Crew index, resolved to a source slot by the adapter.
};
struct Shot {
    uint64_t serial=0, race=0, inputEpoch=0, target=0, instance=0, window=0;
    Tick admitted=0;
    std::string participant, event;
    bool historicalEligible=false, historicalOpen=false, admittedOpen=false;
};
struct Effect {
    uint64_t race=0, inputEpoch=0, target=0, instance=0, window=0;
};
struct Outcome {
    std::string reason;
    bool contributed=false, requestEffect=false;
    Effect effect;
};
class Runtime {
public:
    bool prepare(uint64_t race, uint64_t input, const std::vector<std::string>& crew, int fps);
    void reset(bool preserveIntent=false);
    bool setRequired(bool required);
    uint64_t intentRevision() const { return m_intentRevision; }
    bool required() const { return m_required || configured(); }
    bool permitsStart(bool inputReady) const { return !required() || (configured() && inputReady); }
    void suspendInput();
    void observeTarget(uint64_t id, uint64_t instance, bool compatible);
    void retireTarget(uint64_t id);
    // Called once per completed simulation tick (or held tick with active=false).
    void update(Tick now, bool active, const std::vector<Beam>& beams);
    Outcome shoot(const Shot& shot);
    bool confirmEffect(const Effect& effect, bool applied);
    void animationFinished(uint64_t id, uint64_t instance);
    Snapshot snapshot(uint64_t id) const;
    std::vector<Snapshot> snapshots() const;
    std::vector<ToolSnapshot> toolSnapshots() const;
    bool configured() const { return m_race!=0; }
    uint64_t race() const { return m_race; }
    uint64_t inputEpoch() const { return m_input; }
    const Profile& profile() const { return m_profile; }
    const std::vector<std::string>& crew() const { return m_crew; }
private:
    struct Target {
        Snapshot view;
        Tick first=0, last=0, acquired=0, notBefore=0;
        std::string soloParticipant;
        std::vector<std::string> events;
        bool compatible=false;
    };
    struct Tool { uint64_t epoch=0, target=0, aimTarget=0, aimInstance=0; bool blocked=false; };
    uint64_t m_race=0, m_input=0, m_window=0, m_serial=0;
    Tick m_now=0;
    bool m_active=false;
    bool m_required=false;
    uint64_t m_intentRevision=0;
    int m_fps=0;
    Profile m_profile;
    std::vector<std::string> m_crew;
    std::map<uint64_t,Target> m_targets;
    std::map<std::string,Tool> m_tools;
    int slot(const std::string& participant) const;
    void close(Target& target);
    bool nextWindow(Target& target);
    void expire();
};
}
#endif
