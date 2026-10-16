// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_MISSION_ANCHORS_HPP
#define HEADER_SCINTILLA_MISSION_ANCHORS_HPP
#include "modes/immersive_relay/scintilla_mission.hpp"
#include <array>
#include <bitset>
#include <string>
#include <vector>

namespace ScintillaMissionAnchors
{
struct Position {double x=0,y=0,z=0;};
struct Capsule {Position center;double radius=2;std::string asset;};
struct Group
{
    std::string id;
    unsigned activationNode=0,expiryNode=0;
    double activationFraction=0,expiryFraction=0;
    std::vector<unsigned> laps; // Native official lap index: 0..11.
    std::vector<Capsule> capsules; // One or two, with disjoint spheres.
};
struct Confirmation
{
    unsigned slot=0,node=0;
    double fraction=0;
    std::vector<unsigned> laps;
};
struct Manifest
{
    unsigned schemaVersion=1;
    uint64_t profileRevision=1;
    std::string profileId="sc4_cargo_three_slots_v1",trackId="snowtuxpeak";
    // SHA-256 of exact quads.xml bytes followed by exact graph.xml bytes.
    std::string trackHash,authoringStatus="development_unqualified",author,license;
    bool reverse=false;
    std::vector<Group> groups;
    std::array<Confirmation,ScintillaMission::Profile::SlotCount> confirmations;
};
struct NativeTrack
{
    std::string trackId,trackHash;
    bool reverse=false;
    double lapLength=0;
    std::vector<double> nodeFractions;
};
// Bounded shared JSON parser, closed schemas, finite coordinates and dimensions.
// No I/O, native pointers, GL or qualification claims. Failure leaves out intact.
bool load(const std::string& json,Manifest* out,std::string* reason);
bool validateNative(const Manifest&,const NativeTrack&,std::string* reason);

enum class Kind {Activation=0,Expiry=1,Confirmation=2};
struct Crossing
{
    Kind kind=Kind::Activation;
    unsigned group=0,capsule=0,slot=0,ordinal=0,lap=0;
    double fraction=0;
    bool skipped=false;
};
class Traversal
{
public:
    // Exact reprepare preserves every receipt. Different configuration requires
    // explicit reset at a new race epoch, never an implicit mid-race restart.
    bool prepare(const Manifest&,unsigned totalLaps,unsigned crew=1);
    void reset();
    // officialLap==-1 means before the first native start-line crossing.
    // A discontinuous segment consumes passed occurrences without rewarding.
    // Expiry crossings still retire existing entities, even when skipped.
    std::vector<Crossing> update(int officialLap,double fraction,bool continuous);
    bool prepared()const{return m_prepared;}
    bool lastUpdateValid()const{return m_valid;}
    const std::string& reason()const{return m_reason;}
    unsigned totalLaps()const{return m_totalLaps;}
    std::size_t opportunityReceiptCount()const{return m_activated.count();}
    std::size_t confirmationReceiptCount()const{return m_confirmed.count();}
private:
    struct Point {Crossing crossing;double progress=0;};
    Manifest m_manifest;
    bool m_prepared=false,m_haveSample=false,m_valid=true;
    unsigned m_totalLaps=0,m_crew=0;
    int m_previousLap=-1;
    double m_previousFraction=0,m_highWater=0;
    std::string m_reason="unprepared";
    std::vector<Point> m_points;
    std::bitset<ScintillaMission::Profile::MaxOpportunities> m_activated,m_expired;
    std::bitset<ScintillaMission::Profile::MaxConfirmations> m_confirmed;
};
}
#endif
