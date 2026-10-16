// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_ZONES_HPP
#define HEADER_SCINTILLA_CREW_ZONES_HPP
#include <cstdint>
#include <string>
#include <vector>
namespace ScintillaCrewZones {
struct Zone {std::string id;unsigned startNode=0,endNode=0;double start=0,end=0,width=0;};
struct Manifest {
    std::string trackId,trackHash,status,author,license;
    bool reverse=false;
    double speedBound=0,margin=0,positionError=0,finalExclusion=0;
    std::vector<unsigned> candidates;
    std::vector<Zone> zones;
};
// No file access or native pointers. Qualification is a separate, hash-bound
// native evidence report; authoring candidates never become operating zones.
// "authored" freezes interval data BEFORE native capture. The report grants
// qualification without rewriting the manifest (and invalidating its hash).
bool load(const std::string&,Manifest*,std::string* reason);
bool validateNative(const Manifest&,const std::string& trackId,const std::string& trackHash,
                    bool reverse,double lapLength,const std::vector<double>& nodeDistances,std::string* reason);
bool qualify(const Manifest&,const std::string& report,const std::string& manifestSHA256,
             const std::string& evidenceSHA256,const std::string& nativeBuildSHA256,
             const std::string& trackAssetsSHA256,int physicsFPS,std::string* reason);
// Native/operator observations are separate from mathematical authoring. Each
// required controller check names a hash-bound native capture, verified by the
// owning adapter before it admits the qualification. This validates declared
// provenance/integrity; it cannot establish physical comfort or operator truth.
bool nativeEvidence(const Manifest&,const std::string& bytes,const std::string& manifestSHA256,
                    const std::string& nativeBuildSHA256,const std::string& trackAssetsSHA256,
                    int physicsFPS,std::vector<std::string>* artifactHashes,std::string* reason);
double guardDistance(const Manifest&,double forwardExtent);
struct Sample {int64_t tick=0;int lap=-1;double distance=0,x=0,y=0,z=0,speed=0;bool onRoad=false,animated=false;};
class Tracker {
    bool m_known=false,m_continuous=false;Sample m_previous;
public:
    void reset(){m_known=m_continuous=false;}
    bool update(const Sample&,int fps,double lapLength,double speedBound);
    bool available(const Manifest&,double lapLength,unsigned totalLaps,double forwardExtent)const;
};
}
#endif
