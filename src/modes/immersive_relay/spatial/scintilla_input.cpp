// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/spatial/scintilla_input.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ScintillaSpatial
{
namespace
{
const double PI=3.14159265358979323846;
const int64_t SAFE=9007199254740991LL;
const int64_t RECENT_EVENT_TTL_US=1000000;
const size_t RECENT_EVENTS_PER_SOURCE=512;
using namespace ScintillaJSON;
bool malformedReason(const std::string& reason)
{
    return reason.compare(0,5,"json_")==0||reason.compare(0,8,"invalid_")==0||
        reason.compare(0,9,"expected_")==0||reason=="missing_field"||reason=="unknown_field"||
        reason=="unsupported_input_type"||reason=="unexpected_point"||reason=="point_message_mismatch";
}

Reply error(int status,const std::string& reason)
{return Reply(status,"{\"contract\":\"tholus.stk-spatial/1\",\"ok\":false,\"accepted\":false,\"reason\":"+quote(reason)+"}");}
Reply ok(const std::string& reason="accepted")
{return Reply(200,"{\"contract\":\"tholus.stk-spatial/1\",\"ok\":true,\"accepted\":true,\"reason\":"+quote(reason)+"}");}
Vec3 add(Vec3 a,Vec3 b){return Vec3(a.x+b.x,a.y+b.y,a.z+b.z);}
Vec3 sub(Vec3 a,Vec3 b){return Vec3(a.x-b.x,a.y-b.y,a.z-b.z);}
Vec3 mul(Vec3 a,double n){return Vec3(a.x*n,a.y*n,a.z*n);}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
bool finite(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
bool unit(Vec3 a){return finite(a)&&std::abs(dot(a,a)-1)<0.002;}
Vec3 vector(const J& j,double bound)
{const auto& a=arr(j,3,3);return Vec3(real(a[0],-bound,bound),real(a[1],-bound,bound),real(a[2],-bound,bound));}
void ray(const J& j)
{closed(j,{"originM","direction","basis"});vector(at(j,"originM"),1e6);if(std::abs(std::sqrt(dot(vector(at(j,"direction"),1),vector(at(j,"direction"),1)))-1)>1e-6||text(j,"basis")!="room.rhs-y-up-front-minus-z/1")throw Invalid("invalid_ray");}
struct Identity
{
    std::string source, binding, epoch, continuity, room, grip;
    bool operator==(const Identity& a) const{return source==a.source&&binding==a.binding&&epoch==a.epoch&&continuity==a.continuity&&room==a.room&&grip==a.grip;}
};
Identity identity(const J& j)
{return {tok(j,"sourceId"),tok(j,"deviceBindingRevision"),tok(j,"trackingSpaceEpoch"),tok(j,"poseContinuityId"),tok(j,"roomCalibrationRevision"),tok(j,"gripCalibrationRevision")};}
Projection projection(const J& j)
{
    closed(j,{"kind","width","height","domeTiltDeg"});Projection p;
    p.kind=text(j,"kind");int64_t w=num(j,"width"),h=num(j,"height");
    if(w<1||w>8192||h<1||h>8192||(p.kind!="fisheye_domemaster"&&p.kind!="equirectangular")||
       (p.kind=="fisheye_domemaster"&&w!=h)||(p.kind=="equirectangular"&&w!=h*2))throw Invalid("invalid_projection");
    p.width=unsigned(w);p.height=unsigned(h);p.domeTiltDeg=real(at(j,"domeTiltDeg"),-90,90);return p;
}
bool sameProjection(const Projection& a,const Projection& b)
{return a.kind==b.kind&&a.width==b.width&&a.height==b.height&&std::abs(a.domeTiltDeg-b.domeTiltDeg)<1e-6;}
struct Interval{int64_t lo=0,hi=0;};
bool missionKind(TargetKind kind)
{return kind==TargetKind::MissionCapsule||kind==TargetKind::MissionTether;}
bool sameMission(const ScintillaMission::Snapshot& a,const ScintillaMission::Snapshot& b)
{return a.id==b.id&&a.instance==b.instance&&a.module==b.module&&a.kind==b.kind&&a.phase==b.phase&&a.phaseRevision==b.phaseRevision;}
}

struct Runtime::Impl
{
    struct Pending;
    struct Source
    {
        Identity identity;
        std::string participant, suspendedContinuity;
        std::set<std::string> continuities;
        uint64_t aimSeq=0,actionSeq=0,stateSeq=0,cancelSeq=0,trackingLossRevision=0;
        uint64_t bindingLossRevision=0;
        std::string bindingContinuity;
        bool hasAim=false,hasAction=false,hasState=false,hasCancel=false,tracked=false;
        bool rearmed[3]={false,false,false},held[3]={false,false,false};
        // Physical observation is independent of gameplay admission/disarming.
        bool observedKnown[3]={false,false,false},observedHeld[3]={false,false,false};
        int64_t lastPrimary=-250000;
        // Acquisition time orders the shared cursor, independently of each
        // message kind's sequence and of later host/server clock refinements.
        int64_t cursorHostUs=-1;
        uint64_t toolEpoch=0, proofSerial=0;
        uint64_t toolPressSerial=0,toolReleaseSerial=0,releaseInputEpoch=0;
        ScintillaCrew::PartyAuthority partyPressAuthority;
        uint64_t partyCapturedProposal=0;
        int64_t partyPressTick=0;bool partyPressEligible=false;
        uint64_t proofTarget=0, proofInstance=0;
        TargetKind proofKind=TargetKind::Rival,pressKind=TargetKind::Rival;
        ScintillaMission::Snapshot proofMission,pressMission;
        bool pressMissionEligible=false;
        int64_t proofHostLow=0;
        bool proofVisible=false;
        // Sticky for this tool press: a later positive sample cannot erase a
        // resolved interruption before the authoritative combat tick sees it.
        bool proofInterrupted=false;
        ScintillaDefense::Surface proofSurface,pressSurface;
        uint64_t proofThreat=0,pressThreat=0;bool pressLocked=false;
        int64_t proofAdmitted=0;
        std::shared_ptr<Pending> queuedAim;
        size_t recentEventCount=0;
        bool hasFeedbackPress=false,hasAnchor=false;
        uint64_t feedbackSequence=0,feedbackRevision=0;
        double anchorU=0.5,anchorV=0.5;
    };
    struct EventReceipt
    {
        std::string id;
        size_t sourceIndex;
        int64_t receivedAt;
    };
    struct Presented {uint64_t id;int64_t lo,hi,hostLo,hostHi;};
    struct Pending
    {
        HitEvent event;
        bool aim=false;
        uint64_t queryOwner=0;
        std::shared_ptr<void> queryLease;
        std::map<uint64_t,Visibility> visibilityResults;
        uint64_t proofSerial=0;
        uint64_t toolEpoch=0;
        ScintillaDefense::Surface surface;uint64_t threat=0;
        ScintillaCrew::PartyAuthority crewAuthority;
        uint64_t crewProposal=0;
        uint64_t feedbackRevision=0,actionSequence=0;
        Interval hostTime;
        double u=0,v=0;
        int64_t selectionDeadline=0,deadline=0;
        std::vector<Frame> frames;
    };
    std::string session,generation,installation,consumer,context,routeRevision,stream,timingRevision,lastReason="unprepared";
    std::vector<Source> sources;
    std::vector<Reticle> reticles;
    std::deque<Frame> frames;
    std::deque<HitEvent> events;
    std::deque<Pending> pending, pendingAims;
    std::deque<HitEvent> combatShots;
    bool combatEnabled=false,defenseEnabled=false,missionEnabled=false,missionLayoutRequired=false,allowRebind=false,participantBindingsValid=true;
    bool crewEnabled=false,partyEnabled=false;
    uint64_t crewRace=0,crewTurn=0,crewIntent=0,crewProposal=0,crewRoster=0,crewObservedInput=0;
    uint64_t inputEpoch=0, nextSerial=0, nextProof=0, nextQueryOwner=0,lastRebindFrom=0,lastRebindTo=0;
    int64_t simulationTick=0;
    uint64_t combatRaceEpoch=0;
    std::vector<ScintillaCombat::Snapshot> combatTargets;
    uint64_t missionRevision=0;
    std::vector<ScintillaMission::Snapshot> missionTargets;
    std::vector<Presented> presented;
    // UUID semantic deduplication is recent-window only. Exact canonical replay
    // remains blocked for the grant lifetime by actionSeq and the 60 ms freshness
    // check; changing both sequence/time creates a different canonical packet.
    std::set<std::string> seenEvents,usedSessions;
    std::deque<EventReceipt> recentEvents;
    Projection projection,producerProjection;
    bool producerProjectionKnown=false;
    std::set<std::string> capabilities,expectedParticipants;
    bool participantsKnown=false;
    int64_t issuedAt=0,inputUncertainty=0;
    bool prepared=false,active=false,historyReady=true,hasHeartbeat=false,hasGameplayObservation=false;
    uint64_t resumeRevision=0;
    int64_t activationNotBefore=0;
    uint64_t heartbeatSeq=0;
    // One bounded maintenance transaction. Raw grant/source/anti-replay state
    // remains owned by SI5; this lock only suspends native admission.
    bool inputMaintenance=false,inputMaintenanceCompleted=false;
    std::string inputMaintenanceID;
    uint64_t inputMaintenanceFrom=0,inputMaintenanceTo=0;
    std::deque<uint64_t> retiredFrames;
    std::set<uint64_t> retiredFrameIds;
    uint64_t retiredThrough=0;
    int64_t leaseUntil=0,clockLow=0,clockHigh=0,clockChecked=0,proofHostFrom=0,proofHostUntil=0,lastNow=0;
    VisibilityOracle visibility;
    FramePinOracle pin;
    QueryLeaseOracle queryLease;
    LiveTargetOracle live;

    bool layoutMatches(const Frame& frame)const
    {
        return frame.layout.valid()&&frame.layout.mission==missionLayoutRequired&&
            frame.layout.crew==crewEnabled&&crewFrameMatches(frame)&&
            frame.layout.width==frame.projection.width&&frame.layout.height==frame.projection.height;
    }
    ScintillaCrew::PartyAuthority crewAuthority()const {
        ScintillaCrew::PartyAuthority a;a.race=crewRace;a.driverTurnId=crewTurn;a.crewIntentRevision=crewIntent;
        a.inputEpoch=inputEpoch;a.rosterRevision=crewRoster;a.routeRevision=routeRevision;a.projectionContextId=context;return a;
    }
    bool crewFrameMatches(const Frame& f)const {
        return !crewEnabled?!f.crewEnabled:f.crewEnabled&&f.partyEnabled==partyEnabled&&
            f.crewRaceEpoch==crewRace&&f.crewDriverTurnId==crewTurn&&f.crewIntentRevision==crewIntent&&
            f.crewInputEpoch==inputEpoch&&f.crewRosterRevision==crewRoster&&crewRace&&crewIntent&&crewRoster&&inputEpoch;
    }

    void pruneEvents(int64_t now)
    {
        // Receive time is authoritative even for a rejected future-dated event.
        // Sources are replaced only by fresh prepare, which clears both containers.
        while(!recentEvents.empty()&&now-recentEvents.front().receivedAt>=RECENT_EVENT_TTL_US)
        {
            const EventReceipt& receipt=recentEvents.front();
            seenEvents.erase(receipt.id);
            --sources[receipt.sourceIndex].recentEventCount;
            recentEvents.pop_front();
        }
    }
    size_t sourceIndex(const std::string& id) const
    {for(size_t i=0;i<sources.size();++i)if(sources[i].identity.source==id)return i;throw Invalid("unknown_source");}
    void clearFeedback(size_t i)
    {reticles[i].feedback=Feedback();sources[i].hasAnchor=false;++sources[i].feedbackRevision;}
    bool beginPressFeedback(size_t i,uint64_t sequence)
    {
        Source& source=sources[i];
        if(source.hasFeedbackPress&&sequence<=source.feedbackSequence)return false;
        source.hasFeedbackPress=true;source.feedbackSequence=sequence;++source.feedbackRevision;return true;
    }
    void setFeedback(size_t i,FeedbackKind kind,uint64_t sequence,int64_t now,int64_t until,
                     bool hasPoint,double u,double v,const std::string& reason)
    {
        Feedback feedback;feedback.kind=kind;feedback.actionSequence=sequence;
        feedback.startedUs=now;feedback.untilUs=until;feedback.hasPoint=hasPoint;
        feedback.u=u;feedback.v=v;feedback.reason=reason;reticles[i].feedback=feedback;
    }
    void setToolFeedback(size_t i,ScintillaCombat::ToolSnapshot value)
    {
        const Source& s=sources[i];Reticle& r=reticles[i];r.toolFeedback=ScintillaCombat::ToolSnapshot();
        if(!combatEnabled||!prepared||!active||!s.tracked||!s.held[1]||missionKind(s.proofKind)||
           s.pressSurface.context==ScintillaDefense::Context::PartyBoost)return;
        // The input barrier may be established between authoritative ticks.
        // Even an older same-epoch core snapshot must not hide this instruction.
        if(s.proofInterrupted){
            r.toolFeedback.participant=s.participant;r.toolFeedback.controlEpoch=s.toolEpoch;
            r.toolFeedback.target=s.proofTarget;r.toolFeedback.status=ScintillaCombat::ToolStatus::ReleaseRequired;
        }else if(value.participant==s.participant&&value.controlEpoch==s.toolEpoch)r.toolFeedback=std::move(value);
    }
    void setMissionFeedback(size_t i,ScintillaMission::Feedback value)
    {
        const Source& s=sources[i];Reticle& r=reticles[i];r.missionFeedback=ScintillaMission::Feedback();
        const bool missionContext=missionKind(s.proofKind)||
            (s.proofTarget==0&&missionKind(s.pressKind)&&s.proofSurface.context==ScintillaDefense::Context::WorldOffense);
        if(!missionEnabled||!prepared||!active||!s.tracked||!s.held[1]||!s.pressLocked||!missionContext)return;
        if(value.participant==s.participant&&value.controlEpoch==s.toolEpoch)r.missionFeedback=std::move(value);
        if(s.proofInterrupted){
            r.missionFeedback.participant=s.participant;r.missionFeedback.controlEpoch=s.toolEpoch;
            r.missionFeedback.entity=s.pressMission.id;r.missionFeedback.status=ScintillaMission::Status::ReleaseRequired;
        }
    }
    void setPartyToolFeedback(size_t i,ScintillaCrew::PartyFeedback value)
    {
        const auto& s=sources[i];auto& view=reticles[i].partyFeedback;view=ScintillaCrew::PartyFeedback();
        if(!crewEnabled||!partyEnabled||!prepared||!active||!s.tracked||!s.held[1]||
           s.pressSurface.context!=ScintillaDefense::Context::PartyBoost)return;
        if(value.participantId==s.participant&&value.controlEpoch==s.toolEpoch)view=value;
        if(s.proofInterrupted||(s.pressLocked&&!s.partyPressEligible)){
            view=ScintillaCrew::PartyFeedback();view.participantId=s.participant;
            view.controlEpoch=s.toolEpoch;view.reason=ScintillaCrew::PartyReason::ReleaseRequired;
        }
    }
    void captureBindingContinuity()
    {
        for(auto& source:sources){source.bindingLossRevision=source.trackingLossRevision;
            source.bindingContinuity=source.identity.continuity;}
    }
    void forgetControlObservations(size_t i)
    {for(unsigned c=0;c<3;++c)sources[i].observedKnown[c]=false;
        sources[i].toolReleaseSerial=sources[i].releaseInputEpoch=0;}
    void clearControls(size_t i,const std::string& reason)
    {
        Source& s=sources[i];++s.toolEpoch;s.proofVisible=false;s.queuedAim.reset();for(unsigned k=0;k<3;++k){s.held[k]=false;s.rearmed[k]=false;}
        s.toolPressSerial=0;s.partyPressAuthority=ScintillaCrew::PartyAuthority();
        s.partyCapturedProposal=0;s.partyPressTick=0;s.partyPressEligible=false;
        s.pressMissionEligible=false;s.pressMission=ScintillaMission::Snapshot();s.proofMission=ScintillaMission::Snapshot();
        reticles[i].toolFeedback=ScintillaCombat::ToolSnapshot();reticles[i].defenseFeedback=ScintillaDefense::ToolFeedback();s.pressLocked=false;
        reticles[i].missionFeedback=ScintillaMission::Feedback();
        reticles[i].partyFeedback=ScintillaCrew::PartyFeedback();
        reticles[i].toolEpoch=s.toolEpoch;reticles[i].valid=false;reticles[i].primaryHeld=false;reticles[i].toolHeld=false;reticles[i].result=reason;
    }
    void disarm(size_t i,const std::string& reason)
    {
        clearControls(i,reason);
        const Feedback feedback=reticles[i].feedback;
        if(feedback.kind==FeedbackKind::Pending&&feedback.actionSequence==sources[i].feedbackSequence)
            setFeedback(i,FeedbackKind::Rejected,feedback.actionSequence,lastNow,lastNow+500000,
                        feedback.hasPoint,feedback.u,feedback.v,reason);
        for(auto& q:combatShots)if(q.sourceId==sources[i].identity.source&&(q.result=="pending"||q.result=="hit"))q.result=reason;
        for(auto p=pending.begin();p!=pending.end();)if(p->event.sourceId==sources[i].identity.source)p=pending.erase(p);else ++p;
        for(auto p=pendingAims.begin();p!=pendingAims.end();)if(p->event.sourceId==sources[i].identity.source)p=pendingAims.erase(p);else ++p;
    }
    void stop(const std::string& reason)
    {
        const bool resourceLoss=reason=="history_unavailable"||reason=="frame_history_unavailable"||reason=="depth_history_unavailable";
        const bool authorityBoundary=reason=="prepared"||reason=="participants_rebound"||reason=="participant_rebind_required"||
            reason=="race_rebind_required"||reason=="input_maintenance"||reason=="input_maintenance_resumed"||
            reason=="gameplay_resumed"||reason=="gameplay_suspended"||reason=="layout_context_changed"||reason=="crew_context_changed";
        for(size_t i=0;i<sources.size();++i){if(authorityBoundary)forgetControlObservations(i);disarm(i,reason);if(!resourceLoss)clearFeedback(i);}
        pending.clear();pendingAims.clear();combatShots.clear();lastReason=reason;
    }
    void invalidate(const std::string& reason)
    {stop(reason);prepared=false;active=false;inputMaintenance=false;inputMaintenanceCompleted=false;inputMaintenanceID.clear();inputMaintenanceFrom=inputMaintenanceTo=0;presented.clear();frames.clear();retiredFrames.clear();retiredFrameIds.clear();retiredThrough=0;proofHostFrom=proofHostUntil=0;stream.clear();}
    void contextCheck(const J& j) const
    {if(tok(j,"inputSessionID")!=session||tok(j,"routingGeneration")!=generation)throw Invalid("stale_session");}
    void clocks(const J& j,int64_t now,int64_t& low,int64_t& high)
    {
        const auto& samples=arr(j,3,16);bool first=true;
        int64_t previousHostSent=-1,previousServerSent=-1,previousHostReceived=-1;
        for(const auto& s:samples)
        {
            closed(s,{"hostSentUs","serverReceivedUs","serverSentUs","hostReceivedUs"});
            int64_t t0=num(s,"hostSentUs"),t1=num(s,"serverReceivedUs"),t2=num(s,"serverSentUs"),t3=num(s,"hostReceivedUs");
            if(t3<t0||t2<t1||t3-t0>50000||t3-t0<t2-t1||t2>now||now-t2>1000000||
               t0<=previousHostSent||t2<=previousServerSent||t3<=previousHostReceived)
                throw Invalid("invalid_clock_sample");
            previousHostSent=t0;previousServerSent=t2;previousHostReceived=t3;
            // Each observation is aged to NOW before intersection. Choosing an
            // older low-RTT sample cannot silently reset its accumulated drift.
            int64_t drift=(now-t2+9999)/10000+1;
            int64_t l=t2-t3-drift,h=t1-t0+drift;
            if(l>h)throw Invalid("invalid_clock_sample");
            if(first){low=l;high=h;first=false;}else{low=std::max(low,l);high=std::min(high,h);}
        }
        if(low>high||high-low>40000)throw Invalid("clock_uncertainty");
    }
    Interval hostTime(const J& j,int64_t now,bool includeOptical=true,Interval* originalHostTime=nullptr)
    {
        closed(j,{"evaluatedAtHostUs","uncertaintyUs"});int64_t t=num(j,"evaluatedAtHostUs"),u=num(j,"uncertaintyUs")+(includeOptical?inputUncertainty:0);
        if(u>20000||t>SAFE-u||t<u||now<clockChecked||now-clockChecked>1000000)throw Invalid("stale_clock");
        // Conservative ±100 ppm drift since the last probe, added to BOTH ends.
        int64_t drift=(now-clockChecked+9999)/10000;
        Interval r;r.lo=t-u+clockLow-drift;r.hi=t+u+clockHigh+drift;
        if(r.lo<0||r.hi>now||now-r.lo>60000||r.hi-r.lo>40000)throw Invalid("stale_or_uncertain_pose");
        if(originalHostTime){originalHostTime->lo=t-u;originalHostTime->hi=t+u;}return r;
    }
    void record(const HitEvent& event)
    {events.push_back(event);if(events.size()>256)events.pop_front();}
    void finish(Pending& p,const std::string& result,int64_t now,uint64_t target=0,uint64_t instance=0,uint64_t frame=0)
    {
        p.event.result=result;p.event.targetId=target;p.event.targetInstance=instance;p.event.frameId=frame;
        const size_t i=sourceIndex(p.event.sourceId);Source& source=sources[i];
        const bool secondary=p.event.control=="secondary";
        if(p.aim||secondary){
            if(p.toolEpoch==source.toolEpoch&&p.proofSerial>=source.proofSerial){
                const bool visible=(result=="hit"&&p.event.historicalEligible)||result=="hud";
                if((defenseEnabled||missionLayoutRequired||crewEnabled)&&source.held[1]&&source.pressLocked&&(p.surface.context!=source.pressSurface.context||p.surface.sector!=source.pressSurface.sector||p.threat!=source.pressThreat))source.proofInterrupted=true;
                reticles[i].intent=p.surface;source.proofSurface=p.surface;source.proofThreat=p.threat;source.proofAdmitted=simulationTick;
                const uint64_t proofInstance=missionKind(p.event.kind)?p.event.historicalMission.instance:p.event.historical.instance;
                if(source.proofVisible&&(!visible||source.proofTarget!=target||source.proofInstance!=proofInstance||source.proofKind!=p.event.kind||
                   (missionKind(p.event.kind)&&!sameMission(source.proofMission,p.event.historicalMission))))
                    source.proofInterrupted=true;
                source.proofSerial=p.proofSerial;source.proofVisible=visible;
                source.proofTarget=target;source.proofInstance=proofInstance;
                source.proofKind=p.event.kind;source.proofMission=p.event.historicalMission;
                reticles[i].targetKind=target==0&&source.pressLocked&&missionKind(source.pressKind)&&
                    p.surface.context==ScintillaDefense::Context::WorldOffense?source.pressKind:p.event.kind;
                source.proofHostLow=p.hostTime.lo;
                setToolFeedback(i,reticles[i].toolFeedback);
                setMissionFeedback(i,reticles[i].missionFeedback);
                setPartyToolFeedback(i,reticles[i].partyFeedback);
            }
            if(p.aim)return;
        }
        if(secondary&&p.toolEpoch==source.toolEpoch&&(defenseEnabled||missionEnabled||crewEnabled)){
            source.pressSurface=p.surface;source.pressThreat=p.threat;source.pressLocked=true;
            source.pressKind=p.event.kind;source.pressMission=p.event.historicalMission;
            source.partyPressEligible=result=="hud"&&p.surface.context==ScintillaDefense::Context::PartyBoost&&partyEnabled;
            source.partyCapturedProposal=p.crewProposal;
            source.pressMissionEligible=false;
            if(missionKind(p.event.kind)&&p.event.historicalEligible&&p.event.missionRevision==missionRevision){
                const auto& historic=p.event.historicalMission;
                const bool open=historic.phase==ScintillaMission::Phase::Open||historic.phase==ScintillaMission::Phase::TetherRecoverable;
                for(const auto& admitted:p.event.admittedMissionTargets)
                    if(open&&admitted.eligible&&sameMission(admitted,historic))source.pressMissionEligible=true;
            }
            if(source.proofSerial&&(source.proofSurface.context!=p.surface.context||source.proofSurface.sector!=p.surface.sector||source.proofThreat!=p.threat))source.proofInterrupted=true;
            if(missionKind(source.pressKind)&&source.proofSerial&&
               (source.proofKind!=source.pressKind||!sameMission(source.proofMission,source.pressMission)))source.proofInterrupted=true;
            setMissionFeedback(i,reticles[i].missionFeedback);
            setPartyToolFeedback(i,reticles[i].partyFeedback);
        }
        record(p.event);
        if(p.event.serial){
            for(auto& q:combatShots)if(q.serial==p.event.serial){q=p.event;break;}
        }
        reticles[i].result=result;
        if(source.feedbackRevision==p.feedbackRevision){
            const bool gameplay=result=="hit"&&p.event.serial!=0;
            const bool beam=result=="hit"&&combatEnabled&&p.event.control=="secondary";
            setFeedback(i,beam?FeedbackKind::None:gameplay?FeedbackKind::Pending:result=="hit"?FeedbackKind::Hit:result=="miss"?FeedbackKind::Miss:FeedbackKind::Rejected,
                        p.actionSequence,now,now+500000,true,p.u,p.v,beam?"beam_acquiring":gameplay?"combat_pending":result);
        }
        if(result=="frame_history_unavailable"||result=="depth_history_unavailable"||result=="depth_unavailable"||result=="history_timeout")clearControls(i,result);
    }
    void resolveQueue(std::deque<Pending>& queue,int64_t now)
    {
        for(auto it=queue.begin();it!=queue.end();)
        {
            Pending& p=*it;bool wait=false,fail=false;std::string reason="miss";
            uint64_t agreed=0,agreedInstance=0,representative=0;bool first=true;
            bool combatFirst=true,surfaceFirst=true;
            if(now>p.deadline||(p.frames.empty()&&now>p.selectionDeadline))
            {finish(p,now>p.deadline?"history_timeout":"frame_history_unavailable",now);it=queue.erase(it);continue;}
            if(p.frames.empty())
            {
                // Presentation proof must CLOSE the entire plausible interval.
                // Delayed proof is allowed briefly; never guess the latest frame.
                // Event time and optical presentation use the same canonical
                // host clock. NTP renewals must not change these candidates.
                if(proofHostFrom>p.hostTime.lo||proofHostUntil<p.hostTime.hi){++it;continue;}
                int64_t cursor=p.hostTime.lo;
                std::vector<Frame> candidates;
                for(const auto& r:presented)if(r.hostHi>=p.hostTime.lo&&r.hostLo<=p.hostTime.hi)
                {
                    if(r.hostLo>cursor){fail=true;reason="presentation_gap";break;}
                    cursor=std::max(cursor,r.hostHi);
                    auto duplicate=std::find_if(candidates.begin(),candidates.end(),[&](const Frame& f){return f.id==r.id;});
                    if(duplicate==candidates.end())
                    {
                        auto frame=std::find_if(frames.begin(),frames.end(),[&](const Frame& f){return f.id==r.id&&f.stream==stream;});
                        if(frame==frames.end()){fail=true;reason="frame_history_unavailable";break;}
                        candidates.push_back(*frame);
                    }
                    if(candidates.size()>8){fail=true;reason="presentation_candidate_capacity";break;}
                }
                if(!fail&&(cursor<p.hostTime.hi||candidates.empty())){fail=true;reason="presentation_gap";}
                // Acquire every candidate before issuing even the first GPU query.
                // The rolling metadata journal itself never owns these leases.
                if(!fail&&queryLease){
                    if(nextQueryOwner==std::numeric_limits<uint64_t>::max()){fail=true;reason="query_owner_exhausted";}
                    else {p.queryOwner=++nextQueryOwner;p.queryLease=queryLease(p.queryOwner);}
                }
                for(auto& candidate:candidates)candidate.queryOwner=p.queryOwner;
                if(!fail&&pin)for(auto& candidate:candidates)
                {
                    candidate.visibilityLease=pin(candidate);
                    if(!candidate.visibilityLease){fail=true;reason="depth_history_unavailable";break;}
                }
                if(!fail)p.frames=std::move(candidates);
            }
            for(const Frame& candidate:p.frames)
            {
                if(fail)break;
                const Frame* f=&candidate;const uint64_t frameID=f->id;
                if(f->routeRevision!=routeRevision||f->projectionContextId!=context||!sameProjection(f->projection,projection))
                {fail=true;reason="frame_context_changed";break;}
                ScintillaDefense::Surface surface;surface.context=ScintillaDefense::Context::WorldOffense;
                if(defenseEnabled||missionLayoutRequired||crewEnabled){
                    if((defenseEnabled&&!f->defense.enabled)||!layoutMatches(*f)){fail=true;reason="layout_unavailable";break;}
                    if(crewEnabled&&p.crewAuthority!=crewAuthority()){fail=true;reason="crew_context_changed";break;}
                    surface=f->layout.classify(p.u,p.v);
                    // World offense belongs to the target/control epoch, independent
                    // of an AI warning. Only SC3 surfaces bind the threat identity.
                    const uint64_t contextThreat=surface.context==ScintillaDefense::Context::WorldOffense||surface.context==ScintillaDefense::Context::PartyBoost?0:f->defense.threat;
                    if(!p.aim&&surface.context==ScintillaDefense::Context::PartyBoost&&
                       f->crewProposalId&&f->crewProposalId!=crewProposal){fail=true;reason="party_proposal_changed";break;}
                    if(surfaceFirst){p.surface=surface;p.threat=contextThreat;p.crewProposal=f->crewProposalId;surfaceFirst=false;}
                    else if(surface.context!=p.surface.context||surface.sector!=p.surface.sector||p.threat!=contextThreat){fail=true;reason="ambiguous_layout_context";break;}
                    else if(!p.aim&&surface.context==ScintillaDefense::Context::PartyBoost&&p.crewProposal!=f->crewProposalId){fail=true;reason="ambiguous_party_proposal";break;}
                    if(surface.context!=ScintillaDefense::Context::WorldOffense){representative=frameID;continue;}
                }else p.surface=surface;
                Vec3 local;if(!Runtime::projectionDirection(f->projection,p.u,p.v,&local)){fail=true;reason="outside_projection";break;}
                Vec3 d=add(add(mul(f->cameraRight,local.x),mul(f->cameraUp,local.y)),mul(f->cameraForward,local.z));
                const Target* selected=nullptr;
                double contact=std::numeric_limits<double>::infinity(),runnerUp=contact;
                for(const auto& t:f->targets)
                {
                    Vec3 offset=sub(f->cameraPosition,t.center);double b=dot(offset,d),c=dot(offset,offset)-t.radius*t.radius;
                    double disc=b*b-c;if(disc<0)continue;double q=-b-std::sqrt(disc);
                    // Match the marker shader: select the nearest positive
                    // front surface before applying the capture clip planes.
                    if(q<=0)continue;
                    if(q<contact){runnerUp=contact;selected=&t;contact=q;}
                    else if(q<runnerUp)runnerUp=q;
                }
                // Only a tie at the final nearest surface is ambiguous. A tie
                // farther away cannot hide a closer marker encountered later.
                if(selected&&runnerUp-contact<1e-7){fail=true;reason="ambiguous_target";break;}
                uint64_t id=0,instance=0;
                // Clip in the selected cubemap face's camera-space depth,
                // never radial distance. A clipped nearest marker cannot
                // expose a farther marker hidden by the shader.
                const double faceDepth=contact*std::max({std::abs(local.x),std::abs(local.y),std::abs(local.z)});
                if(selected&&faceDepth>f->nearPlane&&faceDepth<f->farPlane)
                {
                    auto saved=p.visibilityResults.find(f->id);
                    Visibility v=saved!=p.visibilityResults.end()?saved->second:visibility?visibility(*f,*selected,d,contact):Visibility::Unavailable;
                    if(v==Visibility::Visible||v==Visibility::Occluded)p.visibilityResults[f->id]=v;
                    if(v==Visibility::Pending){wait=true;break;}
                    if(v==Visibility::Unavailable){fail=true;reason="depth_unavailable";break;}
                    if(v==Visibility::Visible){id=selected->id;instance=selected->instance;}
                }
                if(id&&selected){
                    if(missionKind(selected->kind)){
                        const auto& snapshot=selected->mission;
                        const ScintillaMission::Kind kind=selected->kind==TargetKind::MissionCapsule?ScintillaMission::Kind::Capsule:ScintillaMission::Kind::Tether;
                        if(!missionEnabled||!f->missionEnabled||f->missionRaceEpoch!=combatRaceEpoch||f->missionRevision!=missionRevision||
                           snapshot.id!=selected->id||snapshot.instance!=selected->instance||!snapshot.module||!snapshot.phaseRevision||
                           snapshot.kind!=kind){fail=true;reason="mission_context_changed";break;}
                        const Vec3 delta=sub(selected->center,f->cameraPosition);
                        const bool eligible=snapshot.eligible&&ScintillaMission::Profile::inRange(kind,dot(delta,delta),selected->radius);
                        if(combatFirst){p.event.kind=selected->kind;p.event.historicalMission=snapshot;
                            p.event.historicalEligible=eligible;p.event.missionRevision=missionRevision;combatFirst=false;}
                        else if(p.event.kind!=selected->kind||!sameMission(p.event.historicalMission,snapshot)){
                            fail=true;reason="ambiguous_mission_phase";break;
                        }else p.event.historicalEligible=p.event.historicalEligible&&eligible;
                    }else{
                    auto snapshot=selected->combat;
                    const Vec3 delta=add(selected->center,mul(f->cameraPosition,-1));
                    const bool eligible=snapshot.eligible&&ScintillaCombat::Profile::inRange(dot(delta,delta),selected->radius);
                    if(combatFirst){p.event.kind=selected->kind;p.event.historical=snapshot;p.event.historicalEligible=eligible;
                        p.event.historicalOpen=eligible&&snapshot.phase==ScintillaCombat::Phase::Breached;combatFirst=false;}
                    else {
                        if(p.event.kind!=selected->kind){fail=true;reason="ambiguous_target_kind";break;}
                        p.event.historicalEligible=p.event.historicalEligible&&eligible&&snapshot.instance==p.event.historical.instance;
                        p.event.historicalOpen=p.event.historicalOpen&&eligible&&snapshot.phase==ScintillaCombat::Phase::Breached&&snapshot.window==p.event.historical.window;
                    }
                    }
                }
                if(first){agreed=id;agreedInstance=instance;representative=frameID;first=false;}
                else if(agreed!=id||agreedInstance!=instance){fail=true;reason="ambiguous_presentation";break;}
            }
            if(wait){++it;continue;}
            const bool hud=(defenseEnabled||missionLayoutRequired||crewEnabled)&&p.surface.context!=ScintillaDefense::Context::WorldOffense&&p.surface.context!=ScintillaDefense::Context::None;
            if(!fail&&hud){finish(p,p.aim||(p.event.control=="secondary"&&(p.surface.context==ScintillaDefense::Context::DefenseSector||p.surface.context==ScintillaDefense::Context::ServiceBoost||(partyEnabled&&p.surface.context==ScintillaDefense::Context::PartyBoost)))?"hud":"non_targetable_surface",now,0,0,representative);it=queue.erase(it);continue;}
            if(!fail&&agreed&&(!live||!live(agreed,agreedInstance))){fail=true;reason="target_instance_retired";}
            if(fail||!agreed){p.event.historicalEligible=false;p.event.historicalOpen=false;}
            finish(p,fail?reason:(agreed?"hit":"miss"),now,fail?0:agreed,fail?0:agreedInstance,representative);
            it=queue.erase(it);
        }
    }
    void resolve(int64_t now)
    {
        resolveQueue(pending,now); // reliable presses reserve resources first
        for(auto& source:sources)if(source.queuedAim){
            const bool busy=std::any_of(pendingAims.begin(),pendingAims.end(),[&](const Pending& p){return p.event.sourceId==source.identity.source;});
            if(!busy){pendingAims.push_back(*source.queuedAim);source.queuedAim.reset();}
        }
        resolveQueue(pendingAims,now);
    }
    void queueAim(size_t i,const Interval& host,int64_t sampleLow,double u,double v,int64_t now)
    {
        Source& source=sources[i];
        // The admitted layout revision is immutable within this projection context.
        // A known context/sector departure is a negative fact even before GPU
        // resolution, and must survive coalescing or a pending older aim. This
        // can only interrupt a held tool; positive proof still requires history.
        if((defenseEnabled||missionLayoutRequired||crewEnabled)&&source.held[1]){
            const auto surface=ScintillaDefense::Layout::create(projection.width,projection.height,missionLayoutRequired,false,crewEnabled).classify(u,v);
            if(surface.context!=source.pressSurface.context||surface.sector!=source.pressSurface.sector){
                source.proofInterrupted=true;setToolFeedback(i,reticles[i].toolFeedback);setMissionFeedback(i,reticles[i].missionFeedback);setPartyToolFeedback(i,reticles[i].partyFeedback);
            }
        }
        if(nextProof==std::numeric_limits<uint64_t>::max()){source.proofVisible=false;return;}
        std::shared_ptr<Pending> p(new Pending());p->aim=true;p->toolEpoch=source.toolEpoch;p->proofSerial=++nextProof;
        p->event.sourceId=source.identity.source;p->event.participantId=source.participant;
        p->crewAuthority=crewAuthority();
        p->hostTime=host;p->event.evaluatedUs=sampleLow;p->u=u;p->v=v;
        p->selectionDeadline=sampleLow+60000;p->deadline=std::min(now+PendingTimeoutUs,sampleLow+60000);
        source.queuedAim=p;
    }

};

Runtime::Runtime():m(new Impl){}
Runtime::~Runtime(){}
const std::vector<Reticle>& Runtime::reticles()const{return m->reticles;}
const std::deque<HitEvent>& Runtime::events()const{return m->events;}
const std::string& Runtime::activeRouteRevision()const{return m->routeRevision;}
const std::string& Runtime::activeProjectionContextId()const{return m->context;}
void Runtime::setVisibilityOracle(const VisibilityOracle& o){m->visibility=o;}
void Runtime::setFramePinOracle(const FramePinOracle& o){m->pin=o;}
void Runtime::setQueryLeaseOracle(const QueryLeaseOracle& o){m->queryLease=o;}
void Runtime::setLiveTargetOracle(const LiveTargetOracle& o){m->live=o;}
void Runtime::reset(const std::string& reason){m->invalidate(reason);}
bool Runtime::resetForRace()
{
    if(!m->prepared||!m->allowRebind||m->inputEpoch==UINT64_MAX)return false;
    m->stop("race_rebind_required");
    if(!m->frames.empty())m->retiredThrough=std::max(m->retiredThrough,m->frames.back().id);
    m->frames.clear();m->presented.clear();m->events.clear();m->retiredFrames.clear();m->retiredFrameIds.clear();
    m->proofHostFrom=m->proofHostUntil=0;m->historyReady=false;m->active=false;m->participantBindingsValid=false;
    m->hasGameplayObservation=false;m->resumeRevision=0;m->activationNotBefore=std::max(m->activationNotBefore,m->lastNow);
    m->combatEnabled=false;m->defenseEnabled=false;m->missionEnabled=false;
    m->combatTargets.clear();m->missionTargets.clear();m->missionRevision=0;m->simulationTick=0;
    m->lastRebindFrom=m->lastRebindTo=0;
    // Stream/frame IDs identify the live producer, not the race. Keep them and
    // SI5 clocks/lease/source continuity/sequence and replay guards unchanged.
    return true;
}

Reply Runtime::validateCombatPrepare(const std::string& body,const std::string& raceEpoch,const std::string& rosterRevision)const
{
    try {
        const J j=Parser(body).parse();
        closed(j,{"contract","profile","expectedRaceEpoch","expectedInputSessionId","expectedRosterRevision"});
        if(text(j,"contract")!="tholus.stk-scintilla-combat/1"||text(j,"profile")!="crew_standard_sc2_v1")return error(400,"invalid_combat_profile");
        if(tok(j,"expectedRaceEpoch")!=raceEpoch||tok(j,"expectedInputSessionId")!=m->session||tok(j,"expectedRosterRevision")!=rosterRevision)return error(409,"combat_identity_changed");
        return ok("combat_identity_valid");
    } catch(const std::exception&){return error(400,"invalid_combat_prepare");}
}
Reply Runtime::validateCombatIntent(const std::string& body,const std::string& raceEpoch,const std::string& intentRevision,bool* required)const
{
    try {
        const J j=Parser(body).parse();
        closed(j,{"contract","expectedRaceEpoch","expectedIntentRevision","required"});
        if(text(j,"contract")!="tholus.stk-scintilla-combat/1"||!required||at(j,"required").kind!=J::Bool)
            throw Invalid("invalid_combat_intent");
        if(tok(j,"expectedRaceEpoch")!=raceEpoch||tok(j,"expectedIntentRevision")!=intentRevision)
            return Reply(409,"{\"ok\":false,\"error\":\"combat_identity_changed\"}");
        *required=at(j,"required").boolean;
        return Reply(200,"{\"ok\":true}");
    } catch(const std::exception&){return Reply(400,"{\"ok\":false,\"error\":\"invalid_combat_intent\"}");}
}
bool Runtime::prepared()const{return m->prepared&&!m->inputMaintenance&&m->participantBindingsValid;}
bool Runtime::hasPreparedGrant()const{return m->prepared;}
bool Runtime::inputMaintenanceActive()const{return m->prepared&&m->inputMaintenance;}
bool Runtime::inputMaintenanceMatches(const std::string& id)const
{return inputMaintenanceActive()&&id==m->inputMaintenanceID&&m->inputEpoch==m->inputMaintenanceTo;}
Reply Runtime::inputMaintenance(const std::string& operation,const std::string& body,int64_t now)
{
    const auto fail=[](int status,const std::string& reason){return Reply(status,"{\"ok\":false,\"error\":"+quote(reason)+"}");};
    try{
        using namespace ScintillaMissionProtocol;
        if(body.size()>8192||now<0||now>=SAFE)throw Invalid("input_maintenance_request_size_or_clock");
        const J j=Parser(body).parse();
        if(operation=="begin")closed(j,{"contract","schemaVersion","producerUUID","raceEpoch","inputSessionID","routingGeneration","expectedInputEpoch","operationID"});
        else if(operation=="resume")closed(j,{"contract","schemaVersion","producerUUID","raceEpoch","inputSessionID","routingGeneration","expectedInputEpoch","operationID","expiresAtHostUs","clockSamples"});
        else return fail(404,"input_maintenance_unknown_operation");
        if(text(j,"contract")!="tholus.stk-scintilla-input-maintenance/1"||num(j,"schemaVersion")!=1||!uuid(text(j,"producerUUID"))||!uuid(text(j,"operationID")))throw Invalid("input_maintenance_contract");
        const auto race=decimal(at(j,"raceEpoch")),epoch=decimal(at(j,"expectedInputEpoch"));
        if(race=="0")throw Invalid("input_maintenance_race");
        if(m->lastNow&&now<m->lastNow)throw Invalid("input_maintenance_clock_reversed");
        m->contextCheck(j);if(!m->prepared)return fail(409,"input_maintenance_grant_unavailable");
        const std::string id=text(j,"operationID");
        const auto reply=[&](){std::ostringstream o;o<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-input-maintenance/1\",\"schemaVersion\":1,\"producerUUID\":"<<quote(text(j,"producerUUID"))<<",\"raceEpoch\":"<<quote(race)<<",\"inputSessionID\":"<<quote(m->session)<<",\"routingGeneration\":"<<quote(m->generation)<<",\"inputEpoch\":\""<<m->inputEpoch<<"\",\"operationID\":"<<quote(id)<<",\"suspended\":"<<(m->inputMaintenance?"true":"false")<<'}';return Reply(200,o.str());};
        if(operation=="begin"){
            // Exact retry returns the current ticket, including a completed
            // ticket, but can never reopen or extend the transaction.
            if(id==m->inputMaintenanceID){
                if(epoch!=std::to_string(m->inputMaintenanceFrom)||m->inputEpoch!=m->inputMaintenanceTo)return fail(409,"input_maintenance_identity_changed");
                if(m->inputMaintenance)return reply();
                if(m->inputMaintenanceCompleted){
                    if(now>=m->leaseUntil||now<m->clockChecked||now-m->clockChecked>1000000)return fail(409,"input_maintenance_grant_expired");
                    return reply();
                }
                return fail(409,"input_maintenance_operation_retired");
            }
            if(m->inputMaintenance)return fail(409,"input_maintenance_operation_conflict");
            if(epoch!=std::to_string(m->inputEpoch))return fail(409,"input_maintenance_identity_changed");
            if(now>=m->leaseUntil||now<m->clockChecked||now-m->clockChecked>1000000)return fail(409,"input_maintenance_grant_expired");
            if(!m->allowRebind||m->inputEpoch==UINT64_MAX)return fail(409,"input_maintenance_unavailable");
            m->stop("input_maintenance");m->active=false;m->historyReady=false;m->participantBindingsValid=false;
            if(!m->frames.empty())m->retiredThrough=std::max(m->retiredThrough,m->frames.back().id);
            m->frames.clear();m->presented.clear();m->proofHostFrom=m->proofHostUntil=0;
            m->activationNotBefore=std::max(m->activationNotBefore,now+1);
            m->inputMaintenanceID=id;m->inputMaintenanceFrom=m->inputEpoch;++m->inputEpoch;m->inputMaintenanceTo=m->inputEpoch;
            m->captureBindingContinuity();
            m->inputMaintenance=true;m->inputMaintenanceCompleted=false;m->lastNow=now;
            return reply();
        }
        if(id!=m->inputMaintenanceID||epoch!=std::to_string(m->inputMaintenanceTo)||m->inputEpoch!=m->inputMaintenanceTo)return fail(409,"input_maintenance_identity_changed");
        // Validate every resume payload even for a retry; retry validation is
        // observational and must never turn malformed clocks into success.
        arr(at(j,"clockSamples"),3,3);int64_t low=0,high=0;m->clocks(at(j,"clockSamples"),now,low,high);
        const int64_t expires=num(j,"expiresAtHostUs");
        // A repeated completed resume must not extend a grant using a replayed
        // renewal. Once normally expired, it cannot resurrect that grant.
        if(m->inputMaintenanceCompleted&&!m->inputMaintenance){
            if(now>=m->leaseUntil||now<m->clockChecked||now-m->clockChecked>1000000)return fail(409,"input_maintenance_grant_expired");
            return reply();
        }
        if(!m->inputMaintenance)return fail(409,"input_maintenance_not_suspended");
        const int64_t drift=(now-m->clockChecked+9999)/10000+1;
        low=std::max(low,m->clockLow-drift);high=std::min(high,m->clockHigh+drift);
        if(low>high)throw Invalid("clock_offset_jump");
        const int64_t until=expires+low;
        if(until<=now||until>SAFE||until-now>10000000)throw Invalid("invalid_renewal");
        m->clockLow=low;m->clockHigh=high;m->clockChecked=now;m->leaseUntil=until;m->lastNow=now;
        m->inputMaintenance=false;m->inputMaintenanceCompleted=true;m->active=false;m->historyReady=false;
        m->stop("input_maintenance_resumed");m->presented.clear();m->frames.clear();m->proofHostFrom=m->proofHostUntil=0;
        m->activationNotBefore=std::max(m->activationNotBefore,now+1);
        return reply();
    }catch(const std::exception&){return fail(400,"input_maintenance_invalid_request");}
}
void Runtime::allowParticipantRebind(bool enabled){m->allowRebind=enabled;}
Reply Runtime::rebindParticipants(const std::string&body,const std::string&rosterRevision,int64_t now)
{
    try{
        const J j=Parser(body).parse();closed(j,{"contract","inputSessionID","routingGeneration","expectedInputEpoch","expectedRosterRevision","bindings"});
        if(text(j,"contract")!="tholus.stk-spatial/1")throw Invalid("invalid_contract");m->contextCheck(j);
        if(m->inputMaintenance||!m->allowRebind||!m->prepared||now<0||now>=m->leaseUntil||now<m->clockChecked||now-m->clockChecked>1000000)return error(409,"participant_rebind_unavailable");
        if(tok(j,"expectedRosterRevision")!=rosterRevision)return error(409,"participant_rebind_identity_changed");
        std::map<std::string,std::string> bindings;std::set<std::string> participants;
        for(const auto&b:arr(at(j,"bindings"),m->sources.size(),m->sources.size())){closed(b,{"sourceId","participantId"});const auto id=tok(b,"sourceId"),person=tok(b,"participantId");
            m->sourceIndex(id);if(!bindings.insert({id,person}).second||!participants.insert(person).second)throw Invalid("invalid_participant_binding");}
        if(!m->participantsKnown||participants!=m->expectedParticipants)return error(409,"participant_roster_mismatch");
        const auto reply=[&](){return Reply(200,"{\"ok\":true,\"accepted\":true,\"inputEpoch\":\""+std::to_string(m->inputEpoch)+"\",\"inputSessionID\":"+quote(m->session)+"}");};
        if(tok(j,"expectedInputEpoch")!=std::to_string(m->inputEpoch)){
            bool same=m->inputEpoch==m->lastRebindTo&&tok(j,"expectedInputEpoch")==std::to_string(m->lastRebindFrom)&&m->participantBindingsValid;
            for(const auto&s:m->sources)same=same&&bindings[s.identity.source]==s.participant;
            return same?reply():error(409,"participant_rebind_identity_changed");
        }
        if(m->inputEpoch==UINT64_MAX||now>=SAFE)return error(409,"input_epoch_exhausted");
        // Physical authorization remains SI5-owned. Only the logical crew map changes.
        m->stop("participants_rebound");m->active=false;m->activationNotBefore=std::max(m->activationNotBefore,now+1);
        for(size_t i=0;i<m->sources.size();++i){m->sources[i].participant=bindings[m->sources[i].identity.source];m->reticles[i].participantId=m->sources[i].participant;m->sources[i].pressLocked=false;}
        m->participantBindingsValid=true;m->lastRebindFrom=m->inputEpoch;++m->inputEpoch;m->lastRebindTo=m->inputEpoch;
        m->captureBindingContinuity();
        return reply();
    }catch(const std::exception&){return error(400,"invalid_participant_rebind");}
}
std::vector<BindingState> Runtime::bindingStates() const
{
    std::vector<BindingState> out;
    for(const auto& s:m->sources) {
        BindingState b;b.sourceId=s.identity.source;b.participantId=s.participant;
        b.deviceBindingRevision=s.identity.binding;b.trackingSpaceEpoch=s.identity.epoch;
        b.poseContinuityId=s.identity.continuity;b.roomCalibrationRevision=s.identity.room;b.gripCalibrationRevision=s.identity.grip;
        b.trackingLossRevision=s.trackingLossRevision;b.tracked=s.tracked;
        b.continuityIntact=s.trackingLossRevision!=UINT64_MAX&&s.trackingLossRevision==s.bindingLossRevision&&
            s.identity.continuity==s.bindingContinuity;
        b.neutral=s.observedKnown[0]&&!s.observedHeld[0]&&s.observedKnown[1]&&!s.observedHeld[1]&&
            (!m->capabilities.count("gripAction")||(s.observedKnown[2]&&!s.observedHeld[2]));out.push_back(b);
    }
    return out;
}
uint64_t Runtime::inputEpoch()const{return m->inputEpoch;}
const std::string& Runtime::inputSessionID()const{return m->session;}
std::vector<std::string> Runtime::participants()const
{std::vector<std::string> out;for(const auto& s:m->sources)out.push_back(s.participant);std::sort(out.begin(),out.end());return out;}
void Runtime::setCombatContext(bool enabled,int64_t tick,const std::vector<ScintillaCombat::Snapshot>& targets,uint64_t raceEpoch)
{
    m->combatRaceEpoch=raceEpoch;m->combatEnabled=enabled;m->simulationTick=tick;m->combatTargets=targets;
    if(!enabled)for(auto& r:m->reticles)r.toolFeedback=ScintillaCombat::ToolSnapshot();
}
void Runtime::setMissionContext(bool enabled,uint64_t revision,
    const std::vector<ScintillaMission::Snapshot>& targets,
    const std::vector<ScintillaMission::Feedback>& feedback)
{
    const bool admitted=enabled&&revision&&targets.size()<=3;
    if(m->missionEnabled!=admitted||m->missionRevision!=revision)
        for(size_t i=0;i<m->sources.size();++i)
            if(missionKind(m->sources[i].proofKind)||missionKind(m->sources[i].pressKind))
                m->clearControls(i,"mission_context_changed");
    m->missionEnabled=admitted;m->missionRevision=revision;
    m->missionTargets=admitted?targets:std::vector<ScintillaMission::Snapshot>();
    for(size_t i=0;i<m->sources.size();++i)
    {
        const auto& s=m->sources[i];ScintillaMission::Feedback value;
        for(const auto& f:feedback)
            if(f.participant==s.participant&&f.controlEpoch==s.toolEpoch)value=f;
        m->setMissionFeedback(i,value);
    }
}
void Runtime::setMissionLayoutRequired(bool required)
{
    if(m->missionLayoutRequired==required)return;
    // No old pending query or already admitted hit may cross the revision
    // boundary. Keep the physical grant and producer history for fresh proof.
    m->stop("layout_context_changed");m->missionLayoutRequired=required;
}
void Runtime::setCrewContext(bool required,bool partyEnabled,uint64_t race,uint64_t turn,
    uint64_t intent,uint64_t proposal,uint64_t roster)
{
    const bool enabled=required&&partyEnabled;
    if(m->crewEnabled!=required||(required&&(m->partyEnabled!=enabled||m->crewRace!=race||m->crewTurn!=turn||
       m->crewIntent!=intent||m->crewRoster!=roster||m->crewObservedInput!=m->inputEpoch))){
        m->stop("crew_context_changed");
        if(m->lastNow<SAFE)m->activationNotBefore=std::max(m->activationNotBefore,m->lastNow+1);
    }
    m->crewEnabled=required;m->partyEnabled=enabled;m->crewRace=race;m->crewTurn=turn;
    m->crewIntent=intent;m->crewProposal=proposal;m->crewRoster=roster;m->crewObservedInput=m->inputEpoch;
}
std::vector<ScintillaCrew::PartyBeam> Runtime::partyBeams(int64_t now)const
{
    std::vector<ScintillaCrew::PartyBeam> out;
    if(!m->crewEnabled||!m->prepared||!m->participantBindingsValid||!m->active||!m->historyReady||
       now<m->clockChecked||now-m->clockChecked>1000000||now>=m->leaseUntil)return out;
    const auto authority=m->crewAuthority();
    for(const auto& s:m->sources) {
        if(!s.tracked)continue;
        ScintillaCrew::PartyBeam b;b.participantId=s.participant;b.controlEpoch=s.toolEpoch;
        b.releaseSerial=s.releaseInputEpoch==m->inputEpoch?s.toolReleaseSerial:0;
        b.held=s.held[1]&&s.toolPressSerial;b.serial=b.held?s.toolPressSerial:0;
        b.authority=b.held?s.partyPressAuthority:authority;
        b.admittedTick=b.held?s.partyPressTick:m->simulationTick;
        b.neutral=!s.held[1]&&s.observedKnown[1]&&!s.observedHeld[1]&&b.releaseSerial;
        b.partyContext=s.pressSurface.context==ScintillaDefense::Context::PartyBoost;
        b.capturedProposalId=s.partyCapturedProposal;
        // Only the actual reliable initial proof can be pending. Disarming,
        // negative resolution or a merely missing press lock is not a query.
        if(b.held&&!s.pressLocked&&!s.proofInterrupted&&s.partyPressAuthority==authority)
            for(const auto& p:m->pending)if(p.event.sourceId==s.identity.source&&p.event.control=="secondary"&&
                p.event.inputEpoch==m->inputEpoch&&p.event.serial==s.toolPressSerial&&p.toolEpoch==s.toolEpoch&&
                p.crewAuthority==s.partyPressAuthority){b.pending=true;break;}
        b.interrupted=s.proofInterrupted||(b.held&&!b.pending&&(!s.pressLocked||!s.partyPressEligible||
            s.proofSurface.context!=ScintillaDefense::Context::PartyBoost));
        const int64_t drift=(now-m->clockChecked+9999)/10000,low=s.proofHostLow+m->clockLow-drift;
        b.visible=b.held&&m->partyEnabled&&s.partyPressAuthority==authority&&s.partyPressEligible&&!b.interrupted&&
            s.proofVisible&&s.proofSurface.context==ScintillaDefense::Context::PartyBoost&&
            low>=m->issuedAt&&low<=now&&now-low<=60000;
        out.push_back(b);
    }return out;
}
void Runtime::setPartyFeedback(const std::vector<ScintillaCrew::PartyFeedback>& feedback)
{
    for(size_t i=0;i<m->sources.size();++i) {
        const auto& s=m->sources[i];ScintillaCrew::PartyFeedback value;
        for(const auto& f:feedback)if(f.participantId==s.participant&&f.controlEpoch==s.toolEpoch)value=f;
        m->setPartyToolFeedback(i,value);
    }
}
ScintillaCrew::PartyFeedback Runtime::partyFeedbackForRendering(const Reticle& r)
{
    const auto& f=r.partyFeedback;
    const bool partyPoint=r.intent.context==ScintillaDefense::Context::PartyBoost;
    if(!r.valid||!r.toolHeld||(!partyPoint&&f.reason!=ScintillaCrew::PartyReason::ReleaseRequired)||
       !std::isfinite(r.u)||!std::isfinite(r.v)||r.u<0||r.u>1||r.v<0||r.v>1||
       f.participantId!=r.participantId||f.controlEpoch!=r.toolEpoch||
       !std::isfinite(f.progress)||f.progress<0||f.progress>1)return ScintillaCrew::PartyFeedback();
    auto current=f;
    if(!partyPoint||current.reason==ScintillaCrew::PartyReason::ReleaseRequired){current.progress=0;current.consenting=false;}
    else current.consenting=current.consenting&&current.progress>=1&&current.reason==ScintillaCrew::PartyReason::Ready;
    return current;
}
std::vector<HitEvent> Runtime::drainCombatShots()
{
    std::vector<HitEvent> out;
    while(!m->combatShots.empty()&&m->combatShots.front().result!="pending"){
        out.push_back(std::move(m->combatShots.front()));m->combatShots.pop_front();
    }
    return out;
}
std::vector<BeamProof> Runtime::beamProofs(int64_t now)const
{
    std::vector<BeamProof> out;
    for(const auto& s:m->sources){
        BeamProof b;b.sourceId=s.identity.source;b.participantId=s.participant;b.controlEpoch=s.toolEpoch;
        b.held=m->prepared&&m->active&&s.tracked&&s.held[1]&&!missionKind(s.proofKind)&&
            (!m->missionEnabled||s.pressLocked)&&
            (!m->crewEnabled||(s.pressLocked&&s.proofSurface.context==ScintillaDefense::Context::WorldOffense))&&
            (!m->defenseEnabled||(s.pressLocked&&s.proofSurface.context==ScintillaDefense::Context::WorldOffense));b.target=s.proofTarget;b.instance=s.proofInstance;
        const int64_t drift=now>=m->clockChecked?(now-m->clockChecked+9999)/10000:0;
        const int64_t low=s.proofHostLow+m->clockLow-drift;
        b.visible=b.held&&!s.proofInterrupted&&s.proofVisible&&low>=m->issuedAt&&low<=now&&now-low<=60000;
        out.push_back(b);
    }
    return out;
}
std::vector<ScintillaMission::Beam> Runtime::missionBeams(int64_t now)const
{
    std::vector<ScintillaMission::Beam> out;
    if(!m->missionEnabled||!m->prepared||!m->participantBindingsValid||!m->active||
       now<m->clockChecked||now-m->clockChecked>1000000||now>=m->leaseUntil)return out;
    uint64_t firstPending=std::numeric_limits<uint64_t>::max();
    for(const auto& p:m->pending)
        if(p.event.control=="secondary"&&p.event.serial&&p.event.inputEpoch==m->inputEpoch){
            const auto& s=m->sources[m->sourceIndex(p.event.sourceId)];
            if(s.held[1]&&s.toolEpoch==p.toolEpoch)
                firstPending=std::min(firstPending,p.event.serial);
        }
    for(const auto& s:m->sources){
        if(!s.tracked)continue;
        ScintillaMission::Beam b;b.participant=s.participant;
        b.race=m->combatRaceEpoch;b.revision=m->missionRevision;b.inputEpoch=m->inputEpoch;
        b.controlEpoch=s.toolEpoch;
        b.releaseSerial=s.releaseInputEpoch==m->inputEpoch?s.toolReleaseSerial:0;
        if(!s.held[1]){
            // Only a real admitted UP edge is a release sample. A cleared
            // context, a pending proof or a tracking loss is never a release.
            if(!s.rearmed[1]||!b.releaseSerial)continue;
            b.serial=b.releaseSerial;out.push_back(b);continue;
        }
        if(!s.pressLocked||!s.toolPressSerial||s.toolPressSerial>firstPending)continue;
        b.held=true;b.serial=s.toolPressSerial;
        b.kind=s.pressKind==TargetKind::MissionTether?ScintillaMission::Kind::Tether:ScintillaMission::Kind::Capsule;
        b.entity=s.pressMission.id;b.instance=s.pressMission.instance;b.phaseRevision=s.pressMission.phaseRevision;
        b.interrupted=s.proofInterrupted||!missionKind(s.pressKind)||
            s.pressKind!=s.proofKind||!sameMission(s.pressMission,s.proofMission);
        const bool current=std::any_of(m->missionTargets.begin(),m->missionTargets.end(),[&](const ScintillaMission::Snapshot& target){
            return target.eligible&&sameMission(target,s.pressMission);
        });
        b.historicalEligible=s.pressMissionEligible&&current;
        const int64_t drift=now>=m->clockChecked?(now-m->clockChecked+9999)/10000:0;
        const int64_t low=s.proofHostLow+m->clockLow-drift;
        b.visible=b.historicalEligible&&!b.interrupted&&s.proofVisible&&
            s.proofSurface.context==ScintillaDefense::Context::WorldOffense&&
            low>=m->issuedAt&&low<=now&&now-low<=60000;
        out.push_back(b);
    }
    return out;
}
ScintillaMission::Feedback Runtime::missionFeedbackForRendering(const Reticle& r)
{
    return !r.valid||!r.toolHeld||!missionKind(r.targetKind)||
        r.missionFeedback.participant!=r.participantId||r.missionFeedback.controlEpoch!=r.toolEpoch
        ?ScintillaMission::Feedback():r.missionFeedback;
}
std::vector<ScintillaDefense::Beam> Runtime::defenseBeams(int64_t now)const
{
    std::vector<ScintillaDefense::Beam> out;
    if(!m->defenseEnabled)return out;
    for(const auto& s:m->sources){
        ScintillaDefense::Beam b;b.participant=s.participant;b.epoch=s.toolEpoch;b.serial=s.proofSerial;
        b.context=s.proofSurface.context;b.sector=s.proofSurface.sector;b.threat=s.proofThreat;b.admitted=s.proofAdmitted;
        b.held=m->prepared&&m->active&&s.tracked&&s.held[1]&&s.pressLocked&&s.proofSerial>0;
        const int64_t drift=now>=m->clockChecked?(now-m->clockChecked+9999)/10000:0,low=s.proofHostLow+m->clockLow-drift;
        b.visible=b.held&&s.pressLocked&&(b.context==ScintillaDefense::Context::DefenseSector||b.context==ScintillaDefense::Context::ServiceBoost||b.context==ScintillaDefense::Context::WorldOffense)&&!s.proofInterrupted&&s.proofVisible&&low>=m->issuedAt&&low<=now&&now-low<=60000;
        out.push_back(b);
    }return out;
}
void Runtime::setDefenseContext(bool enabled,const std::vector<ScintillaDefense::ToolFeedback>& feedback)
{
    if(m->defenseEnabled!=enabled)for(size_t i=0;i<m->sources.size();++i){m->sources[i].pressLocked=false;m->sources[i].proofVisible=false;}
    m->defenseEnabled=enabled;
    for(size_t i=0;i<m->sources.size();++i){
        auto& r=m->reticles[i];const auto&s=m->sources[i];r.defenseFeedback=ScintillaDefense::ToolFeedback();
        if(!enabled||!r.valid||!r.toolHeld)continue;
        for(const auto& f:feedback)if(f.participant==s.participant&&f.epoch==s.toolEpoch)r.defenseFeedback=f;
        if(s.proofInterrupted){r.defenseFeedback.participant=s.participant;r.defenseFeedback.epoch=s.toolEpoch;r.defenseFeedback.status=ScintillaDefense::Status::ReleaseRequired;}
    }
}
bool Runtime::defenseVideoReady(int64_t now,bool allowHeld)const
{
    if(!m->prepared||(!allowHeld&&!m->active)||!m->historyReady||now<m->clockChecked||now-m->clockChecked>1000000)return false;
    if((m->defenseEnabled||m->missionLayoutRequired)&&presentedDefenseFrames(now,allowHeld).empty())return false;
    for(size_t i=1;i<m->presented.size();++i)if(m->presented[i].hostLo>m->presented[i-1].hostHi+1&&m->presented[i].hostLo+m->clockHigh>=now-150000)return false;
    return true;
}
std::vector<PresentedFrame> Runtime::presentedDefenseFrames(int64_t now,bool allowHeld)const
{
    std::vector<PresentedFrame> out;if(!m->prepared||(!allowHeld&&!m->active)||!m->historyReady||now<m->clockChecked||now-m->clockChecked>1000000||now>=m->leaseUntil)return out;
    const int64_t drift=(now-m->clockChecked+9999)/10000;
    for(auto it=m->presented.rbegin();it!=m->presented.rend()&&out.size()<8;++it){
        const int64_t lo=it->hostLo+m->clockLow-drift,hi=it->hostHi+m->clockHigh+drift;
        if(hi>now||lo<m->issuedAt||now-lo>150000)continue;
        auto f=std::find_if(m->frames.begin(),m->frames.end(),[&](const Frame&f){return f.id==it->id&&f.stream==m->stream&&f.routeRevision==m->routeRevision&&f.projectionContextId==m->context;});
        if(f==m->frames.end()||!f->defense.enabled||!m->layoutMatches(*f))continue;
        PresentedFrame p;p.frame=*f;p.low=lo;p.high=hi;out.push_back(p);
    }return out;
}
std::vector<PresentedFrame> Runtime::presentedMissionFrames(int64_t now)const
{
    std::vector<PresentedFrame> out;
    // Actual presentation may recover while gameplay is held. Optics, grant,
    // route and fresh clocks remain mandatory; no gameplay timer advances here.
    if(!m->missionEnabled||!m->prepared||!m->historyReady||now<m->clockChecked||
       now-m->clockChecked>1000000||now>=m->leaseUntil)return out;
    const int64_t drift=(now-m->clockChecked+9999)/10000;
    for(auto it=m->presented.rbegin();it!=m->presented.rend()&&out.size()<8;++it){
        const int64_t low=it->hostLo+m->clockLow-drift,high=it->hostHi+m->clockHigh+drift;
        if(high>now||low<m->issuedAt||now-low>150000)continue;
        auto f=std::find_if(m->frames.begin(),m->frames.end(),[&](const Frame& frame){
            return frame.id==it->id&&frame.stream==m->stream&&frame.routeRevision==m->routeRevision&&
                frame.projectionContextId==m->context&&frame.missionEnabled&&
                frame.missionRaceEpoch==m->combatRaceEpoch&&frame.missionRevision==m->missionRevision&&
                m->layoutMatches(frame);
        });
        if(f==m->frames.end())continue;
        PresentedFrame p;p.frame=*f;p.low=low;p.high=high;out.push_back(p);
    }
    return out;
}
Reply Runtime::validateDefenseIntent(const std::string& body,const std::string& race,const std::string& revision,bool* required)const
{
    try{const J j=Parser(body).parse();closed(j,{"contract","raceEpoch","intentRevision","required"});
        if(text(j,"contract")!="tholus.stk-scintilla-defense/1"||!required||at(j,"required").kind!=J::Bool)throw Invalid("invalid_defense_intent");
        if(tok(j,"raceEpoch")!=race||tok(j,"intentRevision")!=revision)return error(409,"defense_identity_changed");
        *required=at(j,"required").boolean;return ok();
    }catch(const std::exception&){return error(400,"invalid_defense_intent");}
}
Reply Runtime::validateDefensePrepare(const std::string& body,const std::string& race,const std::string& roster,DefenseAdmission* admission,bool missionLayout)const
{
    try{const J j=Parser(body).parse();closed(j,{"contract","profile","inputSessionID","inputEpoch","raceEpoch","rosterRevision","combatProfile","layoutProfile","layoutRevision","routeRevision","projectionContextId","visibilityEvidenceID","visibleRegions"});
        const auto expected=ScintillaDefense::Layout::create(m->projection.width,m->projection.height,missionLayout,false,m->crewEnabled);
        if(!admission||text(j,"contract")!="tholus.stk-scintilla-defense/1"||text(j,"profile")!="sc3_sector_rear_readonly_v1"||text(j,"combatProfile")!="crew_standard_sc2_v1"||text(j,"layoutProfile")!=expected.profile()||text(j,"layoutRevision")!=std::to_string(expected.revision()))throw Invalid("invalid_defense_profile");
        if(tok(j,"raceEpoch")!=race||tok(j,"rosterRevision")!=roster||tok(j,"inputSessionID")!=m->session||tok(j,"inputEpoch")!=std::to_string(m->inputEpoch)||tok(j,"routeRevision")!=m->routeRevision||tok(j,"projectionContextId")!=m->context)return error(409,"defense_identity_changed");
        DefenseAdmission a;a.evidence=tok(j,"visibilityEvidenceID");
        for(const auto& r:arr(at(j,"visibleRegions"),1,8)){closed(r,{"left","top","right","bottom"});double l=real(at(r,"left"),0,1),t=real(at(r,"top"),0,1),rr=real(at(r,"right"),0,1),b=real(at(r,"bottom"),0,1);if(l>=rr||t>=b)throw Invalid("invalid_visible_region");a.regions.insert(a.regions.end(),{l,t,rr,b});}
        *admission=a;return ok();
    }catch(const std::exception&){return error(400,"invalid_defense_prepare");}
}
void Runtime::setCombatToolFeedback(const std::vector<ScintillaCombat::ToolSnapshot>& snapshots)
{
    for(size_t i=0;i<m->sources.size();++i){
        const auto& s=m->sources[i];
        const auto found=std::find_if(snapshots.begin(),snapshots.end(),[&](const ScintillaCombat::ToolSnapshot& p){return p.participant==s.participant&&p.controlEpoch==s.toolEpoch;});
        m->setToolFeedback(i,found==snapshots.end()?ScintillaCombat::ToolSnapshot():*found);
    }
}
ScintillaCombat::ToolSnapshot Runtime::toolFeedbackForRendering(const Reticle& r)
{
    return !r.valid||!r.toolHeld||r.toolFeedback.participant!=r.participantId||r.toolFeedback.controlEpoch!=r.toolEpoch
        ?ScintillaCombat::ToolSnapshot():r.toolFeedback;
}
const char* Runtime::toolFeedbackReason(ScintillaCombat::ToolStatus status)
{
    switch(status){
        case ScintillaCombat::ToolStatus::ReleaseRequired:return "tool_release_required";
        case ScintillaCombat::ToolStatus::TargetBusy:return "target_busy";
        default:return "";
    }
}
const char* Runtime::toolFeedbackInstruction(ScintillaCombat::ToolStatus status)
{
    switch(status){
        case ScintillaCombat::ToolStatus::ReleaseRequired:return "Release the tool button, then press it again.";
        case ScintillaCombat::ToolStatus::TargetBusy:return "Another player controls this breach. Fire when the shield opens, or release the tool and choose another target.";
        default:return "";
    }
}
void Runtime::combatFeedback(const HitEvent& e,FeedbackKind kind,const std::string& reason,int64_t now)
{
    for(size_t i=0;i<m->sources.size();++i)if(m->sources[i].identity.source==e.sourceId&&m->sources[i].feedbackSequence==e.actionSequence){
        Feedback& f=m->reticles[i].feedback;f.kind=kind;f.reason=reason;f.startedUs=now;f.untilUs=now+500000;
    }
}
void Runtime::setParticipants(const std::vector<std::string>& allowedCrewIDs)
{
    const std::set<std::string> next(allowedCrewIDs.begin(),allowedCrewIDs.end());
    if(allowedCrewIDs.size()!=next.size()||next.size()>5)
    {m->expectedParticipants.clear();m->participantsKnown=true;m->invalidate("invalid_crew_roster");return;}
    bool changed=m->participantsKnown&&next!=m->expectedParticipants;
    if(m->prepared)
    {
        std::set<std::string> current;for(const auto& source:m->sources)current.insert(source.participant);
        changed=changed||current!=next;
    }
    if(changed){
        if(m->prepared&&m->allowRebind&&next.size()==m->sources.size()){
            if(m->participantBindingsValid||next!=m->expectedParticipants)m->stop("participant_rebind_required");m->active=false;m->participantBindingsValid=false;
        }else m->invalidate("crew_roster_changed");
    }
    m->expectedParticipants=next;m->participantsKnown=true;
}
void Runtime::setProducerProjection(const Projection& p)
{
    if(m->prepared&&!sameProjection(p,m->projection))m->invalidate("producer_projection_changed");
    m->producerProjection=p;m->producerProjectionKnown=true;
}
Feedback Runtime::feedbackForRendering(const Reticle& reticle,int64_t now)
{
    const Feedback& feedback=reticle.feedback;
    return now<feedback.startedUs||now>=feedback.untilUs?Feedback():feedback;
}
bool Runtime::projectionDirection(const Projection& p,double u,double v,Vec3* out)
{
    if(!out||!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)return false;
    Vec3 d;
    if(p.kind=="equirectangular")
    {
        double longitude=(u-0.5)*2*PI,latitude=(0.5-v)*PI;
        d=Vec3(std::sin(longitude)*std::cos(latitude),std::sin(latitude),std::cos(longitude)*std::cos(latitude));
    }
    else if(p.kind=="fisheye_domemaster")
    {
        double x=2*u-1,y=1-2*v,r=std::sqrt(x*x+y*y);if(r>1)return false;
        double theta=r*PI*0.5;
        d=r<0.00001?Vec3(0,0,1):Vec3(std::sin(theta)*x/r,std::sin(theta)*y/r,std::cos(theta));
        double a=p.domeTiltDeg*PI/180,c=std::cos(a),s=std::sin(a);
        d=Vec3(d.x,c*d.y-s*d.z,s*d.y+c*d.z);
    }
    else return false;
    *out=d;return unit(d);
}
void Runtime::addFrame(const Frame& supplied)
{
    Frame f=supplied;f.visibilityLease.reset();
    if(!m->prepared||m->inputMaintenance)return;
    if(f.routeRevision.empty())f.routeRevision=m->routeRevision;
    if(f.projectionContextId.empty())f.projectionContextId=m->context;
    bool streamOK=f.stream.size()==32;for(char c:f.stream)streamOK=streamOK&&((c>='0'&&c<='9')||(c>='a'&&c<='f'));
    if(!streamOK||!f.id||f.captureUs<0||!finite(f.cameraPosition)||!unit(f.cameraRight)||!unit(f.cameraUp)||!unit(f.cameraForward)||
       std::abs(dot(f.cameraRight,f.cameraUp))>0.002||std::abs(dot(f.cameraRight,f.cameraForward))>0.002||std::abs(dot(f.cameraUp,f.cameraForward))>0.002||
       !std::isfinite(f.nearPlane)||!std::isfinite(f.farPlane)||!std::isfinite(f.faceFovDeg)||f.nearPlane<=0||f.farPlane<=f.nearPlane||f.faceFovDeg<90||f.faceFovDeg>120||
       f.targets.size()>256||f.crewAssignments.size()>ScintillaCrew::Profile::MaxParticipants||
       f.crewSuggestions.size()>ScintillaCrew::Profile::MaxSuggestions||f.crewReleaseAdvice.size()>ScintillaCrew::Profile::MaxCrew||
       !sameProjection(f.projection,m->projection)||f.routeRevision!=m->routeRevision||f.projectionContextId!=m->context)
    {m->invalidate("invalid_producer_frame");return;}
    if(f.id<=m->retiredThrough){m->invalidate("producer_frame_reused");return;}
    if(!m->frames.empty()&&(f.stream!=m->frames.back().stream||f.generation!=m->frames.back().generation||f.id<=m->frames.back().id||f.captureUs<m->frames.back().captureUs))
    {m->invalidate("producer_epoch_changed");return;}
    std::set<uint64_t> ids;
    for(const auto& t:f.targets)if(!t.id||!t.instance||!finite(t.center)||!std::isfinite(t.radius)||t.radius<=0||t.radius>100||!ids.insert(t.id).second)
    {m->invalidate("invalid_producer_target");return;}
    if(!m->stream.empty()&&f.stream!=m->stream){m->invalidate("producer_stream_changed");return;}
    m->stream=f.stream;m->frames.push_back(f);
    // Producer GPU retention is separately bounded. Missing depth fails closed.
    while(m->frames.size()>64)
    {
        const uint64_t id=m->frames.front().id;m->retiredFrames.push_back(id);m->retiredFrameIds.insert(id);m->frames.pop_front();
        if(m->retiredFrames.size()>256){m->retiredFrameIds.erase(m->retiredFrames.front());m->retiredFrames.pop_front();}
    }
}
void Runtime::tick(int64_t now,bool gameplayActive,uint64_t resumeRevision,int64_t resumedAt)
{
    if(now<0||now>SAFE){m->invalidate("invalid_clock");return;}
    if(m->lastNow&&now<m->lastNow){m->invalidate("clock_reversed");return;}m->lastNow=now;
    m->pruneEvents(now);
    for(auto& reticle:m->reticles)if(reticle.feedback.kind!=FeedbackKind::Pending&&now>=reticle.feedback.untilUs)reticle.feedback=Feedback();
    // Explicit maintenance is disarmed and held by the engine. Only this
    // transaction skips elapsed native lease/clock expiry during bounded I/O;
    // resume needs a fresh authoritative SI5 lease and three clock probes.
    if(m->inputMaintenance){m->active=false;return;}
    if(m->prepared&&(now>=m->leaseUntil||now-m->clockChecked>1000000))m->invalidate("lease_or_clock_expired");
    if(resumeRevision!=m->resumeRevision)
    {
        if(!resumeRevision||resumedAt<=0||resumedAt>now)
        {m->invalidate("invalid_gameplay_activation");return;}
        m->resumeRevision=resumeRevision;m->activationNotBefore=std::max(m->activationNotBefore,resumedAt);
        m->stop("gameplay_resumed");
    }
    else if(m->hasGameplayObservation&&!m->active&&gameplayActive)
    {
        // No producer lifecycle metadata: refuse everything acquired before this observed resume.
        m->activationNotBefore=now;m->stop("gameplay_resumed");
    }
    if(m->active&&!gameplayActive)m->stop("gameplay_suspended");
    m->hasGameplayObservation=true;m->active=gameplayActive;
    for(size_t i=0;i<m->reticles.size();++i)if(m->reticles[i].valid&&now-m->reticles[i].updatedUs>60000)m->disarm(i,"aim_stale");
    if(m->prepared&&m->active)m->resolve(now);
}
std::string Runtime::stateJSON()const
{
    std::ostringstream o;o<<"{\"contract\":\"tholus.stk-spatial/1\",\"ok\":true,\"prepared\":"<<(prepared()?"true":"false")
      <<",\"participantBindingsCurrent\":"<<(m->participantBindingsValid?"true":"false")<<",\"active\":"<<(m->active?"true":"false")<<",\"reason\":"<<quote(m->lastReason)
      <<",\"inputSessionID\":"<<quote(m->session)<<",\"routingGeneration\":"<<quote(m->generation)
      <<",\"routeRevision\":"<<quote(m->routeRevision)<<",\"projectionContextId\":"<<quote(m->context)
      <<",\"inputEpoch\":"<<m->inputEpoch<<",\"historyReady\":"<<(m->historyReady?"true":"false")<<",\"crewCount\":"<<m->sources.size()<<",\"recentEvents\":"<<m->recentEvents.size()<<",\"pending\":"<<m->pending.size()<<",\"frames\":"<<m->frames.size()<<",\"events\":[";
    bool first=true;for(const auto& e:m->events){if(!first)o<<',';first=false;
      o<<"{\"eventId\":"<<quote(e.eventId)<<",\"sourceId\":"<<quote(e.sourceId)<<",\"participantId\":"<<quote(e.participantId)
       <<",\"control\":"<<quote(e.control)<<",\"result\":"<<quote(e.result)<<",\"targetKind\":"<<int(e.kind)
       <<",\"targetId\":"<<(missionKind(e.kind)?quote(std::to_string(e.targetId)):std::to_string(e.targetId))
       <<",\"targetInstance\":"<<(missionKind(e.kind)?quote(std::to_string(e.targetInstance)):std::to_string(e.targetInstance))
       <<",\"frameId\":"<<e.frameId;
      if(missionKind(e.kind))o<<",\"moduleId\":\""<<e.historicalMission.module<<"\",\"phaseRevision\":\""<<e.historicalMission.phaseRevision<<"\"";
      o<<'}';}
    o<<"],\"feedback\":[";first=true;
    for(const auto& reticle:m->reticles)
    {
        const Feedback feedback=feedbackForRendering(reticle,m->lastNow);
        if(!first)o<<',';first=false;
        o<<"{\"sourceId\":"<<quote(reticle.sourceId)<<",\"kind\":"<<static_cast<unsigned>(feedback.kind)
         <<",\"hasPoint\":"<<(feedback.hasPoint?"true":"false")<<",\"u\":"<<feedback.u<<",\"v\":"<<feedback.v
         <<",\"startedUs\":"<<feedback.startedUs<<",\"untilUs\":"<<feedback.untilUs
         <<",\"actionSequence\":"<<feedback.actionSequence<<",\"reason\":"<<quote(feedback.reason);
        const auto tool=toolFeedbackForRendering(reticle);
        o<<",\"tool\":{\"status\":"<<static_cast<unsigned>(tool.status)<<",\"reason\":"<<quote(toolFeedbackReason(tool.status))
         <<",\"instruction\":"<<quote(toolFeedbackInstruction(tool.status))<<",\"controlEpoch\":\""<<tool.controlEpoch
         <<"\",\"targetId\":"<<tool.target<<",\"ownerCrewIndex\":"<<tool.owner<<"}";
        const auto& d=reticle.defenseFeedback;
        o<<",\"defenseTool\":{\"status\":"<<int(d.status)<<",\"reason\":"<<quote(ScintillaDefense::Runtime::statusReason(d.status))<<",\"instruction\":"<<quote(d.status==ScintillaDefense::Status::Reinforcing?"Hold the highlighted sector until the threat ends.":d.status==ScintillaDefense::Status::None||d.status==ScintillaDefense::Status::BoostReady?"":"Release the tool button, then select the required action and press again.")<<",\"controlEpoch\":\""<<d.epoch<<"\",\"threatId\":\""<<d.threat<<"\",\"sector\":"<<int(d.sector)<<",\"ownerCrewIndex\":"<<d.owner<<"}";
        const auto mission=missionFeedbackForRendering(reticle);
        o<<",\"missionTool\":{\"status\":"<<int(mission.status)<<",\"reason\":"<<quote(ScintillaMission::Runtime::statusReason(mission.status))
         <<",\"outcome\":"<<quote(ScintillaMission::Runtime::statusReason(mission.outcome))
         <<",\"instruction\":"<<quote(mission.status==ScintillaMission::Status::ReleaseRequired?"Release the tool button, then press it again.":mission.status==ScintillaMission::Status::TargetBusy?"Another player is retrieving this module. Choose another target or wait.":"")
         <<",\"controlEpoch\":\""<<mission.controlEpoch<<"\",\"entityId\":\""<<mission.entity<<"\",\"ownerCrewIndex\":"<<mission.owner<<"}";
        if(m->crewEnabled){const auto party=partyFeedbackForRendering(reticle);
            o<<",\"crewPartyTool\":{\"reason\":"<<quote(ScintillaCrew::Runtime::partyReasonName(party.reason))
             <<",\"controlEpoch\":\""<<party.controlEpoch<<"\",\"progress\":"<<party.progress
             <<",\"consenting\":"<<(party.consenting?"true":"false")<<'}';
        }
        o<<'}';
    }
    o<<"]}";return o.str();
}
Reply Runtime::route(const std::string& path,const std::string& body,int64_t now)
{
    try
    {
        if(now<0||now>SAFE)throw Invalid("invalid_clock");
        if(m->lastNow&&now<m->lastNow){m->invalidate("clock_reversed");throw Invalid("clock_reversed");}
        m->lastNow=now;
        if(path=="/spatial/state")return Reply(200,stateJSON());
        // Reject BEFORE batch recursion, renew, prepare or invalidation paths.
        // Ordinary transport can neither revive nor release this transaction.
        if(m->inputMaintenance)return error(409,"input_maintenance_suspended");
        const J j=Parser(body).parse();if(text(j,"contract")!="tholus.stk-spatial/1")throw Invalid("invalid_contract");
        if(path=="/spatial/update")
        {
            closed(j,{"contract","inputs"},{"presentation","renew","historyReady"});const auto& inputs=arr(at(j,"inputs"),0,32);
            // One command-queue turn for the entire frame. A batch never waits on
            // network calls between sources and preserves reliable edge order.
            bool hostHistoryReady=true;
            if(has(j,"historyReady"))
            {
                const J& hint=at(j,"historyReady");if(hint.kind!=J::Bool)throw Invalid("invalid_history_readiness");
                hostHistoryReady=hint.boolean;
                if(!hostHistoryReady){m->historyReady=false;m->stop("history_unavailable");}
            }
            bool presentationAccepted=true;std::string presentationReason;
            for(const char* operation:{"renew","presentation"})if(has(j,operation))
            {
                Reply reply=route(std::string("/spatial/")+operation,serialize(at(j,operation)),now);
                if(reply.status!=200)
                {
                    const J response=Parser(reply.json).parse();
                    if(std::string(operation)=="presentation"&&reply.status==409&&(text(response,"reason")=="frame_history_unavailable"||text(response,"reason")=="depth_history_unavailable"))
                    {presentationAccepted=false;presentationReason=text(response,"reason");m->historyReady=false;m->stop(presentationReason);continue;}
                    m->invalidate("invalid_update_context");return reply;
                }
            }
            if(!hostHistoryReady){m->historyReady=false;m->stop("history_unavailable");}
            std::ostringstream result;result<<"{\"contract\":\"tholus.stk-spatial/1\",\"ok\":true,\"accepted\":true,\"results\":[";
            bool first=true;
            for(const auto& input:inputs)
            {
                Reply reply=route("/spatial/input",serialize(input),now);
                const J response=Parser(reply.json).parse();std::string reason=text(response,"reason");
                if(malformedReason(reason))
                {m->invalidate("malformed_update_input");return error(400,"malformed_update_input");}
                if(!first)result<<',';first=false;result<<reply.json;
            }
            result<<"]";if(!presentationAccepted)result<<",\"presentationAccepted\":false,\"presentationReason\":\""<<presentationReason<<"\"";
            result<<"}";return Reply(200,result.str());
        }
        if(path=="/spatial/prepare")
        {
            closed(j,{"contract","roster","issuedAtHostUs","bindings","clockSamples","routeRevision","projection","inputUncertaintyUs"});
            if(m->prepared)return error(409,"session_already_prepared");
            const J& r=at(j,"roster");closed(r,{"contract","type","inputSessionID","routingGeneration","installationID","consumerID","capabilities","sources","projectionContextId","expiresAtHostUs"});
            if(text(r,"contract")!="tholus.spatial-input/1"||text(r,"type")!="roster")throw Invalid("invalid_roster");
            std::string session=tok(r,"inputSessionID"),generation=tok(r,"routingGeneration"),key=session+"/"+generation;
            if(m->usedSessions.count(key)||m->usedSessions.size()>=4096)return error(409,"reused_or_exhausted_session");
            std::set<std::string> capabilities;
            const std::set<std::string> allowed={"roomRay6dof","primaryAction","eventTimePose","secondaryAction","gripAction","haptics"};
            for(const auto& c:arr(at(r,"capabilities"),0,6)) {std::string s=str(c);if(!allowed.count(s)||!capabilities.insert(s).second)throw Invalid("invalid_capabilities");}
            for(const char* required:{"roomRay6dof","primaryAction","eventTimePose","secondaryAction"})if(!capabilities.count(required))return error(409,"scintilla_capability_missing");
            std::vector<Impl::Source> sources;std::set<std::string> sourceIDs,participantIDs;
            for(const auto& s:arr(at(r,"sources"),1,5))
            {
                closed(s,{"sourceId","deviceBindingRevision","trackingSpaceEpoch","poseContinuityId","roomCalibrationRevision","gripCalibrationRevision"});
                Impl::Source source;source.identity=identity(s);source.continuities.insert(source.identity.continuity);
                if(!sourceIDs.insert(source.identity.source).second)throw Invalid("duplicate_source");sources.push_back(source);
            }
            std::set<std::string> bound;
            for(const auto& b:arr(at(j,"bindings"),sources.size(),sources.size()))
            {
                closed(b,{"sourceId","participantId"});std::string id=tok(b,"sourceId"),participant=tok(b,"participantId");
                if(!sourceIDs.count(id)||!bound.insert(id).second||!participantIDs.insert(participant).second)throw Invalid("invalid_participant_binding");
                for(auto& s:sources)if(s.identity.source==id)s.participant=participant;
            }
            if(m->participantsKnown&&participantIDs!=m->expectedParticipants)return error(409,"participant_roster_mismatch");
            int64_t low=0,high=0;m->clocks(at(j,"clockSamples"),now,low,high);
            int64_t issued=num(j,"issuedAtHostUs"),expires=num(r,"expiresAtHostUs");
            if(expires<=issued||issued+high>now||now-(issued+low)>60000||expires+low<=now||expires-issued>10000000)throw Invalid("invalid_lease");
            int64_t inputUncertainty=num(j,"inputUncertaintyUs");if(inputUncertainty>5000)throw Invalid("invalid_input_uncertainty");
            Projection p=projection(at(j,"projection"));
            if(m->producerProjectionKnown&&!sameProjection(p,m->producerProjection))return error(409,"producer_projection_mismatch");
            std::string route=tok(j,"routeRevision"),context=tok(r,"projectionContextId"),installation=tok(r,"installationID"),consumer=tok(r,"consumerID");
            if(consumer!="external.immersive_supertuxkart")return error(409,"consumer_not_approved");
            m->stop("prepared");m->frames.clear();m->presented.clear();m->events.clear();m->seenEvents.clear();m->recentEvents.clear();m->reticles.clear();m->stream.clear();m->timingRevision.clear();
            m->retiredFrames.clear();m->retiredFrameIds.clear();m->retiredThrough=0;m->proofHostFrom=m->proofHostUntil=0;m->session=session;m->generation=generation;m->installation=installation;m->consumer=consumer;m->context=context;
            m->routeRevision=route;m->projection=p;m->sources=std::move(sources);m->capabilities=std::move(capabilities);m->usedSessions.insert(key);
            m->clockLow=low;m->clockHigh=high;m->clockChecked=now;m->leaseUntil=expires+low;m->issuedAt=issued+high;m->inputUncertainty=inputUncertainty;m->historyReady=true;m->prepared=true;m->participantBindingsValid=true;m->hasHeartbeat=false;
            for(size_t i=0;i<m->sources.size();++i){Reticle r;r.sourceId=m->sources[i].identity.source;r.participantId=m->sources[i].participant;r.slot=unsigned(i);r.result="release_required";m->reticles.push_back(r);}
            ++m->inputEpoch;m->captureBindingContinuity();return ok("prepared");
        }
        if(!m->prepared)return error(409,"session_unprepared");
        if(now>=m->leaseUntil){m->invalidate("lease_expired");return error(409,"lease_expired");}
        if(path=="/spatial/release")
        {closed(j,{"contract","inputSessionID","routingGeneration"});m->contextCheck(j);m->invalidate("released");return ok("released");}
        if(path=="/spatial/renew")
        {
            closed(j,{"contract","inputSessionID","routingGeneration","expiresAtHostUs","clockSamples"});m->contextCheck(j);
            int64_t low=0,high=0;m->clocks(at(j,"clockSamples"),now,low,high);
            int64_t drift=(now-m->clockChecked+9999)/10000+1;
            low=std::max(low,m->clockLow-drift);high=std::min(high,m->clockHigh+drift);
            if(low>high)throw Invalid("clock_offset_jump");
            int64_t until=num(j,"expiresAtHostUs")+low;
            if(until<=m->leaseUntil||until<=now||until-now>10000000)throw Invalid("invalid_renewal");
            // Deliberately preserve sequence, replay, continuity and rearm guards.
            m->clockLow=low;m->clockHigh=high;m->clockChecked=now;m->leaseUntil=until;return ok("renewed");
        }
        if(now-m->clockChecked>1000000){m->invalidate("clock_expired");return error(409,"clock_expired");}
        if(path=="/spatial/presentation")
        {
            closed(j,{"contract","inputSessionID","routingGeneration","frameStream","timingRevision","proofFromUs","proofUntilUs","runs"});m->contextCheck(j);
            std::string stream=tok(j,"frameStream"),revision=tok(j,"timingRevision");
            if(stream!=m->stream||stream.empty())throw Invalid("unknown_frame_stream");
            if(!m->timingRevision.empty()&&revision!=m->timingRevision){m->invalidate("presentation_timing_changed");return error(409,"presentation_timing_changed");}
            int64_t hostFrom=num(j,"proofFromUs"),hostUntil=num(j,"proofUntilUs");
            int64_t from=hostFrom+m->clockHigh,until=hostUntil+m->clockLow;
            // Both host frontiers are monotone: a discarded sealed prefix must
            // never be reintroduced after its candidate evidence was forgotten.
            if(from>until||until>now||now-until>1000000||until-from>2000000||hostFrom<m->proofHostFrom||hostUntil<m->proofHostUntil)throw Invalid("invalid_presentation_proof");
            std::vector<Impl::Presented> runs;bool retiredPrefix=false;
            for(const auto& run:arr(at(j,"runs"),1,256))
            {
                closed(run,{"frameId","presentedFromUs","presentedUntilUs"});
                Impl::Presented r;r.id=static_cast<uint64_t>(num(run,"frameId"));r.hostLo=num(run,"presentedFromUs");r.hostHi=num(run,"presentedUntilUs");r.lo=r.hostLo+m->clockLow;r.hi=r.hostHi+m->clockHigh;
                if(r.hostLo>r.hostHi||r.lo>r.hi||r.lo>until||r.hi<from||r.hi>now+(m->clockHigh-m->clockLow))throw Invalid("invalid_presentation_run");
                const Frame* frame=nullptr;for(const auto& f:m->frames)if(f.id==r.id)frame=&f;
                if(!frame)
                {
                    if(r.id<=m->retiredThrough||m->retiredFrameIds.count(r.id)||std::any_of(m->presented.begin(),m->presented.end(),[&](const Impl::Presented& old){return old.id==r.id;}))retiredPrefix=true;
                    else throw Invalid("unknown_presentation_frame");
                }
                else if(frame->captureUs>r.lo)throw Invalid("presentation_precedes_capture");
                if(!runs.empty()&&(r.lo<runs.back().lo||r.id<runs.back().id))throw Invalid("presentation_order");runs.push_back(r);
            }
            // Every overlapping sealed interval must retain the same complete
            // set of plausible frames, including ambiguity at exact boundaries.
            int64_t overlapLo=std::max(hostFrom,m->proofHostFrom),overlapHi=std::min(hostUntil,m->proofHostUntil);
            if(!m->presented.empty()&&overlapLo<=overlapHi)
            {
                std::set<int64_t> probes={overlapLo,overlapHi};
                for(const auto& r:runs){if(r.hostLo>=overlapLo&&r.hostLo<=overlapHi)probes.insert(r.hostLo);if(r.hostHi>=overlapLo&&r.hostHi<=overlapHi)probes.insert(r.hostHi);}
                for(const auto& r:m->presented){if(r.hostLo>=overlapLo&&r.hostLo<=overlapHi)probes.insert(r.hostLo);if(r.hostHi>=overlapLo&&r.hostHi<=overlapHi)probes.insert(r.hostHi);}
                std::vector<int64_t> points(probes.begin(),probes.end());
                for(size_t k=1;k<points.size();++k)if(points[k]-points[k-1]>1)probes.insert(points[k-1]+1);
                for(int64_t t:probes)
                {
                    std::set<uint64_t> before,after;
                    for(const auto& r:m->presented)if(r.hostLo<=t&&r.hostHi>=t)before.insert(r.id);
                    for(const auto& r:runs)if(r.hostLo<=t&&r.hostHi>=t)after.insert(r.id);
                    if(before!=after)throw Invalid("presentation_rewrite");
                }
            }
            if(retiredPrefix)return error(409,"frame_history_unavailable");
            // Shape, time and immutable overlap checks above are authoritative.
            // Only then classify unavailable actual producer depth as operational.
            if(m->pin)for(const auto& run:runs)
            {
                auto frame=std::find_if(m->frames.begin(),m->frames.end(),[&](const Frame& f){return f.id==run.id;});
                if(!m->pin(*frame))return error(409,"depth_history_unavailable");
            }
            if(!m->historyReady){m->activationNotBefore=std::max(m->activationNotBefore,now);m->historyReady=true;}
            m->presented=std::move(runs);m->proofHostFrom=hostFrom;m->proofHostUntil=hostUntil;m->timingRevision=revision;m->resolve(now);return ok("presentation_recorded");
        }
        if(path!="/spatial/input")return error(404,"unknown_spatial_route");
        closed(j,{"contract","message"},{"point"});const J& message=at(j,"message");
        if(text(message,"contract")!="tholus.spatial-input/1")throw Invalid("invalid_input_contract");m->contextCheck(message);
        std::string type=text(message,"type");
        if(type=="sessionRevoked")
        {closed(message,{"contract","type","inputSessionID","routingGeneration","reason"});tok(message,"reason");m->invalidate("session_revoked");return ok("session_revoked");}
        if(type=="heartbeat")
        {
            closed(message,{"contract","type","inputSessionID","routingGeneration","sequence","time","expiresAtHostUs"});
            uint64_t seq=static_cast<uint64_t>(num(message,"sequence"));m->hostTime(at(message,"time"),now,false);
            if(m->hasHeartbeat&&seq<=m->heartbeatSeq)throw Invalid("heartbeat_out_of_order");
            // Heartbeat confirms liveness; only /renew may extend authorization.
            if(num(message,"expiresAtHostUs")+m->clockLow<m->leaseUntil)throw Invalid("heartbeat_lease_mismatch");
            m->hasHeartbeat=true;m->heartbeatSeq=seq;return ok("heartbeat");
        }
        if(type!="aim"&&type!="action"&&type!="sourceState"&&type!="cancel")throw Invalid("unsupported_input_type");
        Identity id=identity(message);size_t i=m->sourceIndex(id.source);Impl::Source& source=m->sources[i];Reticle& reticle=m->reticles[i];
        uint64_t sequence=static_cast<uint64_t>(num(message,"sequence"));
        if(type=="sourceState"||type=="cancel")
        {
            if(has(j,"point"))throw Invalid("unexpected_point");
            if(type=="cancel")closed(message,{"contract","type","inputSessionID","routingGeneration","sourceId","deviceBindingRevision","trackingSpaceEpoch","poseContinuityId","roomCalibrationRevision","gripCalibrationRevision","sequence","reason"});
            else closed(message,{"contract","type","inputSessionID","routingGeneration","sourceId","deviceBindingRevision","trackingSpaceEpoch","poseContinuityId","roomCalibrationRevision","gripCalibrationRevision","sequence","reason","state"});
            tok(message,"reason");
            if(type=="cancel") {if(source.hasCancel&&sequence<=source.cancelSeq)throw Invalid("cancel_out_of_order");}
            else if(source.hasState&&sequence<source.stateSeq)throw Invalid("state_out_of_order");
            bool sameFixed=id.source==source.identity.source&&id.binding==source.identity.binding&&id.epoch==source.identity.epoch&&id.room==source.identity.room&&id.grip==source.identity.grip;
            if(!sameFixed){m->forgetControlObservations(i);if(source.trackingLossRevision!=UINT64_MAX)++source.trackingLossRevision;m->clearFeedback(i);m->disarm(i,"source_context_changed");source.tracked=false;throw Invalid("source_context_changed");}
            std::string state=type=="cancel"?"temporarilyLost":text(message,"state");
            static const std::set<std::string> states={"unbound","calibrating","tracked","temporarilyLost","reacquiring","disconnected","calibrationInvalid"};
            if(!states.count(state))throw Invalid("invalid_source_state");
            bool ready=state=="tracked",changed=id.continuity!=source.identity.continuity;
            if(type=="sourceState"&&source.hasState&&sequence==source.stateSeq&&ready)
            {
                if(source.tracked&&id==source.identity)return ok("source_tracked");
                throw Invalid("state_out_of_order");
            }
            if(ready&&source.suspendedContinuity==id.continuity)throw Invalid("fresh_continuity_required");
            if(changed)
            {
                m->clearFeedback(i);
                if(source.continuities.count(id.continuity)||source.continuities.size()>=4096)throw Invalid("continuity_reused_or_exhausted");
                source.continuities.insert(id.continuity);m->disarm(i,"continuity_changed");
            }
            if(type=="cancel"){source.hasCancel=true;source.cancelSeq=sequence;}else{source.hasState=true;source.stateSeq=sequence;}
            if(!ready||changed){m->forgetControlObservations(i);if(source.trackingLossRevision!=UINT64_MAX)++source.trackingLossRevision;}
            if(!ready)
            {
                m->clearFeedback(i);
                if(source.suspendedContinuity.empty())source.suspendedContinuity=source.identity.continuity;
                m->disarm(i,type=="cancel"?"cancelled":state);
            }
            else source.suspendedContinuity.clear();
            source.identity=id;source.tracked=ready;return ok(ready?"source_tracked":"source_cancelled");
        }
        if(!m->participantBindingsValid)return error(409,"participant_rebind_required");
        if(!(id==source.identity)){m->forgetControlObservations(i);m->disarm(i,"source_mismatch");throw Invalid("source_mismatch");}
        if(tok(message,"projectionContextId")!=m->context){m->disarm(i,"projection_mismatch");throw Invalid("projection_mismatch");}
        if(type=="aim")closed(message,{"contract","type","inputSessionID","routingGeneration","sourceId","deviceBindingRevision","trackingSpaceEpoch","poseContinuityId","roomCalibrationRevision","gripCalibrationRevision","sequence","time","projectionContextId","ray"});
        else closed(message,{"contract","type","inputSessionID","routingGeneration","sourceId","deviceBindingRevision","trackingSpaceEpoch","poseContinuityId","roomCalibrationRevision","gripCalibrationRevision","sequence","time","projectionContextId","eventId","control","edge"},{"rayAtEvent"});
        Interval time,originalHostTime;std::string timeFailure;
        try
        {
            time=m->hostTime(at(message,"time"),now,true,&originalHostTime);
            if(time.lo<m->issuedAt)timeFailure="event_precedes_grant";
            else if(time.lo<m->activationNotBefore)timeFailure="event_precedes_gameplay_activation";
        }
        catch(const Invalid& e)
        {
            // Malformed time objects are protocol errors. Operational timing
            // rejection still consumes the valid edge below and disarms it.
            if(malformedReason(e.what()))throw;
            timeFailure=e.what();
        }
        double u=0,v=0;bool point=false;
        if(has(j,"point"))
        {
            const J& p=at(j,"point");closed(p,{"u","v","viewId","sourceId","sequence","projectionContextId"});
            if(text(p,"viewId")!="main"||tok(p,"sourceId")!=id.source||num(p,"sequence")!=static_cast<int64_t>(sequence)||tok(p,"projectionContextId")!=m->context)throw Invalid("point_message_mismatch");
            u=real(at(p,"u"),0,1);v=real(at(p,"v"),0,1);Vec3 direction;point=projectionDirection(m->projection,u,v,&direction);
        }
        if(type=="aim")
        {
            ray(at(message,"ray"));if(source.hasAim&&sequence<=source.aimSeq)throw Invalid("aim_out_of_order");source.hasAim=true;source.aimSeq=sequence;
            const int64_t acquired=num(at(message,"time"),"evaluatedAtHostUs");
            // A pending SI5 aim may follow a newer press, even in another batch.
            // Consume its aim sequence, but neither move nor disarm that cursor.
            if(acquired<=source.cursorHostUs)return ok("aim_superseded");
            if(!timeFailure.empty()){m->disarm(i,timeFailure);return error(409,timeFailure);}
            if(!source.tracked||!m->active||!m->historyReady||!point){m->disarm(i,!m->historyReady?"history_unavailable":!point?"point_unavailable":"source_or_gameplay_unavailable");return error(409,reticle.result);}
            source.cursorHostUs=acquired;
            source.hasAnchor=true;source.anchorU=u;source.anchorV=v;
            reticle.u=u;reticle.v=v;reticle.valid=true;reticle.updatedUs=time.lo;reticle.result="aim";if(m->combatEnabled||m->crewEnabled)m->queueAim(i,originalHostTime,time.lo,u,v,now);return ok("aim");
        }
        const std::string event=tok(message,"eventId"),control=text(message,"control"),edge=text(message,"edge");
        unsigned controlIndex=control=="primary"?0:control=="secondary"?1:control=="grip"?2:3;
        if(controlIndex==3||(edge!="pressed"&&edge!="released"))throw Invalid("invalid_control_edge");
        if(controlIndex==2&&!m->capabilities.count("gripAction"))throw Invalid("control_not_granted");
        if(edge=="pressed")ray(at(message,"rayAtEvent"));else if(has(message,"rayAtEvent"))ray(at(message,"rayAtEvent"));
        bool feedbackAuthoritative=false;
        const bool feedbackHasPoint=point||source.hasAnchor;
        const double feedbackU=point?u:source.anchorU,feedbackV=point?v:source.anchorV;
        const auto reject=[&](int status,const std::string& reason)
        {
            if(feedbackAuthoritative)m->setFeedback(i,FeedbackKind::Rejected,sequence,now,now+500000,
                                                    feedbackHasPoint,feedbackU,feedbackV,reason);
            return error(status,reason);
        };
        m->pruneEvents(now);
        if(m->seenEvents.count(event))return error(409,"duplicate_event");
        if(source.hasAction&&sequence<=source.actionSeq)throw Invalid("action_out_of_order");
        feedbackAuthoritative=edge=="pressed"&&m->beginPressFeedback(i,sequence);
        source.hasAction=true;source.actionSeq=sequence;
        bool wasRearmed=source.rearmed[controlIndex];
        if(edge=="pressed"){source.rearmed[controlIndex]=false;source.held[controlIndex]=false;}
        else {source.held[controlIndex]=false;source.rearmed[controlIndex]=timeFailure.empty()&&m->active&&m->historyReady&&source.tracked;}
        reticle.primaryHeld=source.held[0];reticle.toolHeld=source.held[1];
        if(source.recentEventCount>=RECENT_EVENTS_PER_SOURCE)
        {
            // The sequence was consumed above. One source cannot revoke the room
            // grant or spend another crew member's receipt quota.
            m->disarm(i,"source_replay_capacity_exceeded");
            return reject(429,"source_replay_capacity_exceeded");
        }
        m->seenEvents.insert(event);
        Impl::EventReceipt receipt;receipt.id=event;receipt.sourceIndex=i;receipt.receivedAt=now;
        m->recentEvents.push_back(receipt);++source.recentEventCount;
        if(!timeFailure.empty()){m->forgetControlObservations(i);m->disarm(i,timeFailure);return reject(409,timeFailure);}
        source.observedKnown[controlIndex]=source.tracked;source.observedHeld[controlIndex]=edge=="pressed";
        if(edge=="released"){
            if(controlIndex==1){
                source.toolPressSerial=source.toolReleaseSerial=source.releaseInputEpoch=0;
                if(source.rearmed[1]){
                    if(m->nextSerial==std::numeric_limits<uint64_t>::max()){
                        m->disarm(i,"spatial_serial_exhausted");return reject(409,"spatial_serial_exhausted");
                    }
                    source.toolReleaseSerial=++m->nextSerial;source.releaseInputEpoch=m->inputEpoch;
                }
                ++source.toolEpoch;source.proofVisible=false;source.queuedAim.reset();source.pressLocked=false;
                source.pressMissionEligible=false;source.pressMission=ScintillaMission::Snapshot();
                reticle.defenseFeedback=ScintillaDefense::ToolFeedback();reticle.toolFeedback=ScintillaCombat::ToolSnapshot();
                reticle.missionFeedback=ScintillaMission::Feedback();reticle.toolEpoch=source.toolEpoch;
                reticle.partyFeedback=ScintillaCrew::PartyFeedback();source.partyPressEligible=false;
                source.partyPressAuthority=ScintillaCrew::PartyAuthority();source.partyCapturedProposal=0;source.partyPressTick=0;
            }
            return ok("released");
        }
        if(!m->historyReady)return reject(409,"history_unavailable");
        if(!wasRearmed||!source.tracked||!m->active||!point)return reject(409,!wasRearmed?"release_required":!point?"point_unavailable":"source_or_gameplay_unavailable");
        const bool reservedParty=m->crewEnabled&&ScintillaDefense::Layout::create(m->projection.width,m->projection.height,m->missionLayoutRequired,false,true).classify(u,v).context==ScintillaDefense::Context::PartyBoost;
        if(controlIndex==0&&!reservedParty&&time.lo-source.lastPrimary<250000)return reject(409,"fire_rate_limited");
        if(m->pending.size()>=64||((m->combatEnabled||m->missionEnabled)&&controlIndex==0&&!reservedParty&&m->combatShots.size()>=64)){m->disarm(i,"input_backpressure");return reject(429,"input_backpressure");}
        if(controlIndex==1&&m->nextProof==std::numeric_limits<uint64_t>::max()){
            m->disarm(i,"proof_serial_exhausted");return reject(409,"proof_serial_exhausted");
        }
        source.held[controlIndex]=true;if(controlIndex==1){
            ++source.toolEpoch;source.proofVisible=false;source.proofInterrupted=false;source.pressLocked=false;source.proofSerial=0;
            source.pressMissionEligible=false;source.pressMission=ScintillaMission::Snapshot();
            source.partyPressAuthority=m->crewAuthority();source.partyCapturedProposal=0;
            source.partyPressTick=m->simulationTick;source.partyPressEligible=false;
            // Remember the immutable spatial context immediately, including
            // while this reliable press is still waiting for its GPU proof.
            if(m->defenseEnabled||m->missionLayoutRequired||m->crewEnabled)source.pressSurface=ScintillaDefense::Layout::create(m->projection.width,m->projection.height,m->missionLayoutRequired,false,m->crewEnabled).classify(u,v);
            reticle.toolFeedback=ScintillaCombat::ToolSnapshot();
            reticle.missionFeedback=ScintillaMission::Feedback();
            reticle.partyFeedback=ScintillaCrew::PartyFeedback();
            reticle.toolEpoch=source.toolEpoch;
        }if(controlIndex==0&&!reservedParty)source.lastPrimary=time.lo;
        if(feedbackAuthoritative)m->setFeedback(i,FeedbackKind::Pending,sequence,now,now+PendingTimeoutUs,true,u,v,"queued");
        reticle.primaryHeld=source.held[0];reticle.toolHeld=source.held[1];
        const int64_t acquired=num(at(message,"time"),"evaluatedAtHostUs");
        // An older valid action still resolves its own rayAtEvent, while a newer
        // aim remains the live cursor. Equal-time event poses win over aims.
        if(acquired>=source.cursorHostUs)
        {
            source.cursorHostUs=acquired;source.hasAnchor=true;source.anchorU=u;source.anchorV=v;
            reticle.u=u;reticle.v=v;reticle.valid=true;reticle.updatedUs=time.lo;
        }
        Impl::Pending pending;pending.event.sourceId=id.source;pending.event.participantId=source.participant;pending.event.eventId=event;pending.event.control=control;
        pending.toolEpoch=source.toolEpoch;pending.feedbackRevision=source.feedbackRevision;pending.actionSequence=sequence;
        pending.crewAuthority=m->crewAuthority();
        pending.event.evaluatedUs=time.lo;pending.hostTime=originalHostTime;pending.u=u;pending.v=v;pending.selectionDeadline=time.lo+60000;pending.deadline=now+PendingTimeoutUs;
        if((m->combatEnabled||m->missionEnabled||m->crewEnabled)&&controlIndex<2&&!(controlIndex==0&&reservedParty)){
            if(m->nextSerial==std::numeric_limits<uint64_t>::max()){m->disarm(i,"combat_serial_exhausted");return reject(409,"combat_serial_exhausted");}
            pending.event.serial=++m->nextSerial;pending.event.inputEpoch=m->inputEpoch;
            pending.event.raceEpoch=m->combatRaceEpoch;pending.event.actionSequence=sequence;pending.event.admittedTick=m->simulationTick;
            pending.event.admittedTargets=m->combatTargets;
            pending.event.missionRevision=m->missionRevision;pending.event.admittedMissionTargets=m->missionTargets;
            if(controlIndex==0){HitEvent slot=pending.event;slot.result="pending";m->combatShots.push_back(slot);}
            else source.toolPressSerial=pending.event.serial;
        }
        if(controlIndex==1)pending.proofSerial=++m->nextProof;
        if((m->combatEnabled||m->crewEnabled)&&acquired>=source.cursorHostUs&&(controlIndex==1||source.held[1]))m->queueAim(i,originalHostTime,time.lo,u,v,now);
        m->pending.push_back(pending);m->resolve(now);return ok("queued");
    }
    catch(const Invalid& e){if(path=="/spatial/update")m->invalidate("malformed_update");return error(400,e.what());}
    catch(const std::exception&){return error(400,"invalid_request");}
}
}
