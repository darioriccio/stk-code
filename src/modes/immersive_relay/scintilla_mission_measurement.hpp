// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_MISSION_MEASUREMENT_HPP
#define HEADER_SCINTILLA_MISSION_MEASUREMENT_HPP
#include "modes/immersive_relay/scintilla_mission_anchors.hpp"
#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include "modes/immersive_relay/scintilla_layout.hpp"
#include <deque>
#include <set>
#include <sstream>
#include <iomanip>
namespace ScintillaMissionMeasurement {
using namespace ScintillaJSON;
namespace Protocol=ScintillaMissionProtocol;
using Vec=ScintillaDefense::Vec;
static const unsigned MaxFrames=18000,MaxPending=8,MaxObjects=4,MaxTraces=8;
// A window qualifies when the kart holds at least this share of its native
// base speed throughout. Its proven bound then covers speeds up to
// bound/QualifiedSpeedShare, which is the runtime envelope.
static const double QualifiedSpeedShare=.95;
inline double qualifiedEnvelope(double bound){return bound/QualifiedSpeedShare;}
static const std::size_t MaxBytes=64u*1024u*1024u,MaxLine=65536;
static const int64_t MaxGapUs=50000,LiveProofMaxAgeUs=50000;
// A completed capsule query is usable only inside this exact availability
// interval. Capture continuity and live proof age are distinct contracts.
inline bool capsuleProofAvailable(int64_t capture,int64_t readback,int64_t at)
{return capture>=0&&readback>=capture&&at>=readback&&at-capture<=LiveProofMaxAgeUs;}
inline bool capsuleProofsOverlap(int64_t previousCapture,int64_t readback)
{return readback>=previousCapture&&readback-previousCapture<=LiveProofMaxAgeUs;}
inline uint64_t wide(const J& j){return std::stoull(Protocol::decimal(j));}
inline int64_t clock(const J& j){const auto n=wide(j);if(n>uint64_t(INT64_MAX))throw Invalid("measurement_clock_overflow");return int64_t(n);}
inline int signedInt(const J& j,int lo,int hi){const double n=real(j,lo,hi);if(std::floor(n)!=n)throw Invalid("measurement_integer");return int(n);}
inline bool boolean(const J& j){if(j.kind!=J::Bool)throw Invalid("measurement_boolean");return j.boolean;}
inline Vec vector(const J& j){const auto& a=arr(j,3,3);return Vec(real(a[0],-1e6,1e6),real(a[1],-1e6,1e6),real(a[2],-1e6,1e6));}
inline std::string vecJSON(const Vec& v){std::ostringstream o;o<<std::setprecision(17)<<'['<<v.x<<','<<v.y<<','<<v.z<<']';return o.str();}
inline bool close(const Vec&a,const Vec&b,double epsilon=1e-5){return (a-b).length()<=epsilon;}
// Conservative containing rectangle in the SAME output mapping, followed by
// exact revision-2 HUD intersection. Counts separately test every raster sample.
inline bool footprint(const std::string& projection,double tilt,unsigned width,unsigned height,
                      const Vec& center,double radius,unsigned revision=2)
{
    const double pi=3.14159265358979323846,d=center.length();if(!center.finite()||d<=radius)return false;
    const Vec unit=center*(1/d);const double alpha=std::asin(radius/d);double u,v,du,dv;
    if(projection=="equirectangular"){
        const double latitude=std::asin(std::max(-1.0,std::min(1.0,unit.y)));
        if(std::abs(latitude)+alpha>=pi*.5)return false;
        du=alpha/(2*pi*std::cos(std::abs(latitude)+alpha));dv=alpha/pi;
        u=.5+std::atan2(unit.x,unit.z)/(2*pi);v=.5-latitude/pi;
    }else if(projection=="fisheye_domemaster"){
        const double angle=-tilt*pi/180,c=std::cos(angle),s=std::sin(angle);
        const Vec p(unit.x,c*unit.y-s*unit.z,s*unit.y+c*unit.z);
        const double theta=std::acos(std::max(-1.0,std::min(1.0,p.z))),xy=std::hypot(p.x,p.y);
        if(theta+alpha>=pi*.5)return false;
        du=dv=alpha;u=.5+(xy?theta/(pi*.5)*p.x/xy:0)*.5;v=.5-(xy?theta/(pi*.5)*p.y/xy:0)*.5;
    }else return false;
    if(revision!=2&&revision!=3)return false;
    const auto layout=ScintillaDefense::Layout::create(width,height,true,false,revision==3);
    return layout.classify(u,v).context==ScintillaDefense::Context::WorldOffense&&layout.worldRectFree(u-du,v-dv,u+du,v+dv);
}
struct Bounds {
    std::array<double,288> capsules;std::array<double,12> tether;
    Bounds(){capsules.fill(0);tether.fill(0);}
};
// Capsule intervals start only when their first proof is actually available;
// end at the last CAPTURE in both clocks. Delay can reduce, never inflate, time.
// Tether geometry retains its independent capture-based three-second contract.
struct Window {
    struct Point{int64_t us,tick,availableUs,availableTick;double speed;uint64_t serial;};
    std::deque<Point> samples,minima;
    uint64_t serial=0;double best=0;int64_t lastUs=0,lastTick=0;
    void reset(){samples.clear();minima.clear();lastUs=lastTick=0;}
    void observe(int64_t us,int64_t tick,double speed,unsigned fps,bool tether,int64_t readbackUs=-1,int64_t readbackTick=-1)
    {
        if(lastUs&&(us-lastUs>MaxGapUs||tick<=lastTick))reset();
        if(readbackUs<0)readbackUs=us;if(readbackTick<0)readbackTick=tick;
        lastUs=us;lastTick=tick;Point p={us,tick,tether?us:readbackUs,tether?tick:readbackTick,speed,++serial};samples.push_back(p);
        while(!minima.empty()&&minima.back().speed>=speed)minima.pop_back();minima.push_back(p);
        const int64_t span=tether?3000000:2000000,ticks=int64_t(fps)*(tether?3:2);
        auto enough=[&](const Point& a){return tether?(us-a.availableUs>=span&&tick-a.availableTick>=ticks):(us-a.availableUs>span&&tick-a.availableTick>ticks);};
        while(samples.size()>1&&enough(samples[1])){const auto id=samples.front().serial;samples.pop_front();if(!minima.empty()&&minima.front().serial==id)minima.pop_front();}
        if(!samples.empty()&&enough(samples.front())&&!minima.empty())best=std::max(best,minima.front().speed);
    }
};
// Streaming verifier used by the native admission gate. It RECOMPUTES bounded
// windows from hashed raw traces; a report status alone can never promote data.
class Evidence {
    const ScintillaMissionAnchors::Manifest& manifest;
    std::string expectedFingerprint,producer;unsigned crew=0,laps=0,fps=0,width=0,height=0;
    double base=0,tilt=0;std::string projection,phase;unsigned layoutRevision=2;
    std::array<Window,576> capWindows;std::array<Window,48> tetherWindows;
    std::array<double,576> capBest;std::array<double,48> tetherBest;
    std::set<std::size_t> previousKeys,previousVisible;
    std::array<int64_t,576> previousReadback;
    std::array<bool,576> activationProven;
    bool haveHeader=false,haveFooter=false,natural=false,normal=false,stress=false,broken=false;
    uint64_t count=0,lastFrame=0,lastIndex=0;int64_t lastUs=0,lastTick=0,lastReadbackUs=0,lastReadbackTick=0;int lastLap=-1;
    double lastFraction=0;Vec lastPosition;
    void clearRuns(){for(auto&w:capWindows)w.reset();for(auto&w:tetherWindows)w.reset();previousKeys.clear();previousVisible.clear();}
public:
    Evidence(const ScintillaMissionAnchors::Manifest& m,const std::string& fingerprint):manifest(m),expectedFingerprint(fingerprint){capBest.fill(0);tetherBest.fill(0);activationProven.fill(false);previousReadback.fill(0);}
    void beginTrace(){haveHeader=haveFooter=false;count=lastFrame=lastIndex=0;lastUs=lastTick=lastReadbackUs=lastReadbackTick=0;lastLap=-1;clearRuns();}
    void header(const J& j)
    {
        closed(j,{"type","contract","schemaVersion","recordingID","producerUUID","raceEpoch","provenance","fingerprint","fingerprintJSON","fingerprintHash","groups","limits","phasePolicy","phase","actualLayoutRevision"});
        const J& f=at(j,"fingerprint");const auto profile=text(f,"profileId");
        const bool crewProfile=profile=="sc5_crew_standard_v1"||profile=="sc5_crew_party_v1";
        layoutRevision=crewProfile?3:2;
        if(haveHeader||text(j,"type")!="header"||text(j,"contract")!=(crewProfile?"tholus.stk-scintilla-measurement/2":"tholus.stk-scintilla-measurement/1")||num(j,"schemaVersion")!=(crewProfile?2:1)||
           text(j,"provenance")!="native_render_depth"||text(j,"fingerprintJSON")!=expectedFingerprint||
           !Protocol::uuid(text(j,"recordingID"))||!Protocol::uuid(text(j,"producerUUID"))||wide(at(j,"raceEpoch"))==0||
           text(j,"phasePolicy")!="diagnostic_constant_per_trace")throw Invalid("measurement_header");
        if(serialize(Parser(text(j,"fingerprintJSON")).parse())!=serialize(at(j,"fingerprint")))throw Invalid("measurement_fingerprint");
        if(!crewProfile&&profile!="sc4_cargo_three_slots_v1")throw Invalid("measurement_profile");
        const std::set<std::string> common={"anchorHash","trackHash","trackAssetsHash","shaderHash","executableHash","profileId","profileRevision","P","C","laps","F","width","height","cubemapSize","projection","output","near","far","faceFovDeg","domeTiltDeg","cameraMode","camera","horizonStabilization","capsuleRadius","tetherRadius","tetherOffsets","evaluatedLayoutRevision","nativeBaseSpeed"};
        auto fields=common;if(crewProfile)for(const auto* key:{"crewRulesRevision","zoneManifestHash","partyEnabled","partyCost","partyGain","partyHoldTicks","partyWindowTicks","partyDurationTicks","partyFadeTicks","partyCooldownTicks"})fields.insert(key);
        if(f.kind!=J::Object||f.object.size()!=fields.size())throw Invalid("measurement_fingerprint_fields");
        for(const auto& pair:f.object)if(!fields.count(pair.first))throw Invalid("measurement_fingerprint_fields");
        for(const auto* key:{"anchorHash","trackHash","trackAssetsHash","shaderHash","executableHash"})if(!Protocol::hash(text(f,key)))throw Invalid("measurement_fingerprint_hash");
        if(text(f,"profileRevision")!="1")throw Invalid("measurement_profile");
        crew=unsigned(signedInt(at(f,"C"),1,5));laps=unsigned(signedInt(at(f,"laps"),1,12));fps=unsigned(signedInt(at(f,"F"),30,1000));width=unsigned(signedInt(at(f,"width"),256,8192));height=unsigned(signedInt(at(f,"height"),256,8192));
        if(crew<1||crew>5||laps<1||laps>12||fps<(crewProfile?60u:30u)||fps>1000||width<256||height<256||width>8192||height>8192||num(f,"P")!=crew+1||num(f,"evaluatedLayoutRevision")!=layoutRevision)throw Invalid("measurement_config");
        if(crewProfile&&(text(f,"crewRulesRevision")!="1"||!Protocol::hash(text(f,"zoneManifestHash"))||boolean(at(f,"partyEnabled"))!=(profile=="sc5_crew_party_v1")||num(f,"partyCost")!=15||num(f,"partyGain")!=1||num(f,"partyHoldTicks")!=(3*fps+3)/4||num(f,"partyWindowTicks")!=3*fps||num(f,"partyDurationTicks")!=fps/2||num(f,"partyFadeTicks")!=fps/4||num(f,"partyCooldownTicks")!=6*fps))throw Invalid("measurement_crew_parameters");
        if(crewProfile?signedInt(at(j,"actualLayoutRevision"),0,3)!=3:signedInt(at(j,"actualLayoutRevision"),0,1)>1)throw Invalid("measurement_actual_layout");
        base=real(at(f,"nativeBaseSpeed"),.01,200)+(profile=="sc5_crew_party_v1"?1:0);projection=text(f,"projection");tilt=real(at(f,"domeTiltDeg"),-90,90);
        phase=text(j,"phase");if(phase!="shell"&&phase!="open")throw Invalid("measurement_phase");
        const J& limits=at(j,"limits");closed(limits,{"maxFrames","maxPending","maxGapUs","liveProofMaxAgeUs"});
        if(num(limits,"maxFrames")!=MaxFrames||num(limits,"maxPending")!=MaxPending||num(limits,"maxGapUs")!=MaxGapUs||num(limits,"liveProofMaxAgeUs")!=LiveProofMaxAgeUs)throw Invalid("measurement_limits");
        const auto& groups=arr(at(j,"groups"),manifest.groups.size(),manifest.groups.size());
        for(std::size_t g=0;g<groups.size();++g){const auto& a=groups[g];const auto& b=manifest.groups[g];closed(a,{"id","activationFraction","expiryFraction","laps","capsules"});
            if(text(a,"id")!=b.id||real(at(a,"activationFraction"),0,1)!=b.activationFraction||real(at(a,"expiryFraction"),0,1)!=b.expiryFraction)throw Invalid("measurement_groups");
            const auto& ls=arr(at(a,"laps"),b.laps.size(),b.laps.size());for(std::size_t i=0;i<ls.size();++i)if(integer(ls[i])!=b.laps[i])throw Invalid("measurement_laps");
            const auto& cs=arr(at(a,"capsules"),b.capsules.size(),b.capsules.size());for(std::size_t i=0;i<cs.size();++i){closed(cs[i],{"center","radius"});
                if(!close(vector(at(cs[i],"center")),Vec(b.capsules[i].center.x,b.capsules[i].center.y,b.capsules[i].center.z))||real(at(cs[i],"radius"),.01,10)!=b.capsules[i].radius)throw Invalid("measurement_shapes");}}
        haveHeader=true;
    }
    void frame(const J& j)
    {
        closed(j,{"type","index","frameID","captureUs","readbackUs","readbackTick","tick","officialLap","fraction","speed","minimumSpeed","maxSpeed","scenario","speedIncreaseTicks","active","continuous","published","camera","kartPosition","tetherCandidates","objects"});
        if(!haveHeader||haveFooter||text(j,"type")!="frame"||++count>MaxFrames)throw Invalid("measurement_frame_order");
        const uint64_t index=wide(at(j,"index")),frame=wide(at(j,"frameID"));
        const int64_t us=clock(at(j,"captureUs")),tick=clock(at(j,"tick"));
        const int64_t readbackUs=clock(at(j,"readbackUs")),readbackTick=clock(at(j,"readbackTick"));
        if(readbackUs<us||readbackTick<tick||(lastReadbackUs&&(readbackUs<lastReadbackUs||readbackTick<lastReadbackTick)))throw Invalid("measurement_readback_order");
        const int lap=int(real(at(j,"officialLap"),-1,12));if(double(lap)!=real(at(j,"officialLap"),-1,12))throw Invalid("measurement_lap");
        const double fraction=real(at(j,"fraction"),0,1),speed=real(at(j,"speed"),-200,200),minimum=real(at(j,"minimumSpeed"),0,200);
        real(at(j,"maxSpeed"),.01,200);if(minimum>std::abs(speed)+.00001)throw Invalid("measurement_speed_minimum");
        if(index!=count||!frame||(lastUs&&(us<=lastUs||frame<=lastFrame||tick<lastTick)))throw Invalid("measurement_time_order");
        const auto& flags=arr(at(j,"speedIncreaseTicks"),6,6);for(const J& f:flags)if(integer(f)>10000000)throw Invalid("measurement_speed_flag");
        const bool zipper=integer(flags[0])>0,nitro=integer(flags[2])>0;
        bool any=false;for(const auto& f:flags)any=any||integer(f)>0;
        const std::string scenario=nitro?"nitro":zipper?"zipper":any?"other":"normal";
        if(text(j,"scenario")!=scenario)throw Invalid("measurement_scenario");
        const auto& camera=at(j,"camera");closed(camera,{"position","right","up","forward"});
        const Vec position=vector(at(camera,"position")),right=vector(at(camera,"right")),up=vector(at(camera,"up")),forward=vector(at(camera,"forward"));
        if(std::abs(right.length()-1)>.001||std::abs(up.length()-1)>.001||std::abs(forward.length()-1)>.001||std::abs(right.dot(up))>.001||std::abs(right.dot(forward))>.001||std::abs(up.dot(forward))>.001)throw Invalid("measurement_camera_basis");
        const Vec kart=vector(at(j,"kartPosition"));const auto& candidates=arr(at(j,"tetherCandidates"),2,2);vector(candidates[0]);vector(candidates[1]);
        bool contiguous=lastUs&&us-lastUs<=MaxGapUs;
        if(lastLap>=0&&contiguous){const double progress=double(lap-lastLap)+fraction-lastFraction;if(lap<lastLap||progress<0||std::abs(progress)>.1||(kart-lastPosition).length()>20)contiguous=false;}
        const bool repeated=contiguous&&tick==lastTick&&boolean(at(j,"active"))&&boolean(at(j,"published"))&&boolean(at(j,"continuous"))&&lap>=0&&lap<int(laps);
        bool continuous=boolean(at(j,"continuous"))&&boolean(at(j,"active"))&&boolean(at(j,"published"))&&contiguous&&tick>lastTick&&lap>=0&&lap<int(laps);
        if(!continuous&&!repeated)clearRuns();
        if(boolean(at(j,"active"))&&boolean(at(j,"continuous"))&&boolean(at(j,"published"))){normal=normal||scenario=="normal";stress=stress||zipper||nitro;}
        std::set<std::size_t> keys,visibleKeys;
        for(const J& object:arr(at(j,"objects"),0,MaxObjects)){
            closed(object,{"kind","group","capsule","side","phase","center","radius","range","rectangleFree","activation","activationUs","inWindow","idealPixels","admissiblePixels","visiblePixels","status"});
            const bool tether=text(object,"kind")=="tether";if(!tether&&text(object,"kind")!="capsule")throw Invalid("measurement_kind");
            const Vec center=vector(at(object,"center")),delta=center-position;
            const Vec local(delta.dot(right),delta.dot(up),delta.dot(forward));const double radius=real(at(object,"radius"),.01,10);
            const auto kind=tether?ScintillaMission::Kind::Tether:ScintillaMission::Kind::Capsule;
            const bool range=ScintillaMission::Profile::inRange(kind,delta.dot(delta),radius);
            if(boolean(at(object,"range"))!=range)throw Invalid("measurement_range");
            const bool rect=footprint(projection,tilt,width,height,local,radius,layoutRevision);if(boolean(at(object,"rectangleFree"))!=rect)throw Invalid("measurement_footprint");
            const uint64_t ideal=integer(at(object,"idealPixels")),admitted=integer(at(object,"admissiblePixels")),visible=integer(at(object,"visiblePixels"));
            if(ideal>uint64_t(width)*height||admitted>ideal||visible>admitted)throw Invalid("measurement_counts");
            const auto status=text(object,"status");if(status!="ready"&&status!="pending_timeout"&&status!="unavailable"&&status!="overflow")throw Invalid("measurement_status");
            if(lap<0||lap>=int(laps))continue;
            std::size_t key;Window* window;double* best;
            if(tether){const int side=int(real(at(object,"side"),0,1));if(real(at(object,"side"),0,1)!=side||signedInt(at(object,"group"),-1,11)!=-1||signedInt(at(object,"capsule"),-1,1)!=-1||radius!=.5||!close(center,vector(candidates[side])))throw Invalid("measurement_tether");
                const std::string p=phase=="shell"?"awaiting_presentation":"recoverable";if(text(object,"phase")!=p)throw Invalid("measurement_tether_phase");
                key=576+(lap*2+side)*2+(phase=="open");window=&tetherWindows[key-576];best=&tetherBest[key-576];
            }else{const auto g=integer(at(object,"group")),c=integer(at(object,"capsule"));if(g>=int64_t(manifest.groups.size()))throw Invalid("measurement_group");const auto& group=manifest.groups[g];
                if(c>=int64_t(group.capsules.size())||(crew==1&&c!=0)||signedInt(at(object,"side"),-1,1)!=-1||text(object,"phase")!=phase||
                   std::find(group.laps.begin(),group.laps.end(),unsigned(lap))==group.laps.end()||fraction>=group.expiryFraction)throw Invalid("measurement_capsule_occurrence");
                const auto& shape=group.capsules[c];if(radius!=shape.radius||!close(center,Vec(shape.center.x,shape.center.y,shape.center.z)))throw Invalid("measurement_capsule_shape");
                key=(lap*24+g*2+c)*2+(phase=="open");window=&capWindows[key];best=&capBest[key];}
            if(!keys.insert(key).second)throw Invalid("measurement_duplicate_object");
            const bool geometry=status=="ready"&&range&&rect&&ideal>0&&ideal==admitted&&ideal==visible;
            const bool inWindow=boolean(at(object,"inWindow")),activation=boolean(at(object,"activation"));
            const int64_t activationUs=clock(at(object,"activationUs"));
            if((activationUs!=0)!=activation)throw Invalid("measurement_activation_clock");
            if(tether){if(!inWindow||activation)throw Invalid("measurement_tether_activation");}
            else {const auto& group=manifest.groups[integer(at(object,"group"))];
                if(inWindow!=(fraction>=group.activationFraction&&fraction<group.expiryFraction))throw Invalid("measurement_window_flag");
                if(activation){if(!inWindow||lastLap<0||!(double(lastLap)+lastFraction<double(lap)+group.activationFraction&&double(lap)+group.activationFraction<=double(lap)+fraction))throw Invalid("measurement_activation");
                    if(activationUs<=lastUs||activationUs>us)throw Invalid("measurement_activation_clock");
                    if(continuous&&geometry&&previousVisible.count(key)&&capsuleProofAvailable(lastUs,previousReadback[key],activationUs))activationProven[key]=true;}}
            const bool available=tether||capsuleProofAvailable(us,readbackUs,readbackUs);
            const bool overlap=tether||(previousKeys.count(key)&&capsuleProofsOverlap(lastUs,readbackUs));
            if(!tether&&!inWindow&&geometry&&available&&(continuous||repeated)){visibleKeys.insert(key);previousReadback[key]=readbackUs;}
            if(!overlap||!available)window->reset();
            const bool valid=continuous&&geometry&&inWindow&&available;
            if(valid){window->observe(us,tick,minimum,fps,tether,readbackUs,readbackTick);*best=std::max(*best,window->best);}
            else if(!repeated||!geometry||!inWindow)window->reset();
        }
        for(const auto key:previousKeys)if(!keys.count(key)){if(key<576)capWindows[key].reset();else tetherWindows[key-576].reset();}
        previousKeys=keys;previousVisible=visibleKeys;lastUs=us;lastTick=tick;lastFrame=frame;lastIndex=index;lastReadbackUs=readbackUs;lastReadbackTick=readbackTick;lastLap=lap;lastFraction=fraction;lastPosition=kart;
    }
    void footer(const J& j)
    {
        closed(j,{"type","frameCount","complete","reason","naturalFinish","officialLap"});
        if(!haveHeader||haveFooter||!count||text(j,"type")!="footer"||wide(at(j,"frameCount"))!=count||!boolean(at(j,"complete"))||!text(j,"reason").empty())throw Invalid("measurement_incomplete");
        const auto lap=signedInt(at(j,"officialLap"),-1,int(laps));
        if(boolean(at(j,"naturalFinish"))&&lap!=int(laps))throw Invalid("measurement_natural_finish");
        natural=natural||boolean(at(j,"naturalFinish"));haveFooter=true;clearRuns();
    }
    bool traceComplete()const{return haveHeader&&haveFooter;}
    bool qualified(Bounds* out,std::string* reason)const
    {
        auto fail=[&](const char*s){if(reason)*reason=s;return false;};if(!natural||!normal||!stress)return fail("measurement_native_race_or_stress_missing");
        Bounds result;for(unsigned lap=0;lap<laps;++lap){unsigned qualifiedGroups=0;
            for(std::size_t g=0;g<manifest.groups.size();++g){const auto& group=manifest.groups[g];if(std::find(group.laps.begin(),group.laps.end(),lap)==group.laps.end())continue;
                if(crew>1&&group.capsules.size()!=2)return fail("measurement_two_opportunities_required");
                const unsigned capsules=crew==1?1:2;for(unsigned c=0;c<capsules;++c){const auto i=lap*24+g*2+c;const double bound=std::min(capBest[i*2],capBest[i*2+1]);
                    if(bound<base*QualifiedSpeedShare)return fail("measurement_capsule_window_missing");if(!activationProven[i*2])return fail("measurement_activation_not_proven");result.capsules[i]=bound;}
                ++qualifiedGroups;}
            if(qualifiedGroups<4)return fail("measurement_replacement_opportunity_missing");
            double bound=200;for(unsigned side=0;side<2;++side)for(unsigned p=0;p<2;++p)bound=std::min(bound,tetherBest[(lap*2+side)*2+p]);
            if(bound<base*QualifiedSpeedShare)return fail("measurement_tether_window_missing");result.tether[lap]=bound;
        }if(out)*out=result;if(reason)reason->clear();return true;
    }
};
}
#endif
