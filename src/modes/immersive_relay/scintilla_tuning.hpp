// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_TUNING_HPP
#define HEADER_SCINTILLA_TUNING_HPP
#include <string>
#include <vector>

// Balance parameters of one race. Pure C++11: no STK, file or clock access.
// Built-in defaults reproduce the qualified SC0-SC4 constants exactly. A
// tholus.stk-scintilla-tuning/1 profile overrides selected values; the engine
// loads it with each new race and keeps it fixed until that race ends.
namespace ScintillaTuning
{
enum class BoostPolicy { Cancel=0, Pause=1, Ignore=2 };
enum class HitTest { Pose=0, DrawnReticle=1 };

struct Combat
{
    double markerRadiusM=0.35, maxRangeM=30, minAngularRadiusDeg=0.75;
    double acquireS=0.6, breachS=3.5, cooperationS=1.25;
    double soloGapS=2.5, soloTotalS=8, staggerS=1, recoveryS=4;
};
struct Input
{
    double fireIntervalS=0.25, autoFireIntervalS=0, beamGraceS=0;
    bool fireRateFeedback=true;
    HitTest hitTest=HitTest::Pose;
};
struct Defense
{
    double telegraphSoloS=3, telegraphCrewS=2.5, holdS=0.6, marginS=0.25;
    double gapS=8, attackerGapS=12, technicalRetryS=0, protectionS=5;
    double boostAcquireS=0.5, boostCooldownS=6, boostCost=25, activationCost=5;
    double counterCostPerS=20, maintainCostPerS=2, counterHeatPerS=25;
    double maintainHeatPerS=2, regenPerS=5, coolingPerS=20;
    double minDistanceM=8, maxDistanceM=24, projectileSpeed=40;
    double minFlightS=0.35, maxFlightS=0.9, impactSpeedRetained=0.85, maxPauseS=1.5;
    bool inheritAttackerVelocity=false, launchSolutionDuringTelegraph=true;
    BoostPolicy boostPolicy=BoostPolicy::Cancel;
};
struct Mission
{
    double collectS=0.6, recoverS=0.6, recoveryWindowS=3;
    bool withdrawAboveQualifiedSpeed=true;
    std::vector<std::string> disabledGroups;
};
struct Diagnostics
{
    bool latencyProbe=false;
    double latencyFlashS=0.1;
};
struct Tuning
{
    std::string profileId="builtin_sc4";
    Combat combat;
    Input input;
    Defense defense;
    Mission mission;
    Diagnostics diagnostics;
};
// One documented parameter: numeric ranges are closed; booleans use 0/1 and
// choices list the accepted strings of an enumeration.
struct Parameter
{
    std::string path, unit, description, value;
    double minimum=0, maximum=0;
    std::vector<std::string> choices;
};
extern const char* const Contract;
// Strict document: unknown sections/keys, wrong types, out-of-range values or
// inconsistent combinations are rejected with "tuning:<path>:<reason>".
bool parse(const std::string& bytes, Tuning* out, std::string* error);
// Cross-field limits, also satisfied by the built-in defaults.
bool valid(const Tuning& tuning, std::string* error);
std::string effectiveJSON(const Tuning& tuning);
std::vector<Parameter> parameters(const Tuning& tuning);
std::string parametersJSON(const Tuning& tuning);
const char* name(BoostPolicy policy);
const char* name(HitTest mode);
}
#endif
