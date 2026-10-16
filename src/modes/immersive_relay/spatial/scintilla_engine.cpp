// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/spatial/scintilla_engine.hpp"
#include "modes/immersive_relay/spatial/scintilla_target_lifetime.hpp"
#include "modes/immersive_relay/spatial/scintilla_readback_cache.hpp"
#include "modes/immersive_relay/spatial/scintilla_depth_visibility.hpp"
#include "modes/immersive_relay/scintilla_mission_anchors.hpp"
#include "modes/immersive_relay/scintilla_crew.hpp"
#include "modes/immersive_relay/scintilla_crew_zones.hpp"
#include "modes/immersive_relay/scintilla_crew_physics.hpp"
#include "modes/immersive_relay/scintilla_crew_protocol.hpp"
#include "modes/immersive_relay/scintilla_crew_result_json.hpp"
#include "modes/immersive_relay/scintilla_mission_measurement.hpp"
#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"
#include "utils/command_line.hpp"
#include "karts/max_speed.hpp"
#include "karts/kart_properties.hpp"
#include <sys/stat.h>
#include "modes/immersive_relay/scintilla_mission_protocol.hpp"
#include "modes/immersive_relay/scintilla_mission_result_json.hpp"
#include "modes/immersive_relay/scintilla_tuning.hpp"
#include "modes/immersive_relay/scintilla_telemetry.hpp"
#include <climits>
#ifndef SERVER_ONLY
// scintilla_crew_text_native.hpp draws through the complete driver type.
#include <IVideoDriver.h>
#endif
#include <cstdlib>
#include <ctime>
#include "io/file_manager.hpp"
#include "network/crypto.hpp"
#include "modes/linear_world.hpp"
#include "karts/abstract_kart_animation.hpp"
#include "tracks/drive_graph.hpp"
#include "modes/immersive_relay/relay_session_controller.hpp"
#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"
#endif
#include "modes/immersive_relay/render/camera/immersive_camera_rig.hpp"
#include "modes/immersive_relay/render/full_sphere_capture.hpp"
#include "modes/immersive_relay/render/immersive_render_config.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"
#include "config/stk_config.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/explosion_animation.hpp"
#include "karts/rescue_animation.hpp"
#include "modes/immersive_relay/scintilla_defense_physics.hpp"
#include "physics/physics.hpp"
#include "physics/stk_dynamics_world.hpp"
#include "audio/sfx_manager.hpp"
#include "items/powerup.hpp"
#include "items/powerup_manager.hpp"
#include "items/attachment.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "network/network_config.hpp"
#include "network/rewind_manager.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/string_utils.hpp"
#include <SColor.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <random>
#include <sstream>
#include <utility>
#ifndef SERVER_ONLY
#include "graphics/gl_headers.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/stk_tex_manager.hpp"
#include "font/font_manager.hpp"
#include "font/regular_face.hpp"
#include "font/font_settings.hpp"
#include "utils/translation.hpp"
#include <ge_texture.hpp>
#include <IImage.h>
#include <ITexture.h>
#include "items/item_manager.hpp"
#include "items/item.hpp"
#include "karts/abstract_kart.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "tracks/track.hpp"
#include "tracks/drive_node.hpp"
#endif

namespace ImmersiveRelay { namespace ScintillaEngine {
using namespace ScintillaSpatial;
using SpatialVec3 = ScintillaSpatial::Vec3;
namespace {
std::string makeProducerUUID()
{
    std::random_device random;std::array<unsigned char,16> bytes;
    for(auto& b:bytes)b=static_cast<unsigned char>(random());
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(unsigned i=0;i<bytes.size();++i){if(i==4||i==6||i==8||i==10)out<<'-';out<<std::setw(2)<<unsigned(bytes[i]);}return out.str();
}
const std::string producerID=makeProducerUUID();
namespace MissionProtocol=ScintillaMissionProtocol;
MissionProtocol::Receipts missionRequests;
MissionProtocol::IntroReceipt missionIntroReceipt;
MissionProtocol::Identity missionPreparedIdentity;
std::string missionIntroHash,missionIntroError="mission_intro_not_loaded",missionIntroVariant,missionIntroProfile;
bool missionIntroAvailable=false,missionAPIPrepared=false;
uint64_t missionAPIRevision=0;
bool active=false;
uint64_t suppressedStaggerMotion=0;
uint64_t generation=1, sequence=0, nextInstance=0;
std::string stream;
Frame current;
std::deque<Frame> publishedFrames;
Runtime consumer;
ScintillaCombat::Runtime combat;
ScintillaMission::Runtime mission;
namespace Crew=ScintillaCrew;
Crew::Runtime crew;
Crew::Result crewTerminalRecord;
Crew::Observation crewTerminalObservation;
Crew::Result frozenCrewResult();
uint64_t crewCreditSerial=0,crewObservedAdministrative=0;
int64_t crewPausedObservedUs=0,crewPausedRemainder=0;
void recordCrewCredit(const std::string&,Crew::ContributionKind);
void updateCrewCoordination(bool admitted);
void synchronizeCrewTurn();
void clearCrewCoordinationEvidence();
bool crewPreparationBound();
void resetCrewAuthority();
void resetCrewPartyZones();
bool prepareCrewPartyZones(std::string* reason);
std::string crewPartyZoneManifestHash();
void commitOptionalService(bool admitted);
void writeCrewParty(std::ostream&);
std::vector<Crew::PartyBeam> currentCrewPartyBeams(int64_t now){return consumer.partyBeams(now);}
namespace Anchors=ScintillaMissionAnchors;
Anchors::Manifest missionAnchors;
Anchors::Traversal missionTraversal;
unsigned anchorCrew=0;
bool measurementLoading=false;
std::string measurementAssetsHash,measurementShaderHash,measurementExecutableHash;
ScintillaMissionMeasurement::Bounds qualifiedBounds;
bool refreshMissionQualification(std::string* reason);
bool measurementPlacementPermitted(unsigned group,unsigned capsule,unsigned lap,const Anchors::Capsule& shape);
void pollMissionMeasurement();
void prepareMissionMeasurementFrame(const ImmersiveRenderConfig& config);
void finishMissionMeasurementFrame(bool published);
void interruptMissionMeasurement(const std::string& reason);
bool missionTetherFullMask(uint64_t frame,int side);
void resetMissionMeasurementProofs();
void breakMissionMeasurementContinuity();
void invalidateMissionQualificationCache();
unsigned missionCrewCount();
std::string anchorHash,anchorStatus="not_loaded";
uint64_t anchorRace=0;
struct MissionPlacement {SpatialVec3 center;double radius=2;uint64_t instance=0;};
std::map<uint64_t,MissionPlacement> missionPlacements;
SpatialVec3 missionPreviousPosition;
bool missionPositionKnown=false,missionPreviousAnimated=false;
bool missionProgressKnown=false;
double missionPreviousProgress=0;
ScintillaMission::Result missionTerminalRecord;
bool missionStepOpen=false,missionClockBound=false;
int64_t missionClockBase=0;
std::string missionStepFailure;
uint64_t raceEpoch=1, combatNextInstance=0, rosterEpoch=1;
int64_t simulationTick=0,crewDefenseThreatTick=-1;
std::vector<std::string> lastRoster;
struct CombatOwner {const void* pointer=nullptr;uint64_t instance=0;bool effectRunning=false;const void* effectAnimation=nullptr;};
std::map<uint64_t,CombatOwner> combatOwners;
// Race tuning: reloaded with every race generation, fixed until that race ends.
ScintillaTuning::Tuning tuning;
std::string tuningSource="builtin",tuningHash,tuningError;
const char* const TuningEnvironment="STK_RELAY_SCINTILLA_TUNING";
bool relaxedHuman(){return tuning.defense.boostPolicy!=ScintillaTuning::BoostPolicy::Cancel;}
double combatMinAngularSine(){return std::sin(tuning.combat.minAngularRadiusDeg*3.14159265358979323846/180.0);}
bool localCombatSupported()
{return !NetworkConfig::get()->isNetworking()&&!RewindManager::isEnabled()&&!RaceManager::get()->isWatchingReplay()&&!RaceManager::get()->isRecordingRace();}
void refreshRoster()
{
    auto roster=RelaySessionController::get()->getSpatialCrewIDs();std::sort(roster.begin(),roster.end());
    if(roster!=lastRoster){lastRoster=roster;++rosterEpoch;}
}
using Registered=TargetLifetime::Registered;
std::map<uint64_t, Registered> registered;
std::array<uint32_t,8> stamp;
#ifndef SERVER_ONLY
video::ITexture* missionResultAtlas=nullptr; // Driver-owned, loaded while held.
bool missionAtlasBound=false;
bool missionAssetsInvalidated=false;
GLint missionSavedAtlasTexture=0,missionSavedActiveTexture=GL_TEXTURE0;
#endif

void changeStream()
{
    std::random_device random;
    std::ostringstream out;
    for (unsigned i=0;i<4;i++) {
        uint32_t value=random();
        for(unsigned j=0;j<4;j++) out<<std::hex<<std::setw(2)<<std::setfill('0')<<((value>>(j*8))&255);
        stamp[i+1]=value;
    }
    stream=out.str(); sequence=0;
}
void encodeStamp()
{
    stamp[0]=0x31424854; stamp[5]=(uint32_t)current.id; stamp[6]=(uint32_t)(current.id>>32);
    uint32_t crc=0xffffffff;
    for(unsigned i=0;i<28;i++) {
        crc^=(stamp[i/4]>>(8*(i%4)))&255;
        for(unsigned j=0;j<8;j++) crc=(crc&1)?(crc>>1)^0xedb88320:(crc>>1);
    }
    stamp[7]=~crc;
}
bool sameProjection(const Projection& a,const Projection& b)
{return a.kind==b.kind&&a.width==b.width&&a.height==b.height&&std::abs(a.domeTiltDeg-b.domeTiltDeg)<1e-6;}
bool currentPublished(const Frame& frame)
{return frame.stream==stream&&frame.generation==generation&&sameProjection(frame.projection,current.projection);}
double dot(const SpatialVec3& a,const SpatialVec3& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
// Why depth visibility was unavailable since the last telemetry sample: no
// frame lease, point outside the face, incomplete FBO, failed read or fence,
// failed poll; and GL errors left pending by other rendering code.
enum VisibilityLoss {LossLease,LossRange,LossFbo,LossRead,LossPoll,StaleGlError,VisibilityLossCount};
unsigned visibilityLoss[VisibilityLossCount]={};
#ifndef SERVER_ONLY
struct GLState {
    GLint readFbo,drawFbo,texture,activeUnit,pack;
    GLint packAlignment,packRowLength,packSkipRows,packSkipPixels,packSwapBytes;
    GLState() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeUnit); glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);
        glGetIntegerv(GL_PACK_ALIGNMENT,&packAlignment);glGetIntegerv(GL_PACK_ROW_LENGTH,&packRowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS,&packSkipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&packSkipPixels);
        glGetIntegerv(GL_PACK_SWAP_BYTES,&packSwapBytes);
    }
    ~GLState() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);
        glActiveTexture(activeUnit); glBindTexture(GL_TEXTURE_2D,texture); glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);
        glPixelStorei(GL_PACK_ALIGNMENT,packAlignment);glPixelStorei(GL_PACK_ROW_LENGTH,packRowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS,packSkipRows);glPixelStorei(GL_PACK_SKIP_PIXELS,packSkipPixels);
        glPixelStorei(GL_PACK_SWAP_BYTES,packSwapBytes);
    }
};
const size_t depthBudget=192u*1024u*1024u;
size_t allocatedDepthBytes=0;
GLuint depthFbo=0;
bool signaled(GLsync fence)
{
    if(!fence)return false;
    const GLenum result=glClientWaitSync(fence,0,0);
    return result==GL_ALREADY_SIGNALED || result==GL_CONDITION_SATISFIED;
}
struct DepthFrame {
    uint64_t id; std::string stream; unsigned size;
    size_t bytes;GLsync copyFence=NULL;
    GLuint textures[6];
    explicit DepthFrame(unsigned dimension):id(0),size(dimension),bytes(size_t(dimension)*dimension*6*4)
    { allocatedDepthBytes+=bytes;std::fill(textures,textures+6,0); }
    ~DepthFrame() {
        GLState saved;
        if(depthFbo) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER,depthFbo);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,0,0);
        }
        if(copyFence)glDeleteSync(copyFence);
        glDeleteTextures(6,textures);allocatedDepthBytes-=bytes;
    }
};
struct QueryKey {
    uint64_t frame; unsigned face,x,y;
    bool operator<(const QueryKey& b) const {
        if(frame!=b.frame)return frame<b.frame;
        if(face!=b.face)return face<b.face;
        if(x!=b.x)return x<b.x;
        return y<b.y;
    }
};
struct Query {
    GLuint buffer=0; GLsync fence=NULL; int64_t lastRequestedUs=0; float depth=0; bool complete=false;
    bool failed=false,gpuComplete=false;
    std::shared_ptr<DepthFrame> frame;
    std::set<uint64_t> owners;
};
std::deque<std::shared_ptr<DepthFrame> > depths;
std::map<QueryKey,Query> queries;
// The lease pool outlives consumer destruction; its deleter performs no GL work.
std::shared_ptr<std::set<uint64_t> > liveQueryOwners(new std::set<uint64_t>());
std::shared_ptr<void> queryLease(uint64_t id)
{
    auto pool=liveQueryOwners;pool->insert(id);
    return std::shared_ptr<void>(new uint64_t(id),[pool,id](void* value){pool->erase(id);delete static_cast<uint64_t*>(value);});
}
// Invalidating a session cannot turn in-flight GPU storage into an unbounded
// stream of deferred deletes. Retired resources retain the SAME global budgets.
std::vector<std::shared_ptr<DepthFrame> > retiredDepths;
std::vector<Query> retiredQueries;
void clearQuery(Query& q) {
    if(q.fence)glDeleteSync(q.fence);
    if(q.buffer)glDeleteBuffers(1,&q.buffer);
    q.fence=NULL;q.buffer=0;q.frame.reset();
}
void pollRetiredGraphics()
{
    GLState saved;
    if(depthFbo) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,depthFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,0,0);
    }
    for(auto it=retiredQueries.begin();it!=retiredQueries.end();) {
        if(it->gpuComplete || signaled(it->fence)) {clearQuery(*it);it=retiredQueries.erase(it);}
        else ++it; // A missing/failed fence remains reserved until process exit.
    }
    for(auto it=retiredDepths.begin();it!=retiredDepths.end();) {
        if(it->use_count()==1 && signaled((*it)->copyFence))it=retiredDepths.erase(it);
        else ++it;
    }
}
void clearGraphics()
{
    for(auto& item:queries)retiredQueries.push_back(std::move(item.second));
    queries.clear();
    for(auto& frame:depths)retiredDepths.push_back(std::move(frame));
    depths.clear();
    // Keep one FBO across generations; detaching does not wait on queued reads.
    if(depthFbo) {
        GLState saved;glBindFramebuffer(GL_READ_FRAMEBUFFER,depthFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,0,0);
    }
    pollRetiredGraphics();
}
bool poll(Query& q)
{
    if(q.complete)return true;
    if(q.failed)return false;
    if(!q.fence){q.failed=true;return false;}
    GLenum result=glClientWaitSync(q.fence,0,0); // Never wait for GPU work on the render thread.
    if(result==GL_WAIT_FAILED){q.failed=true;return false;}
    if(result!=GL_ALREADY_SIGNALED && result!=GL_CONDITION_SATISFIED)return false;
    q.gpuComplete=true;
    GLState saved;
    glBindBuffer(GL_PIXEL_PACK_BUFFER,q.buffer);
    const void* mapped=glMapBufferRange(GL_PIXEL_PACK_BUFFER,0,sizeof(float),GL_MAP_READ_BIT);
    if(!mapped){clearQuery(q);q.failed=true;return false;}
    std::memcpy(&q.depth,mapped,sizeof(float));
    const bool valid=glUnmapBuffer(GL_PIXEL_PACK_BUFFER)==GL_TRUE;
    clearQuery(q);q.complete=valid;q.failed=!valid;return valid;
}
std::shared_ptr<void> pinFrame(const Frame& frame)
{
    auto record=std::find_if(depths.begin(),depths.end(),[&](const std::shared_ptr<DepthFrame>& d){return d->id==frame.id&&d->stream==frame.stream;});
    if(record==depths.end()||!signaled((*record)->copyFence))return std::shared_ptr<void>();
    return *record;
}
Visibility visible(const Frame& frame,const Target&,const SpatialVec3& direction,double distance)
{
    std::shared_ptr<DepthFrame> record=std::static_pointer_cast<DepthFrame>(frame.visibilityLease);
    if(!record||record->id!=frame.id||record->stream!=frame.stream){++visibilityLoss[LossLease];return Visibility::Unavailable;}
    const SpatialVec3 d(dot(direction,frame.cameraRight),dot(direction,frame.cameraUp),dot(direction,frame.cameraForward));
    double ma,sc,tc; unsigned face;
    const double ax=std::abs(d.x),ay=std::abs(d.y),az=std::abs(d.z);
    if(ax>=ay && ax>=az) {face=d.x>=0?0:1;ma=ax;sc=d.x>=0?-d.z:d.z;tc=d.y;}
    else if(ay>=az) {face=d.y>=0?2:3;ma=ay;sc=d.x;tc=d.y>=0?-d.z:d.z;}
    else {face=d.z>=0?4:5;ma=az;sc=d.z>=0?d.x:-d.x;tc=d.y;}
    if(ma<=0){++visibilityLoss[LossRange];return Visibility::Unavailable;}
    const unsigned size=record->size;
    const double tangent=std::tan(frame.faceFovDeg*3.14159265358979323846/360);
    const double u=(sc/(ma*tangent)+1)*0.5,v=(tc/(ma*tangent)+1)*0.5;
    if(u<0 || u>=1 || v<0 || v>=1){++visibilityLoss[LossRange];return Visibility::Unavailable;}
    QueryKey key={frame.id,face,(unsigned)(u*size),(unsigned)(v*size)};
    auto found=queries.find(key);
    if(found==queries.end()) {
        if(queries.size()+retiredQueries.size()>=64){
            // Reclaim only a completed result with no live consumer. Poll first:
            // a signaled-but-unread fence must never be thrown away by age.
            const bool reclaimed=ReadbackCache::reclaimCompletedUnowned(queries,poll,
                [](const Query& q){for(uint64_t owner:q.owners)if(liveQueryOwners->count(owner))return true;return false;},
                [](Query& q){clearQuery(q);});
            if(!reclaimed)return Visibility::Pending;
        }
        GLState saved;
        if(!depthFbo) {
            glGenFramebuffers(1,&depthFbo);glBindFramebuffer(GL_FRAMEBUFFER,depthFbo);
            glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER,depthFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,record->textures[face],0);
        glReadBuffer(GL_NONE);
        if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){++visibilityLoss[LossFbo];return Visibility::Unavailable;}
        // Errors are sticky: one left by unrelated rendering would otherwise
        // be read below as this query's failure.
        while(glGetError()!=GL_NO_ERROR)++visibilityLoss[StaleGlError];
        Query q;q.lastRequestedUs=nowUs();q.frame=record;if(frame.queryOwner)q.owners.insert(frame.queryOwner);glGenBuffers(1,&q.buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER,q.buffer);glBufferData(GL_PIXEL_PACK_BUFFER,sizeof(float),NULL,GL_STREAM_READ);
        glPixelStorei(GL_PACK_ALIGNMENT,4);glPixelStorei(GL_PACK_ROW_LENGTH,0);
        glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_SWAP_BYTES,GL_FALSE);
        glReadPixels(key.x,key.y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,NULL);
        q.failed=glGetError()!=GL_NO_ERROR;
        q.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
        if(!q.fence)q.failed=true;
        if(q.failed){++visibilityLoss[LossRead];retiredQueries.push_back(std::move(q));return Visibility::Unavailable;}
        queries.insert(std::make_pair(key,q));return Visibility::Pending;
    }
    for(auto it=found->second.owners.begin();it!=found->second.owners.end();)if(!liveQueryOwners->count(*it))it=found->second.owners.erase(it);else ++it;
    if(frame.queryOwner)found->second.owners.insert(frame.queryOwner);
    found->second.lastRequestedUs=nowUs();
    if(!poll(found->second)){
        if(!found->second.failed)return Visibility::Pending;
        ++visibilityLoss[LossPoll];return Visibility::Unavailable;
    }
    const double z=distance*ma,n=frame.nearPlane,f=frame.farPlane;
    if(z<=n || z>=f)return Visibility::Occluded;
    return DepthVisibility::visible(found->second.depth,static_cast<float>(z),
        static_cast<float>(n),static_cast<float>(f))?Visibility::Visible:Visibility::Occluded;
}
TargetLifetime::Current currentTarget(uint64_t id)
{
    World* world=World::getWorld();
    if(!world||!id)return TargetLifetime::Current();
    if(id<100000)return TargetLifetime::currentKart(world,id-1);
    Track* track=Track::getCurrentTrack();
    ItemManager* items=track?track->getItemManager():NULL;
    return TargetLifetime::currentItem(items,id-100000,ItemState::ITEM_BONUS_BOX);
}
void target(uint64_t id,const void* pointer,const SpatialVec3& center,double radius)
{
    Registered& ref=registered[id];
    if(ref.pointer!=pointer || !ref.present) {ref.pointer=pointer;ref.instance=++nextInstance;}
    ref.present=true;
    Target value;value.id=id;value.instance=ref.instance;value.center=center;value.radius=radius;
    current.targets.push_back(value);
}
#endif
}
int64_t nowUs() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
#ifdef ENABLE_RELAY_LOCAL_API
// Socket-thread /spatial/clock: the same steady clock, registered only while
// Scintilla input is enabled and answered only while the runtime would be.
static bool socketClock(int64_t* now)
{
    if(!RelaySessionController::get()->isRuntimeEnabled())return false;
    *now=nowUs();return true;
}
#endif
Runtime& input(){return consumer;}
bool enabled(){return active;}
namespace {
namespace Defense=ScintillaDefense;
Defense::Runtime defense;
bool defensePrepared=false;
Projection defenseProjection;
std::string defenseRoute,defenseContext,defenseEvidence;
std::vector<double> defenseVisibleRegions;
uint64_t defenseRoster=0,humanInstance=0,nextHumanInstance=0;
unsigned defenseLayoutRevision=Defense::Layout::Revision;
const void* humanPointer=nullptr;
int64_t soloIdleSince=0;
uint64_t tetherPlacementInstance=0;
int tetherSide=-1;
int64_t tetherCreatedUs=0;
bool tetherPresentationHold=false;
uint64_t missionCueRace=0,missionCueFinish=0;
std::array<ScintillaMission::Slot,3> missionCueSlots;
#ifndef SERVER_ONLY
struct MissionPresentationQuery {std::shared_ptr<Frame> frame;std::shared_ptr<void> lease;};
std::array<MissionPresentationQuery,3> missionPresentationQueries;
std::shared_ptr<Frame> defenseEvidenceFrame;
std::shared_ptr<void> defenseQueryLease;
std::shared_ptr<Frame> candidateEvidenceFrame;std::shared_ptr<void> candidateQueryLease;
uint64_t candidateEvidenceID=0,candidateEvidenceInstance=0;int64_t candidateEvidenceStarted=0;
struct SourceProof {uint64_t instance=0;int64_t low=0;bool visible=false;};std::map<uint64_t,SourceProof> sourceProofs;
int64_t defenseEvidenceStarted=0,lastDefenseVisibleLow=0;uint64_t lastDefenseVisibleThreat=0;
#endif
void clearMissionPresentationQueries()
{
#ifndef SERVER_ONLY
    for(auto& q:missionPresentationQueries){q.frame.reset();q.lease.reset();}
#endif
}
void observeNewTether(int64_t now)
{
    const auto snapshot=mission.snapshot(ScintillaMission::Runtime::TetherEntityId);
    if(!snapshot.id||snapshot.instance==tetherPlacementInstance)return;
    tetherPlacementInstance=snapshot.instance;tetherSide=-1;tetherCreatedUs=now;
    clearMissionPresentationQueries();
}
void clearDefenseEvidence()
{
#ifndef SERVER_ONLY
    defenseEvidenceFrame.reset();defenseQueryLease.reset();
#endif
}
bool defenseBound()
{
    return World::getWorld()&&World::getWorld()->getIdent()=="IMMERSIVE_RELAY"&&RelaySessionController::get()->isRuntimeEnabled()&&defensePrepared&&active&&defense.configured()&&defense.race()==raceEpoch&&defense.inputEpoch()==consumer.inputEpoch()&&consumer.prepared()&&defense.crew()==lastRoster&&defenseRoster==rosterEpoch&&defenseRoute==consumer.activeRouteRevision()&&defenseContext==consumer.activeProjectionContextId()&&sameProjection(defenseProjection,current.projection)&&defenseLayoutRevision==(mission.required()?Defense::Layout::MissionRevision:Defense::Layout::Revision);
}
AbstractKart* defenseHuman()
{
    auto*w=World::getWorld();auto*k=w&&w->getNumKarts()?w->getPlayerKart(0):nullptr;
    if(k!=humanPointer){humanPointer=k;humanInstance=k?++nextHumanInstance:0;}
    return k;
}
bool liveMissionTarget(uint64_t id,uint64_t instance)
{
    if(!mission.configured() || mission.race()!=raceEpoch || missionTerminal())return false;
    const auto* human=defenseHuman();
    if(!human || human->isEliminated() || human->hasFinishedRace() ||
       mission.humanInstance()!=humanInstance)return false;
    const auto snapshot=mission.snapshot(id);
    return snapshot.id==id && snapshot.instance==instance &&
        (snapshot.phase==ScintillaMission::Phase::Shell ||
         snapshot.phase==ScintillaMission::Phase::Open ||
         snapshot.phase==ScintillaMission::Phase::TetherAwaitingPresentation ||
         snapshot.phase==ScintillaMission::Phase::TetherRecoverable);
}
void updateMissionCrossings()
{
    if(!mission.active()||anchorRace!=raceEpoch||!missionTraversal.prepared())return;
    auto* world=dynamic_cast<LinearWorld*>(World::getWorld());
    auto* human=defenseHuman();auto* graph=DriveGraph::get();
    if(!world||!human||!graph||graph->getLapLength()<=0)return;
    const auto& p=human->getXYZ();const SpatialVec3 position(p.x(),p.y(),p.z());
    const bool animated=human->getKartAnimation()!=nullptr;
    const SpatialVec3 delta(position.x-missionPreviousPosition.x,
        position.y-missionPreviousPosition.y,position.z-missionPreviousPosition.z);
    const double stepBound=2.0+std::max(std::abs(double(human->getSpeed())),
        std::abs(double(human->getCurrentMaxSpeed())))/mission.profile().fps;
    // Re-entry after a rescue must also break continuity: track coordinates
    // stay cached while native animations own the kart.
    bool continuous=missionPositionKnown&&!animated&&!missionPreviousAnimated&&
        dot(delta,delta)<=stepBound*stepBound;
    missionPreviousPosition=position;missionPositionKnown=true;missionPreviousAnimated=animated;
    const unsigned kart=human->getWorldKartId();
    const int lap=world->getLapForKart(kart);
    const double fraction=world->getDistanceDownTrackForKart(kart,true)/graph->getLapLength();
    const double progress=lap+fraction;
    // A checkline/projection jump is not road travelled even when XYZ moved
    // little. Official lap is retained; only continuity is rejected.
    if(lap>=0){
        continuous=continuous&&missionProgressKnown&&
            std::abs(progress-missionPreviousProgress)*graph->getLapLength()<=stepBound*4;
        missionPreviousProgress=progress;missionProgressKnown=true;
    }else missionProgressKnown=false;
    const auto crossings=missionTraversal.update(lap,fraction,continuous);
    if(!missionTraversal.lastUpdateValid()){
        missionStepFailure="mission_track_progress_invalid";setMissionPresentationHeld(true);return;
    }
    for(const auto& crossing:crossings){
        if(crossing.kind==Anchors::Kind::Confirmation){
            if(crossing.skipped)mission.skipConfirmation(crossing.ordinal);
            else mission.crossConfirmation(crossing.ordinal,crossing.slot);
            continue;
        }
        const uint64_t id=ScintillaMission::Runtime::CapsuleEntityBase+crossing.ordinal;
        if(crossing.kind==Anchors::Kind::Expiry){
            const auto snapshot=mission.snapshot(id);
            mission.expireCapsule(id,snapshot.instance);missionPlacements.erase(id);continue;
        }
        const auto& disabled=tuning.mission.disabledGroups;
        if(crossing.skipped||animated||!world->isOnRoad(kart)||
           std::find(disabled.begin(),disabled.end(),missionAnchors.groups[crossing.group].id)!=disabled.end()){
            mission.skipOccurrence(crossing.ordinal);continue;
        }
        const auto& shape=missionAnchors.groups[crossing.group].capsules[crossing.capsule];
        if(!measurementPlacementPermitted(crossing.group,crossing.capsule,crossing.lap,shape)){mission.skipOccurrence(crossing.ordinal);continue;}
        ScintillaMission::CapsuleSpec spec;spec.id=id;spec.instance=++nextInstance;
        spec.module=uint64_t(crossing.ordinal)+1;spec.occurrence=crossing.ordinal;
        // Native route expiry, not a guessed elapsed travel time. The core's
        // deadline remains safely beyond the bounded duration of this race.
        spec.expires=std::numeric_limits<int64_t>::max();
        const auto activation=mission.activateCapsule(spec);
        if(!activation.accepted)mission.skipOccurrence(crossing.ordinal);
        if(activation.changed){
            const auto snapshot=mission.snapshot(id);
            if(snapshot.id){MissionPlacement placement;placement.instance=spec.instance;
                placement.center=SpatialVec3(shape.center.x,shape.center.y,shape.center.z);
                placement.radius=shape.radius;missionPlacements[id]=placement;}
        }
    }
    for(auto it=missionPlacements.begin();it!=missionPlacements.end();){
        const auto snapshot=mission.snapshot(it->first);
        if(snapshot.phase!=ScintillaMission::Phase::Shell&&snapshot.phase!=ScintillaMission::Phase::Open)
            it=missionPlacements.erase(it);
        else {const unsigned ordinal=unsigned(it->first-ScintillaMission::Runtime::CapsuleEntityBase),lap=ordinal/24,group=(ordinal%24)/2,capsule=ordinal%2;
            auto* kart=defenseHuman();if(!kart||(tuning.mission.withdrawAboveQualifiedSpeed&&std::abs(double(kart->getSpeed()))>ScintillaMissionMeasurement::qualifiedEnvelope(qualifiedBounds.capsules[ordinal]))){
                mission.expireCapsule(it->first,snapshot.instance);it=missionPlacements.erase(it);
            }else ++it;}
    }
}
AbstractKart* defenseAttacker()
{
    auto*w=World::getWorld();const auto&v=defense.view();auto owner=combatOwners.find(v.attacker);
    if(!w||!v.attacker||v.attacker>w->getNumKarts()||owner==combatOwners.end()||owner->second.instance!=v.instance)return nullptr;
    auto*k=w->getKart(unsigned(v.attacker-1));return owner->second.pointer==k?k:nullptr;
}
bool defenseGeometry(Defense::Candidate* c,Defense::Ballistic* b)
{
    auto*human=defenseHuman();auto*attacker=defenseAttacker();
    if(!human||!attacker||humanInstance!=defense.view().humanInstance)return false;
    const auto state=combat.snapshot(defense.view().attacker);
    if(!state.eligible||state.phase==ScintillaCombat::Phase::Stagger||state.phase==ScintillaCombat::Phase::StaggerPending||state.phase==ScintillaCombat::Phase::Recovery)return false;
    const auto phase=defense.view().phase;
    const bool launching=phase!=Defense::Phase::Flying&&phase!=Defense::Phase::AwaitImpact&&
        (defense.profile().solveDuringTelegraph||phase==Defense::Phase::LaunchPending);
    return ScintillaDefensePhysics::opportunity(human,attacker,defense.view().sector,c,b,defense.profile(),launching,relaxedHuman());
}
#ifndef SERVER_ONLY
bool commissionedRegion(double left,double top,double right,double bottom)
{
    for(size_t i=0;i<defenseVisibleRegions.size();i+=4)if(defenseVisibleRegions[i]<=left&&defenseVisibleRegions[i+1]<=top&&defenseVisibleRegions[i+2]>=right&&defenseVisibleRegions[i+3]>=bottom)return true;
    return false;
}
bool pointInOutput(const Frame&f,const SpatialVec3& point,double radius,bool allowRear=true)
{
    const SpatialVec3 delta(point.x-f.cameraPosition.x,point.y-f.cameraPosition.y,point.z-f.cameraPosition.z);
    const SpatialVec3 d(dot(delta,f.cameraRight),dot(delta,f.cameraUp),dot(delta,f.cameraForward));
    double u,v;if(allowRear&&f.layout.rearUV(Defense::Vec(d.x,d.y,d.z),&u,&v)){
        // Perspective extrema of a containing cube occur at its vertices.
        bool whole=true;for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1}){double cu,cv;whole=whole&&f.layout.rearUV(Defense::Vec(d.x+x*radius,d.y+y*radius,d.z+z*radius),&cu,&cv);}if(whole&&commissionedRegion(f.layout.rear[0]/f.layout.width,f.layout.rear[1]/f.layout.height,f.layout.rear[2]/f.layout.width,f.layout.rear[3]/f.layout.height))return true;
    }
    const double distance=std::sqrt(dot(delta,delta));if(distance<=radius)return false;
    SpatialVec3 unit(d.x/distance,d.y/distance,d.z/distance);
    const double alpha=std::asin(radius/distance);double du=0,dv=0;
    if(f.projection.kind=="equirectangular"){
        const double latitude=std::asin(std::max(-1.0,std::min(1.0,unit.y)));
        if(std::abs(latitude)+alpha>=3.14159265358979323846*.5)return false;
        du=alpha/(2*3.14159265358979323846*std::cos(std::abs(latitude)+alpha));dv=alpha/3.14159265358979323846;
        u=.5+std::atan2(unit.x,unit.z)/(2*3.14159265358979323846);v=.5-std::asin(std::max(-1.0,std::min(1.0,unit.y)))/3.14159265358979323846;
    }else{
        const double angle=-f.projection.domeTiltDeg*3.14159265358979323846/180,c=std::cos(angle),s=std::sin(angle);
        const SpatialVec3 r(unit.x,c*unit.y-s*unit.z,s*unit.y+c*unit.z);
        const double theta=std::acos(std::max(-1.0,std::min(1.0,r.z))),xy=std::hypot(r.x,r.y);
        if(theta+alpha>3.14159265358979323846*.5)return false;
        // Equidistant front-disc map has differential norm <=1 on the hemisphere.
        du=alpha;dv=alpha;
        u=.5+(xy?theta/(3.14159265358979323846*.5)*r.x/xy:0)*.5;
        v=.5-(xy?theta/(3.14159265358979323846*.5)*r.y/xy:0)*.5;
    }
    return f.layout.classify(u,v).context==Defense::Context::WorldOffense&&f.layout.worldRectFree(u-du,v-dv,u+du,v+dv)&&commissionedRegion(u-du,v-dv,u+du,v+dv);
}
Visibility defensePointVisibility(Frame&f,const SpatialVec3& point,double radius,bool allowRear=true)
{
    if(radius<=0||!pointInOutput(f,point,radius,allowRear))return Visibility::Occluded;
    SpatialVec3 d(point.x-f.cameraPosition.x,point.y-f.cameraPosition.y,point.z-f.cameraPosition.z);
    const double length=std::sqrt(dot(d,d));if(length<=radius)return Visibility::Occluded;
    d=SpatialVec3(d.x/length,d.y/length,d.z/length);Target target;target.center=point;target.radius=radius;
    return visible(f,target,d,length-radius);
}
Visibility missionPresentationVisibility(unsigned query,bool actual,int side,int64_t now,
    const ScintillaMission::Snapshot& snapshot)
{
    auto& q=missionPresentationQueries[query];
    const auto frames=consumer.presentedMissionFrames(now);
    const auto matches=[&](const Frame& frame){
        if(frame.missionHumanInstance!=mission.humanInstance()||!missionTetherFullMask(frame.id,side))return false;
        if(!actual)return true;
        return std::any_of(frame.targets.begin(),frame.targets.end(),[&](const Target& target){
            return target.kind==TargetKind::MissionTether&&target.id==snapshot.id&&
                target.instance==snapshot.instance&&target.mission.phaseRevision==snapshot.phaseRevision&&
                target.mission.phase==snapshot.phase;
        });
    };
    if(q.frame){
        const bool stillPresented=std::any_of(frames.begin(),frames.end(),[&](const PresentedFrame& record){
            return record.frame.id==q.frame->id&&record.frame.stream==q.frame->stream&&matches(record.frame);
        });
        if(!stillPresented){q.frame.reset();q.lease.reset();}
    }
    if(!q.frame)for(const auto& record:frames){
        if(!matches(record.frame))continue;
        auto frame=std::make_shared<Frame>(record.frame);
        frame->queryOwner=(uint64_t(1)<<63)+2+query;frame->visibilityLease=pinFrame(*frame);
        if(!frame->visibilityLease)continue;
        q.frame=frame;q.lease=queryLease(frame->queryOwner);break;
    }
    if(!q.frame)return Visibility::Pending;
    SpatialVec3 center;
    if(actual){
        const auto found=std::find_if(q.frame->targets.begin(),q.frame->targets.end(),[&](const Target& t){
            return t.kind==TargetKind::MissionTether&&t.id==snapshot.id&&t.instance==snapshot.instance;
        });
        if(found==q.frame->targets.end())return Visibility::Unavailable;
        center=found->center;
    }else center=q.frame->missionTetherCandidates[side];
    const SpatialVec3 delta(center.x-q.frame->cameraPosition.x,center.y-q.frame->cameraPosition.y,
        center.z-q.frame->cameraPosition.z);
    if(!ScintillaMission::Profile::inRange(ScintillaMission::Kind::Tether,dot(delta,delta),.5))
        return Visibility::Occluded;
    return defensePointVisibility(*q.frame,center,.5,false);
}
Visibility candidateVisibility(const Defense::Candidate&c,int64_t now)
{
    auto saved=sourceProofs.find(c.id);
    if(saved!=sourceProofs.end()&&saved->second.instance==c.instance&&saved->second.low<=now&&now-saved->second.low<=150000)return saved->second.visible?Visibility::Visible:Visibility::Occluded;
    if(candidateEvidenceFrame&&now-candidateEvidenceStarted>PendingTimeoutUs){candidateEvidenceFrame.reset();candidateQueryLease.reset();}
    // Selection asks in priority order and stops on Pending. If live geometry
    // changes that order, coalesce the one query towards the new best candidate.
    if(candidateEvidenceFrame&&(candidateEvidenceID!=c.id||candidateEvidenceInstance!=c.instance)){candidateEvidenceFrame.reset();candidateQueryLease.reset();}
    if(!candidateEvidenceFrame)for(const auto&r:consumer.presentedDefenseFrames(now)){
        const auto target=std::find_if(r.frame.targets.begin(),r.frame.targets.end(),[&](const Target&t){return t.id==c.id&&t.combat.instance==c.instance;});
        if(target==r.frame.targets.end()||!pointInOutput(r.frame,target->center,target->radius))continue;
        auto f=std::make_shared<Frame>(r.frame);f->queryOwner=(uint64_t(1)<<63)+1;f->visibilityLease=pinFrame(*f);if(!f->visibilityLease)continue;
        candidateEvidenceFrame=f;candidateQueryLease=queryLease(f->queryOwner);candidateEvidenceID=c.id;candidateEvidenceInstance=c.instance;candidateEvidenceStarted=now;break;
    }
    if(!candidateEvidenceFrame)return Visibility::Pending;
    auto&f=*candidateEvidenceFrame;const auto target=std::find_if(f.targets.begin(),f.targets.end(),[&](const Target&t){return t.id==c.id&&t.combat.instance==c.instance;});
    Visibility visible=target==f.targets.end()?Visibility::Unavailable:defensePointVisibility(f,target->center,target->radius);
    if(visible==Visibility::Pending)return visible;
    int64_t low=0;for(const auto&r:consumer.presentedDefenseFrames(now))if(r.frame.id==f.id&&r.frame.stream==f.stream)low=r.low;
    if(!low)visible=Visibility::Unavailable;
    if(sourceProofs.size()>=32&&!sourceProofs.count(c.id))sourceProofs.erase(sourceProofs.begin());
    auto&proof=sourceProofs[c.id];proof.instance=c.instance;proof.low=low?low:now;proof.visible=visible==Visibility::Visible;
    candidateEvidenceFrame.reset();candidateQueryLease.reset();return visible;
}
Defense::Observation defenseObservation(int64_t now)
{
    Defense::Observation o;auto* human=defenseHuman();
    o.humanEligible=relaxedHuman()?ScintillaDefensePhysics::eligibleHumanCore(human):ScintillaDefensePhysics::eligibleHuman(human);
    o.humanTransient=ScintillaDefensePhysics::humanTransient(human);
    Defense::Candidate c;Defense::Ballistic b;o.geometryValid=defenseGeometry(&c,&b);
    const auto v=defense.view();
    o.videoContinuous=consumer.defenseVideoReady(now)&&lastDefenseVisibleThreat==v.threat&&lastDefenseVisibleLow<=now&&now-lastDefenseVisibleLow<=150000;
    if(v.phase==Defense::Phase::Idle||v.phase==Defense::Phase::Cooldown)return o;
    if(!consumer.defenseVideoReady(now)){o.visibilityLost=true;return o;}
    if(defenseEvidenceFrame&&(now-defenseEvidenceStarted>PendingTimeoutUs||defenseEvidenceFrame->defense.threat!=v.threat||defenseEvidenceFrame->defense.phase!=v.phase))clearDefenseEvidence();
    const auto records=consumer.presentedDefenseFrames(now);
    if(!defenseEvidenceFrame)for(const auto&r:records){
        if(r.frame.defense.threat!=v.threat||r.frame.defense.phase!=v.phase||r.frame.defense.instance!=v.instance||r.frame.defense.humanInstance!=v.humanInstance)continue;
        auto frame=std::make_shared<Frame>(r.frame);frame->queryOwner=uint64_t(1)<<63;frame->visibilityLease=pinFrame(*frame);
        if(!frame->visibilityLease)continue;defenseEvidenceFrame=frame;defenseQueryLease=queryLease(frame->queryOwner);defenseEvidenceStarted=now;break;
    }
    if(!defenseEvidenceFrame)return o;
    auto receipt=std::find_if(records.begin(),records.end(),[&](const PresentedFrame&r){return r.frame.id==defenseEvidenceFrame->id&&r.frame.stream==defenseEvidenceFrame->stream;});
    if(receipt==records.end()){clearDefenseEvidence();return o;}
    auto&f=*defenseEvidenceFrame;
    const Visibility source=f.defenseSourceLineClear?defensePointVisibility(f,f.defenseSource,f.defenseSourceRadius):Visibility::Occluded;
    if(source==Visibility::Pending)return o;
    if(source!=Visibility::Visible){o.visibilityLost=true;o.videoContinuous=false;lastDefenseVisibleLow=0;clearDefenseEvidence();return o;}
    if(f.defense.hasPulse){
        const auto&p=f.defense.pulse;const Visibility pulse=defensePointVisibility(f,SpatialVec3(p.x,p.y,p.z),.12);
        if(pulse==Visibility::Pending)return o;if(pulse!=Visibility::Visible){o.visibilityLost=true;o.videoContinuous=false;lastDefenseVisibleLow=0;clearDefenseEvidence();return o;}
    }
    lastDefenseVisibleLow=receipt->low;lastDefenseVisibleThreat=v.threat;
    o.videoContinuous=true;o.presentedThreat=f.defense.threat;o.presentedContact=f.defense.contact;o.presentedPhase=f.defense.phase;
    o.presentedLow=receipt->low;o.presentedHigh=receipt->high;clearDefenseEvidence();return o;
}
#endif
void serviceMissionPresentation(int64_t now)
{
    if(!mission.configured()||missionTerminal())return;
    const auto snapshot=mission.snapshot(ScintillaMission::Runtime::TetherEntityId);
    if(!snapshot.id){
        clearMissionPresentationQueries();tetherPlacementInstance=0;tetherSide=-1;tetherCreatedUs=0;
        if(tetherPresentationHold&&missionStepFailure.empty())setMissionPresentationHeld(false);
        tetherPresentationHold=false;return;
    }
    observeNewTether(now);
    if(snapshot.phase!=ScintillaMission::Phase::TetherAwaitingPresentation)return;
#ifndef SERVER_ONLY
    if(tetherSide<0){
        for(int side=0;side<2;++side)
            if(missionPresentationVisibility(unsigned(side),false,side,now,snapshot)==Visibility::Visible){
                tetherSide=side;clearMissionPresentationQueries();break;
            }
    }
    if(tetherSide>=0&&missionPresentationVisibility(2,true,tetherSide,now,snapshot)==Visibility::Visible&&
       mission.confirmTetherPresented(snapshot.id,snapshot.instance,snapshot.phaseRevision)){
        clearMissionPresentationQueries();
        if(tetherPresentationHold&&missionStepFailure.empty())setMissionPresentationHeld(false);
        tetherPresentationHold=false;publishMissionSnapshot();return;
    }
#endif
    if(tetherCreatedUs>0&&now>=tetherCreatedUs&&now-tetherCreatedUs>=250000){
        tetherPresentationHold=true;setMissionPresentationHeld(true);
    }
}
void updateDefense(bool admitted)
{
    if(!defense.configured())return;
    const int64_t now=nowUs();auto*human=defenseHuman();
    if(!admitted||!defenseBound()){defense.update(simulationTick,now,false,Defense::Observation(),{});clearDefenseEvidence();ScintillaDefensePhysics::clear();return;}
    Defense::Observation o;
#ifndef SERVER_ONLY
    o=defenseObservation(now);
#endif
    const auto before=defense.view();
    if(before.phase!=Defense::Phase::Idle&&before.phase!=Defense::Phase::Cooldown)crewDefenseThreatTick=simulationTick;
    const int previousCrewDefender=defense.owner();
    // Missing/negative current proof is a real interruption, never hidden by a later frame.
    defense.update(simulationTick,now,true,o,consumer.defenseBeams(now));
    if(before.phase!=Defense::Phase::Cooldown&&defense.view().phase==Defense::Phase::Cooldown&&defense.view().outcome==1&&
        previousCrewDefender>=0&&size_t(previousCrewDefender)<defense.crew().size())
        recordCrewCredit(defense.crew()[previousCrewDefender],Crew::ContributionKind::Defense);
    if(before.phase!=Defense::Phase::Flying||defense.view().phase!=Defense::Phase::Flying)ScintillaDefensePhysics::clear();
    if(before.phase==Defense::Phase::AwaitWarning&&defense.view().phase==Defense::Phase::Telegraph)SFXManager::get()->quickSound("pre_start_race");
    if(defense.view().phase==Defense::Phase::LaunchPending&&o.presentedThreat==defense.view().threat&&o.presentedPhase==Defense::Phase::LaunchPending&&o.presentedHigh<=now){
        Defense::Candidate c;Defense::Ballistic b;
        const auto& profile=defense.profile();const auto& h=human->getBody()->getWorldTransform().getOrigin();
        const bool geometry=defenseGeometry(&c,&b);
        const Defense::Vec carrier=profile.inheritVelocity?c.sourceVelocity:Defense::Vec();
        if(!geometry||!Defense::Ballistic::solve(Defense::Vec(h.x(),h.y(),h.z())-c.origin,c.velocity-carrier,profile,&b))defense.cancel("launch_geometry_changed");
        else defense.launch(c.origin,carrier+b.direction*profile.speed);
    }
    if(defense.view().phase==Defense::Phase::Flying){
        auto trace=ScintillaDefensePhysics::take();
        if(trace.available){
            const Defense::Vec delta=trace.point-trace.humanPosition;
            const bool sector=delta.length()<1e-6?trace.hit:Defense::Runtime::sector(delta,trace.humanForward,trace.humanUp,defense.view().sector)!=Defense::Sector::None;
            if(!trace.valid||!sector)defense.cancel(!trace.valid?"collision_uncertain":"sector_changed");
            else if(trace.hit)defense.contact(trace.human,trace.point,now);else defense.advancePulse(trace.point);
        }else if(before.phase==Defense::Phase::Flying)defense.cancel("collision_step_missing");
    }
    if(defense.wantsImpact()){
        // Validate the live owners after all SC2 and native-world effects, again at commit.
        Defense::Candidate c;Defense::Ballistic b;const bool current=defenseGeometry(&c,&b);
        const bool applied=current&&ScintillaDefensePhysics::applyImpact(human,defense.profile().impactRetained,relaxedHuman());
        if(defense.confirmImpact(applied)&&applied&&mission.required()&&mission.active()&&!missionTerminal()){
            const auto& impact=defense.impactEvent();ScintillaMission::Impact event;
            event.race=impact.race;event.threat=impact.threat;event.humanInstance=impact.humanInstance;
            mission.consumeImpact(event);observeNewTether(now);
        }
    }
    bool soloBusy=false;for(const auto&s:combat.snapshots())if(s.count||s.phase==ScintillaCombat::Phase::Acquiring||s.phase==ScintillaCombat::Phase::Breached)soloBusy=true;
    if(soloBusy)soloIdleSince=simulationTick;
    if(consumer.defenseVideoReady(now)&&defense.readyForCandidate()&&(lastRoster.size()!=1||simulationTick-soloIdleSince>=(defense.profile().fps+1)/2)){
        auto*w=World::getWorld();std::vector<Defense::Candidate> candidates;
        for(const auto&e:combatOwners){
            const auto state=combat.snapshot(e.first);
            if(!state.eligible||state.phase==ScintillaCombat::Phase::Stagger||state.phase==ScintillaCombat::Phase::StaggerPending||state.phase==ScintillaCombat::Phase::Recovery||!defense.candidateReady(e.first,e.second.instance))continue;
            auto*k=w->getKart(unsigned(e.first-1));Defense::Candidate c;Defense::Ballistic b;
            // Pause starts no warning during a boost; Ignore disregards it.
            const bool ignore=tuning.defense.boostPolicy==ScintillaTuning::BoostPolicy::Ignore;
            if(!ScintillaDefensePhysics::opportunity(human,k,Defense::Sector::None,&c,&b,defense.profile(),defense.profile().solveDuringTelegraph,ignore))continue;
            c.id=e.first;c.instance=e.second.instance;c.humanInstance=humanInstance;
#ifndef SERVER_ONLY
            if(!ScintillaDefensePhysics::cameraLineClear(Defense::Vec(current.cameraPosition.x,current.cameraPosition.y,current.cameraPosition.z),human,k))continue;
#else
            continue;
#endif
            candidates.push_back(c);
        }
#ifndef SERVER_ONLY
        Defense::Candidate best;
        const bool found=defense.chooseCandidate(std::move(candidates),[&](const Defense::Candidate&c){
            const auto proof=candidateVisibility(c,now);
            return proof==Visibility::Visible?Defense::CandidateVisibility::Visible:
                proof==Visibility::Pending?Defense::CandidateVisibility::Pending:Defense::CandidateVisibility::Rejected;
        },&best);
        if(found&&defense.begin(best,now)){
            candidateEvidenceFrame.reset();candidateQueryLease.reset();
        }
#endif
    }
    if(defense.view().phase!=Defense::Phase::Idle&&defense.view().phase!=Defense::Phase::Cooldown)crewDefenseThreatTick=simulationTick;
    consumer.setDefenseContext(defenseBound(),defense.feedback());
}
void updateMissionCues()
{
    if(!mission.required())return;
    if(missionCueRace!=raceEpoch){
        missionCueRace=raceEpoch;missionCueFinish=0;missionCueSlots={};
    }
    const auto result=missionTerminalRecord.valid?missionTerminalRecord:RelaySessionController::get()->getMissionResult();
    const auto slots=result.valid?result.slots:mission.slots();
    bool lost=false,detached=false,collected=false,recovered=false,beacon=false;
    for(unsigned i=0;i<3;++i){
        const auto& previous=missionCueSlots[i];const auto& next=slots[i];
        lost=lost||(previous.state==ScintillaMission::SlotState::Tethered&&next.state==ScintillaMission::SlotState::Empty);
        detached=detached||(previous.state==ScintillaMission::SlotState::Secured&&next.state==ScintillaMission::SlotState::Tethered);
        recovered=recovered||(previous.state==ScintillaMission::SlotState::Tethered&&
            next.state==ScintillaMission::SlotState::Secured&&previous.module==next.module);
        collected=collected||(next.state==ScintillaMission::SlotState::Secured&&
            (previous.state==ScintillaMission::SlotState::Empty||previous.module!=next.module));
        beacon=beacon||(!previous.beaconLit&&next.beaconLit);
    }
    // Bounded state receipts, not a queue of old voices waiting through pause.
    missionCueSlots=slots;
    std::string message,sound;
    if(result.valid&&result.finishSequence!=missionCueFinish){
        missionCueFinish=result.finishSequence;
        message=!result.natural?"Sessione interrotta. Missione non completata.":
            result.outcome=="mission_victory"?"Missione compiuta: primi, con tutti i moduli.":
            result.outcome=="race_won_cargo_incomplete"?"Gara vinta. Consegna incompleta.":
            result.outcome=="cargo_delivered_race_lost"?"Carico consegnato. Primo posto mancato.":
            "Missione non completata.";
        message+=" Posizione: "+(result.natural?std::to_string(result.officialTeamRacePosition):"non disponibile");
        message+=". Moduli: "+std::to_string(result.securedCount)+"/3.";
        sound=result.outcome=="mission_victory"?"wee":"anvil";
    }else if(mission.active()){
        if(lost){message="Modulo perso. Cerca una prossima capsula.";sound="anvil";}
        else if(detached){message="Modulo sganciato: recuperalo con il fascio.";sound="anvil";}
        else if(recovered){message="Modulo sul cavo recuperato.";sound="energy_bar_full";}
        else if(collected){message="Carico aggiornato: "+std::to_string(mission.securedCount())+"/3 moduli.";sound="energy_bar_full";}
        else if(beacon){message="Faro confermato. Il carico resta a bordo.";sound="energy_bar_full";}
        const auto phase=defense.view().phase;
        const bool danger=phase!=Defense::Phase::Idle&&phase!=Defense::Phase::Cooldown;
        const auto session=RelaySessionController::get()->getSnapshot();
        if(danger||((session.m_rotation.m_handoff_active||session.m_rotation.m_handoff_warning_active)&&!lost&&!detached))sound.clear();
    }
    if(!sound.empty()&&SFXManager::get()->soundExist(sound))SFXManager::get()->quickSound(sound);
    auto* world=World::getWorld();auto* gui=world?world->getRaceGUI():nullptr;
    if(gui&&!message.empty())gui->addMessage(StringUtils::utf8ToWide(message),defenseHuman(),
        result.valid?14.f:3.f,video::SColor(255,255,245,190),true,true,true);
}
}
void beforeDefensePhysics()
{
    if(defenseBound()&&defense.view().phase==Defense::Phase::Flying&&!RelaySessionController::get()->isWorldHeld())ScintillaDefensePhysics::before(defenseHuman(),defenseAttacker(),defense.view().pulse,defense.velocity(),defense.profile().fps,relaxedHuman());
}
void afterDefensePhysics(){ScintillaDefensePhysics::after();}
Reply rebindSpatialParticipants(const std::string&body)
{
    refreshCombatInput();auto*w=World::getWorld();
    if(!defense.required()||!w||w->getIdent()!="IMMERSIVE_RELAY"||!localCombatSupported()||!RelaySessionController::get()->isWorldHeld())return Reply(409,"{\"ok\":false,\"reason\":\"participant_rebind_requires_held_sc3_world\"}");
    const uint64_t previous=consumer.inputEpoch();
    auto reply=consumer.rebindParticipants(body,std::to_string(rosterEpoch),nowUs());
    if(reply.status==200&&consumer.inputEpoch()!=previous){defense.suspend("participants_rebound");defensePrepared=false;combat.suspendInput();clearDefenseEvidence();ScintillaDefensePhysics::clear();refreshCombatInput();}
    return reply;
}
Reply setDefenseIntent(const std::string&body)
{
    bool required=false;auto valid=consumer.validateDefenseIntent(body,std::to_string(raceEpoch),std::to_string(defense.intentRevision()),&required);if(valid.status!=200)return valid;
    if(required&&missionMeasurementActive())return Reply(409,"{\"ok\":false,\"error\":\"mission_measurement_active\"}");
    if(!required&&mission.required())return Reply(409,"{\"ok\":false,\"error\":\"mission_requires_defense\"}");
    auto*c=RelaySessionController::get();auto*w=World::getWorld();
    if(!w||w->getIdent()!="IMMERSIVE_RELAY"||!c->isWorldHeld()||(required&&(!localCombatSupported()||c->getSnapshot().m_crew_mode!="disabled")))return Reply(409,"{\"ok\":false,\"error\":\"defense_intent_requires_held_local_world\"}");
    if(!defense.setRequired(required))return Reply(409,"{\"ok\":false,\"error\":\"defense_requires_race_reset\"}");
    if(required)combat.setRequired(true);refreshCombatInput();return Reply(200,defenseStateJSON());
}
Reply prepareDefense(const std::string&body)
{
    refreshRoster();DefenseAdmission a;auto valid=consumer.validateDefensePrepare(body,std::to_string(raceEpoch),std::to_string(rosterEpoch),&a,mission.required());if(valid.status!=200)return valid;
    auto*c=RelaySessionController::get();auto*w=World::getWorld();
    if(!defense.required()||!active||!combat.configured()||!consumer.prepared()||combat.inputEpoch()!=consumer.inputEpoch()||combat.crew()!=lastRoster||!w||w->getIdent()!="IMMERSIVE_RELAY"||!c->isWorldHeld()||!localCombatSupported()||c->getSnapshot().m_crew_mode!="disabled")return Reply(409,"{\"ok\":false,\"error\":\"defense_requires_prepared_local_combat\"}");
    if(!defense.configured()&&w->getTimeTicks()>0)return Reply(409,"{\"ok\":false,\"error\":\"defense_requires_race_reset\"}");
    if(!SFXManager::get()->soundExist("pre_start_race"))return Reply(409,"{\"ok\":false,\"error\":\"defense_audio_unavailable\"}");
    const auto l=Defense::Layout::create(current.projection.width,current.projection.height,mission.required(),false,crew.required());
    auto covered=[&](double x0,double y0,double x1,double y1){for(size_t i=0;i<a.regions.size();i+=4)if(a.regions[i]<=x0&&a.regions[i+1]<=y0&&a.regions[i+2]>=x1&&a.regions[i+3]>=y1)return true;return false;};
    bool coverage=l.valid()&&covered(l.rear[0]/l.width,l.rear[1]/l.height,l.rear[2]/l.width,l.rear[3]/l.height);
    for(unsigned i=0;i<5&&coverage;++i){double x,y,r;l.node(i,&x,&y,&r);coverage=covered(x-r/l.width,y-r/l.height,x+r/l.width,y+r/l.height);}
    const double s=std::min(l.width,l.height);
    coverage=coverage&&covered(.5-.21*s/l.width,.5+.21*s/l.height,.5+.21*s/l.width,.5+.26*s/l.height)&&covered(.5-.1875*s/l.width,.5+.0125*s/l.height,.5+.1875*s/l.width,.5+.1525*s/l.height);
    if(mission.required()){
        for(unsigned i=0;i<3&&coverage;++i){
            double x,y,r;l.cargo(i,&x,&y,&r);coverage=covered(x-r/l.width,y-r/l.height,x+r/l.width,y+r/l.height);
            l.beacon(i,&x,&y,&r);coverage=coverage&&covered(x-r/l.width,y-r/l.height,x+r/l.width,y+r/l.height);
        }
        coverage=coverage&&covered(.5-.38*s/l.width,.5-.04*s/l.height,.5+.38*s/l.width,.5+.20*s/l.height);
    }
    if(!coverage)return Reply(409,"{\"ok\":false,\"error\":\"defense_layout_not_visible\"}");
    if(defenseBound()&&defenseEvidence==a.evidence){if(defenseVisibleRegions!=a.regions)return Reply(409,"{\"ok\":false,\"error\":\"defense_visibility_evidence_changed\"}");return Reply(200,defenseStateJSON());}
    if(!tuningError.empty())return Reply(409,"{\"ok\":false,\"error\":\"tuning_invalid\",\"detail\":"+ScintillaJSON::quote(tuningError)+"}");
    if(!defense.prepare(raceEpoch,consumer.inputEpoch(),lastRoster,stk_config->getPhysicsFPS(),tuning.defense))return Reply(409,"{\"ok\":false,\"error\":\"defense_profile_conflict\"}");
    defenseProjection=current.projection;defenseRoute=consumer.activeRouteRevision();defenseContext=consumer.activeProjectionContextId();defenseRoster=rosterEpoch;defenseEvidence=a.evidence;defenseVisibleRegions=a.regions;defensePrepared=true;defenseLayoutRevision=l.revision();
    clearDefenseEvidence();
#ifndef SERVER_ONLY
    candidateEvidenceFrame.reset();candidateQueryLease.reset();sourceProofs.clear();
#endif
    ScintillaDefensePhysics::clear();refreshCombatInput();return Reply(200,defenseStateJSON());
}
std::string defenseStateJSON()
{
    refreshRoster();
    const auto l=Defense::Layout::create(current.projection.width,current.projection.height,mission.required(),false,crew.required());
    const auto& v=defense.view();const auto& stats=defense.statistics();const auto& impact=defense.impactEvent();
    const auto escaped=[](const std::string& value){
        std::string out="\"";const char hex[]="0123456789abcdef";
        for(unsigned char c:value){
            if(c=='"'||c=='\\'){out+='\\';out+=char(c);}
            else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
            else out+=char(c);
        }
        return out+'"';
    };
    std::ostringstream o;
    o<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-defense/1\""
     <<",\"profile\":\"sc3_sector_rear_readonly_v1\",\"layoutProfile\":\""<<l.profile()<<"\",\"layoutRevision\":\""<<l.revision()<<'"'
     <<",\"required\":"<<(defense.required()?"true":"false")
     <<",\"policyActive\":"<<(defense.configured()?"true":"false")
     <<",\"prepared\":"<<(defenseBound()?"true":"false")
     <<",\"intentRevision\":\""<<defense.intentRevision()<<'"'
     <<",\"raceEpoch\":\""<<raceEpoch<<"\",\"rosterRevision\":\""<<rosterEpoch<<'"'
     <<",\"inputSessionID\":"<<escaped(consumer.inputSessionID())
     <<",\"inputEpoch\":\""<<consumer.inputEpoch()<<'"'
     <<",\"routeRevision\":"<<escaped(consumer.activeRouteRevision())
     <<",\"projectionContextId\":"<<escaped(consumer.activeProjectionContextId())
     <<",\"visibilityEvidenceID\":"<<escaped(defenseEvidence)
     <<",\"layout\":{\"width\":"<<l.width<<",\"height\":"<<l.height<<",\"rearRect\":[";
    for(int i=0;i<4;++i){if(i)o<<',';o<<(l.valid()?l.rear[i]/(i%2?l.height:l.width):0);}
    o<<"]},\"capabilities\":{\"rear_alert\":true,\"rear_visible_view\":true,\"rear_targetable_view\":false,\"projectile_interception\":false}"
     <<",\"blockedReason\":"<<escaped(defenseBound()?"":defense.required()?"defense_not_prepared":"")
     <<",\"energy\":"<<defense.energy()<<",\"reserve\":"<<(defense.profile().q?double(defense.profile().reserve)/defense.profile().q:0)
     <<",\"heat\":"<<defense.heat()<<",\"phase\":"<<int(v.phase)
     <<",\"boostCost\":"<<(defense.profile().q?double(defense.profile().boostCost)/defense.profile().q:double(tuning.defense.boostCost))
     <<",\"threatId\":\""<<v.threat<<"\",\"sector\":"<<int(v.sector)<<",\"attackerId\":\""<<v.attacker<<'"'
     <<",\"deadlineTick\":"<<defense.deadline()<<",\"simulationTick\":"<<simulationTick
     <<",\"cooldownUntilTick\":"<<defense.nextAttack()<<",\"channelOwner\":"<<defense.owner()<<",\"boostOwner\":"<<defense.boostOwner()
     <<",\"boostStatus\":"<<int(defense.boostStatus(ScintillaDefensePhysics::eligibleHuman(defenseHuman())))
     <<",\"statistics\":{\"cancelled\":\""<<stats.cancelled<<"\",\"blocked\":\""<<stats.blocked<<"\",\"impacts\":\""<<stats.impacts<<"\",\"missed\":\""<<stats.missed<<"\"}"
     <<",\"lastReason\":"<<escaped(defense.reason())
     <<",\"impactEvent\":{\"raceEpoch\":\""<<impact.race<<"\",\"threatId\":\""<<impact.threat<<"\",\"humanInstance\":\""<<impact.humanInstance<<"\",\"tick\":"<<impact.tick<<"}}";
    return o.str();
}

namespace {
void invalidateState(const std::string& reason,bool preserveSpatialGrant)
{
    const Projection projection=current.projection;
    const bool retained=preserveSpatialGrant&&consumer.resetForRace();
    if(!retained)consumer.reset(reason);
    combat.suspendInput();defense.suspend(reason);defensePrepared=false;clearDefenseEvidence();
    clearMissionPresentationQueries();
    releaseMissionPresentationAssets();
    // Close synchronous/native Ready paths immediately, including between a
    // projection/world invalidation and the next authoritative race update.
    mission.suspendInput();missionStepOpen=false;
    RelaySessionController::get()->setScintillaStartPermitted(!combat.required()&&!defense.required()&&!mission.required());
#ifndef SERVER_ONLY
    candidateEvidenceFrame.reset();candidateQueryLease.reset();sourceProofs.clear();
#endif
    ScintillaDefensePhysics::clear();++generation;
#ifndef SERVER_ONLY
    clearGraphics();
#endif
    current=Frame();publishedFrames.clear();registered.clear();
    if(retained)current.projection=projection;
    else if(active)changeStream();
}
}
void invalidate(const std::string& reason){if(reason!="scintilla_enabled")interruptMissionMeasurement(reason);resetMissionMeasurementProofs();invalidateState(reason,false);}
void setEnabled(bool value)
{
    if(active==value)return;
    active=value;invalidate(value?"scintilla_enabled":"scintilla_disabled");
#ifdef ENABLE_RELAY_LOCAL_API
    RelayGameDispatcher::setSpatialClock(value?&socketClock:nullptr);
#endif
#ifndef SERVER_ONLY
    consumer.setLiveTargetOracle([](uint64_t id,uint64_t instance){
        if(id>=ScintillaMission::Runtime::CapsuleEntityBase)return liveMissionTarget(id,instance);
        return TargetLifetime::isLive(registered,id,instance,currentTarget);
    });
    consumer.setVisibilityOracle(visible);
    consumer.setFramePinOracle(pinFrame);
    consumer.setFramePendingOracle([](const Frame& frame){
        auto record=std::find_if(depths.begin(),depths.end(),[&](const std::shared_ptr<DepthFrame>& d){return d->id==frame.id&&d->stream==frame.stream;});
        return record!=depths.end()&&!signaled((*record)->copyFence);
    });
    consumer.setQueryLeaseOracle(queryLease);
#else
    consumer.setLiveTargetOracle([](uint64_t,uint64_t){return false;});
#endif
}
std::string historyJSON()
{
    std::ostringstream out;out<<"{\"ok\":true,\"contract\":\"tholus.stk-spatial-history/1\",\"stream\":\""<<stream<<"\",\"metadataFrameIds\":[";
    bool first=true;
    for(const Frame& frame:publishedFrames)if(currentPublished(frame))
    {if(!first)out<<',';first=false;out<<frame.id;}
    out<<"],\"depthFrameIds\":[";first=true;
#ifndef SERVER_ONLY
    for(const auto& depth:depths)
    {
        auto metadata=std::find_if(publishedFrames.begin(),publishedFrames.end(),[&](const Frame& frame){return frame.id==depth->id&&currentPublished(frame);});
        if(depth->stream!=stream||metadata==publishedFrames.end()||!signaled(depth->copyFence))continue;
        if(!first)out<<',';first=false;out<<depth->id;
    }
#endif
    out<<"]}";return out.str();
}
void seedPreparedHistory()
{
    // Rebind only immutable, genuinely published producer metadata. Coordinates,
    // capture time and world/target instances remain those of the original frame.
    for(const Frame& supplied:publishedFrames)if(currentPublished(supplied))
    {
        Frame frame=supplied;frame.routeRevision=consumer.activeRouteRevision();
        frame.projectionContextId=consumer.activeProjectionContextId();consumer.addFrame(frame);
    }
}
namespace {
ScintillaMission::Result missionObservation()
{
    if(missionTerminalRecord.valid) return missionTerminalRecord;
    if(mission.result().valid) return mission.result();
    ScintillaMission::Result result;
    result.raceEpoch=raceEpoch;
    result.missionRevision=mission.configured()?mission.revision():1;
    result.finishTick=mission.tick();result.slots=mission.slots();
    result.securedCount=mission.securedCount();
    result.cargoCompleteAtFinish=result.securedCount==ScintillaMission::Profile::SlotCount;
    return result;
}
void closeMissionAdmission()
{
    missionStepOpen=false;combat.suspendInput();defense.suspend("mission_finished");crew.suspend();
    clearMissionPresentationQueries();
    ScintillaDefensePhysics::clear();mission.suspendInput();
    consumer.setCombatContext(false,simulationTick,{},raceEpoch);
    const auto gameplay=RelaySessionController::get()->getSpatialGameplayState();
    consumer.tick(nowUs(),false,gameplay.m_resume_revision,gameplay.m_resume_at_us);
    RelaySessionController::get()->setScintillaStartPermitted(false);
}
}
ScintillaMission::Runtime& missionRuntime(){return mission;}
const std::string& missionAnchorHash(){return anchorHash;}
const std::string& missionAnchorStatus(){return anchorStatus;}
namespace {
bool readMissionFile(const std::string& path,size_t limit,std::string* out)
{
    if(!out)return false;std::ifstream file(path.c_str(),std::ios::binary|std::ios::ate);if(!file)return false;
    const auto length=file.tellg();if(length<=0||uint64_t(length)>limit)return false;
    std::string bytes(size_t(length),'\0');file.seekg(0,std::ios::beg);file.read(&bytes[0],bytes.size());
    if(file.gcount()!=std::streamsize(bytes.size())||file.bad()||file.peek()!=std::char_traits<char>::eof())return false;
    *out=std::move(bytes);return true;
}
std::string missionSHA256(const std::string& bytes)
{
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(uint8_t b:Crypto::sha256(bytes))out<<std::setw(2)<<unsigned(b);
    return out.str();
}
}
#include "modes/immersive_relay/spatial/scintilla_crew_text_native.hpp"
bool prepareMissionPresentationAssets(std::string* reason)
{
    if(!RelaySessionController::get()->isWorldHeld()){
        if(reason)*reason="mission_asset_load_requires_held_world";return false;
    }
#ifndef SERVER_ONLY
    missionAssetsInvalidated=true;
    if(!missionResultAtlas){
        const std::string path=file_manager->getAsset("immersive_relay/scintilla/mission_results_it_v1.png");
        std::string bytes;
        if(!readMissionFile(path,1024*1024,&bytes)){
            if(reason)*reason="mission_result_asset_unavailable";return false;
        }
        auto* texture=irr_driver->getTexture(path);
        if(!texture||texture->getOriginalSize().Width!=2048||texture->getOriginalSize().Height!=800||
           texture->getSize().Width!=2048||texture->getSize().Height!=800||!texture->getTextureHandler()){
            if(reason)*reason="mission_result_asset_invalid";return false;
        }
        missionResultAtlas=texture;
    }
    if(crew.required()&&!prepareCrewTextAtlas(reason))return false;
    for(const char* sound:{"anvil","energy_bar_full","wee"})
        if(!SFXManager::get()->soundExist(sound)){
            if(reason)*reason="mission_audio_unavailable";return false;
        }
    missionAssetsInvalidated=false;
    if(reason)reason->clear();return true;
#else
    if(reason)*reason="mission_renderer_unavailable";return false;
#endif
}
void releaseMissionPresentationAssets(const void* texture)
{
#ifndef SERVER_ONLY
    releaseCrewTextAtlas(texture);
    if(!missionResultAtlas||(texture&&texture!=missionResultAtlas))return;
    // Crew results copy these same outcome rows into their bounded atlas.
    // Retiring the source invalidates that copy before a held reconstruction.
    releaseCrewTextAtlas(nullptr);
    // No GL calls: cache teardown may also precede destruction of the context.
    // The texture remains driver-owned; never retain or dereference it again.
    missionResultAtlas=nullptr;missionAtlasBound=false;
    missionAssetsInvalidated=mission.required();
    if(auto* controller=RelaySessionController::getExisting())
        if(mission.required())controller->setScintillaStartPermitted(false);
    missionStepOpen=false;mission.suspendInput();
#else
    (void)texture;
#endif
}
void invalidateMissionContent(){invalidateMissionQualificationCache();resetCrewPartyZones();}
bool prepareMissionAnchors(std::string* reason)
{
    auto fail=[&](const std::string& value){if(reason)*reason=value;anchorStatus=value;return false;};
    if(!RelaySessionController::get()->isWorldHeld())return fail("mission_anchor_load_requires_held_world");
#ifndef SERVER_ONLY
    auto* graph=DriveGraph::get();auto* track=Track::getCurrentTrack();
    if(!graph||!track||!std::isfinite(graph->getLapLength())||graph->getLapLength()<=0)
        return fail("mission_native_driveline_unavailable");
    if(anchorRace==raceEpoch&&missionTraversal.prepared()){
        Anchors::NativeTrack cached;cached.trackId=RaceManager::get()->getTrackName();
        cached.trackHash=missionAnchors.trackHash;cached.reverse=graph->isReverse();
        cached.lapLength=graph->getLapLength();
        for(unsigned i=0;i<graph->getNumNodes();++i)
            cached.nodeFractions.push_back(graph->getDistanceFromStart(i)/cached.lapLength);
        std::string error;
        if(RaceManager::get()->getNumLaps()!=int(missionTraversal.totalLaps())||
           (measurementLoading?missionCrewCount():unsigned(lastRoster.size()))!=anchorCrew||!Anchors::validateNative(missionAnchors,cached,&error))
            return fail("mission_anchor_configuration_changed");
        anchorStatus=missionAnchors.authoringStatus;if(!measurementLoading)refreshMissionQualification(reason);else if(reason)reason->clear();return true;
    }
    std::string bytes,quads,edges;
    if(!readMissionFile(file_manager->getAsset("immersive_relay/scintilla/snowtuxpeak_mission_v1.json"),65536,&bytes)||
       !readMissionFile(track->getTrackFile("quads.xml"),2*1024*1024,&quads)||
       !readMissionFile(track->getTrackFile("graph.xml"),2*1024*1024,&edges))return fail("mission_anchor_asset_unavailable");
    Anchors::Manifest candidate;std::string error;
    if(!Anchors::load(bytes,&candidate,&error))return fail(error);
    Anchors::NativeTrack native;native.trackId=RaceManager::get()->getTrackName();
    native.trackHash=missionSHA256(quads+edges);native.reverse=graph->isReverse();native.lapLength=graph->getLapLength();
    for(unsigned i=0;i<graph->getNumNodes();++i)
        native.nodeFractions.push_back(graph->getDistanceFromStart(i)/native.lapLength);
    if(!Anchors::validateNative(candidate,native,&error))return fail(error);
    const int laps=RaceManager::get()->getNumLaps();
    const unsigned expectedCrew=measurementLoading?missionCrewCount():unsigned(lastRoster.size());
    if(laps<1||laps>12||expectedCrew<1||expectedCrew>5)return fail("mission_anchor_session_limits");
    Anchors::Traversal traversal;
    if(!traversal.prepare(candidate,unsigned(laps),expectedCrew))return fail(traversal.reason());
    missionAnchors=candidate;missionTraversal=traversal;anchorRace=raceEpoch;
    anchorCrew=expectedCrew;
    anchorHash=missionSHA256(bytes);anchorStatus=candidate.authoringStatus;
    missionPlacements.clear();missionPositionKnown=false;missionPreviousAnimated=false;missionProgressKnown=false;
    if(!measurementLoading)refreshMissionQualification(reason);else if(reason)reason->clear();return true;
#else
    return fail("mission_renderer_unavailable");
#endif
}
namespace {
std::string missionIntroPath;
unsigned missionCrewCount()
{
    const auto s=RelaySessionController::get()->getSnapshot();
    return s.m_player_count>=2&&s.m_player_count<=6?unsigned(s.m_player_count-1):0;
}
MissionProtocol::Identity missionIdentity()
{
    MissionProtocol::Identity i;i.producerUUID=producerID;i.raceEpoch=std::to_string(raceEpoch);
    i.intentRevision=std::to_string(mission.intentRevision());i.missionRevision="1";
    i.rosterRevision=std::to_string(rosterEpoch);i.inputSessionID=consumer.inputSessionID();
    i.inputEpoch=std::to_string(consumer.inputEpoch());i.routeRevision=consumer.activeRouteRevision();
    i.projectionContextId=consumer.activeProjectionContextId();i.anchorManifestHash=anchorHash;i.introManifestHash=missionIntroHash;
    if(crew.required()){i.layoutProfile="front_rear_crew_v3";i.layoutRevision="3";}return i;
}
std::string introReceiptKey()
{return producerID+"|"+std::to_string(raceEpoch)+"|"+(crew.required()?crew.profileId():"sc4_cargo_three_slots_v1")+"|"+missionIntroHash+"|"+MissionProtocol::variantForCrew(missionCrewCount());}
bool missionIntroComplete()
{return missionIntroAvailable&&missionIntroVariant==MissionProtocol::variantForCrew(missionCrewCount())&&missionIntroReceipt.matches(introReceiptKey());}
bool loadMissionIntro(std::string* reason)
{
    auto fail=[&](const std::string& why){missionIntroAvailable=false;missionIntroError=why;missionIntroPath.clear();if(reason)*reason=why;return false;};
    if(!RelaySessionController::get()->isWorldHeld())return fail("mission_intro_load_requires_hold");
    const unsigned crewCount=missionCrewCount();if(!crewCount)return fail("mission_crew_count_invalid");
    const std::string selectedProfile=crew.required()?crew.profileId():"sc4_cargo_three_slots_v1";
    const std::string folder=crew.required()?(crew.partyEnabled()?"intro_crew_party_it_v1":"intro_crew_standard_it_v1"):"intro_it_v1";
    const std::string manifestPath=file_manager->getAsset("immersive_relay/scintilla/"+folder+"/manifest.json");
    std::string bytes;if(!readMissionFile(manifestPath,65536,&bytes))return fail("mission_intro_asset_unavailable");
    try{
        const auto manifest=MissionProtocol::introManifest(bytes,selectedProfile);
        const auto hash=missionSHA256(bytes);
        if(!missionIntroHash.empty()&&hash!=missionIntroHash&&mission.configured()&&missionIntroProfile==selectedProfile)return fail("mission_intro_manifest_changed");
        if(missionIntroProfile!=selectedProfile){missionIntroReceipt=MissionProtocol::IntroReceipt();missionAPIPrepared=false;}
        missionIntroProfile=selectedProfile;
        missionIntroHash=hash;missionIntroVariant=MissionProtocol::variantForCrew(crewCount);
        const auto& variant=manifest.variants[crewCount==1?0:crewCount==2?1:2];
        const auto slash=manifestPath.find_last_of("/\\");if(slash==std::string::npos)return fail("mission_intro_path_invalid");
        const auto root=manifestPath.substr(0,slash+1);std::string video,subtitles;
        if(!readMissionFile(root+variant.videoPath,128*1024*1024,&video)||missionSHA256(video)!=variant.videoHash||
           !readMissionFile(root+variant.subtitlesPath,256*1024,&subtitles)||missionSHA256(subtitles)!=variant.subtitlesHash)
            return fail("mission_intro_media_invalid");
        missionIntroAvailable=true;missionIntroError.clear();missionIntroPath=manifestPath;
        if(reason)reason->clear();return true;
    }catch(const std::exception&){return fail("mission_intro_manifest_invalid");}
}
std::string missionNativeProfileReason(bool requireCrew)
{
    const auto s=RelaySessionController::get()->getSnapshot();auto* world=World::getWorld();auto* rm=RaceManager::get();
    if(!s.m_runtime_enabled||!world||world->getIdent()!="IMMERSIVE_RELAY"||!localCombatSupported())return "mission_requires_local_world";
    if(s.m_player_count<2||s.m_player_count>6||s.m_crew_mode!="disabled"||s.m_test_accelerated_progress||
       s.m_adaptive_difficulty_config.m_enabled||s.m_adaptive_difficulty.m_enabled||s.m_track_id!="snowtuxpeak"||s.m_kart_id!="tux"||
       s.m_ai_karts!=3||s.m_difficulty!="easy"||s.m_total_laps<1||s.m_total_laps>12)return "mission_native_profile_mismatch";
    if(rm->getTrackName()!="snowtuxpeak"||rm->getReverseTrack()||rm->getNumPlayers()!=1||
       world->getNumKarts()!=4||rm->getDifficulty()!=RaceManager::DIFFICULTY_EASY||
       rm->getNumLaps()!=s.m_total_laps)return "mission_native_race_mismatch";
    const auto* human=world->getPlayerKart(0);if(!human||human->getIdent()!="tux"||human->isGhostKart()||human->isEliminated())return "mission_human_profile_mismatch";
    if(requireCrew&&(lastRoster.size()!=unsigned(s.m_player_count-1)||consumer.participants()!=lastRoster))return "mission_crew_roster_mismatch";
    return "";
}
#include "modes/immersive_relay/spatial/scintilla_measurement_native.hpp"

bool missionPreparationBound()
{
#ifndef SERVER_ONLY
    if(!missionResultAtlas||missionAssetsInvalidated)return false;
#endif
    if(qualificationFingerprint.empty()||measurementFingerprint(ImmersiveRenderPipeline::get()->getConfig())!=qualificationFingerprint)return false;
    if(!missionAPIPrepared||!mission.configured()||missionTerminal()||!mission.required()||!active||
       !consumer.prepared()||!combat.configured()||!defenseBound()||anchorRace!=raceEpoch||
       anchorStatus!="qualified"||!missionIntroComplete()||!missionNativeProfileReason(true).empty())return false;
    const auto time=nowUs();if(!consumer.defenseVideoReady(time,true))return false;
    const auto presentation=consumer.presentedDefenseFrames(time,true);
    if(std::none_of(presentation.begin(),presentation.end(),[](const PresentedFrame& p){return p.frame.missionRaceEpoch==raceEpoch&&p.frame.layout.mission;}))return false;
    const auto now=missionIdentity();
    return missionPreparedIdentity==now&&mission.race()==raceEpoch&&mission.inputEpoch()==consumer.inputEpoch()&&
        mission.crew()==lastRoster&&mission.humanInstance()==humanInstance&&mission.profile().fps==stk_config->getPhysicsFPS();
}
std::string missionSuspendedReason()
{
    if(!mission.required())return "";
    if(missionTerminal())return "mission_finished";
    if(!missionStepBlockedReason().empty())return missionStepBlockedReason();
    const auto profile=missionNativeProfileReason(false);if(!profile.empty())return profile;
    if(!missionIntroAvailable)return missionIntroError;
    if(!missionIntroComplete())return "mission_intro_required";
    if(anchorStatus!="qualified")return "mission_anchors_unqualified";
    if(!missionPreparationBound())return "mission_not_prepared";
    return RelaySessionController::get()->getSpatialGameplayState().m_active?"":"world_held";
}
void writeMissionIdentity(std::ostream& out,const MissionProtocol::Identity& i)
{
    using ScintillaJSON::quote;
    out<<"\"producerUUID\":"<<quote(i.producerUUID)<<",\"raceEpoch\":"<<quote(i.raceEpoch)
       <<",\"intentRevision\":"<<quote(i.intentRevision)<<",\"missionRevision\":"<<quote(i.missionRevision)
       <<",\"rosterRevision\":"<<quote(i.rosterRevision)<<",\"inputSessionID\":"<<quote(i.inputSessionID)
       <<",\"inputEpoch\":"<<quote(i.inputEpoch)<<",\"routeRevision\":"<<quote(i.routeRevision)
       <<",\"projectionContextId\":"<<quote(i.projectionContextId)<<",\"anchorManifestHash\":"<<quote(i.anchorManifestHash)
       <<",\"introManifestHash\":"<<quote(i.introManifestHash)<<",\"layoutProfile\":"<<quote(i.layoutProfile)
       <<",\"layoutRevision\":"<<quote(i.layoutRevision);
}
ScintillaSpatial::Reply missionError(int status,const std::string& reason)
{return Reply(status,"{\"ok\":false,\"error\":"+ScintillaJSON::quote(reason)+"}");}
ScintillaSpatial::Reply missionRequestGate(const MissionProtocol::Request& request,bool* duplicate)
{
    *duplicate=false;const auto match=missionRequests.check(request);
    if(match==MissionProtocol::Receipts::Conflict)return missionError(409,"mission_request_id_conflict");
    if(match==MissionProtocol::Receipts::Duplicate){*duplicate=true;return Reply(200,"");}
    if(!(request.expected==missionIdentity()))return missionError(409,"mission_identity_changed");
    return Reply(200,"");
}
}
bool missionMeasurementActive(){return measurement.started&&!measurement.closed;}
std::string missionMeasurementStateJSON()
{
    std::ostringstream o;o<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-measurement/1\",\"producerUUID\":"<<ScintillaJSON::quote(producerID)
      <<",\"raceEpoch\":\""<<raceEpoch<<"\",\"recordingID\":"<<ScintillaJSON::quote(measurement.id)<<",\"recording\":"<<(missionMeasurementActive()?"true":"false")
      <<",\"closed\":"<<(measurement.closed?"true":"false")<<",\"phase\":"<<ScintillaJSON::quote(measurement.phase)<<",\"reason\":"<<ScintillaJSON::quote(measurement.reason)
      <<",\"frameCount\":\""<<measurement.frames<<"\",\"byteCount\":\""<<measurement.trace.size()<<"\",\"sha256\":"<<ScintillaJSON::quote(measurement.hash)<<'}';return o.str();
}
Reply missionInputMaintenanceCommand(const std::string& operation,const std::string& body)
{
    try{
        using namespace ScintillaJSON;
        if(body.size()>8192)return missionError(400,"input_maintenance_request_size");
        const J j=Parser(body).parse();
        if(text(j,"producerUUID")!=producerID||MissionProtocol::decimal(at(j,"raceEpoch"))!=std::to_string(raceEpoch))return missionError(409,"input_maintenance_identity_changed");
        auto* controller=RelaySessionController::get();auto* world=World::getWorld();
        if(!controller->isWorldHeld()||!world||world->getTimeTicks()!=0)return missionError(409,"input_maintenance_requires_prerace_held_world");
        const auto profile=missionNativeProfileReason(false);if(!profile.empty())return missionError(409,profile);
        if(!active||missionMeasurementActive())return missionError(409,"input_maintenance_unavailable");
        const auto config=ImmersiveRenderPipeline::get()->getConfig();
        Projection projection;projection.kind=projectionKindToString(config.m_projection_mode);
        projection.width=config.m_projection_width;projection.height=config.m_projection_height;projection.domeTiltDeg=config.m_dome_tilt_deg;
        consumer.setProducerProjection(projection);
        const auto before=consumer.inputEpoch();
        const auto reply=consumer.inputMaintenance(operation,body,nowUs());
        if(reply.status==200){
            controller->setScintillaStartPermitted(false);
            if(operation=="begin"&&before!=consumer.inputEpoch()){
                combat.suspendInput();defense.suspend("input_maintenance");mission.suspendInput();
                defensePrepared=false;missionAPIPrepared=false;missionStepOpen=false;
                clearDefenseEvidence();clearMissionPresentationQueries();
            }
        }
        return reply;
    }catch(const std::exception&){return missionError(400,"input_maintenance_invalid_request");}
}
Reply missionMeasurementCommand(const std::string& operation,const std::string& body)
{
    try{using namespace ScintillaJSON;if(body.size()>4096)return missionError(400,"measurement_request_size");const J j=Parser(body).parse();
        if(operation=="start")closed(j,{"contract","schemaVersion","requestId","producerUUID","raceEpoch","phase"},{"crewProfile"});
        else if(operation=="preflight")closed(j,{"contract","schemaVersion","requestId","producerUUID","raceEpoch"},{"inputMaintenanceOperationID"});
        else if(operation=="stop")closed(j,{"contract","schemaVersion","requestId","producerUUID","raceEpoch"});else return missionError(404,"measurement_unknown_operation");
        if(text(j,"contract")!="tholus.stk-scintilla-measurement/1"||num(j,"schemaVersion")!=1||!MissionProtocol::uuid(text(j,"requestId")))return missionError(400,"measurement_request_contract");
        if(text(j,"producerUUID")!=producerID||MissionProtocol::decimal(at(j,"raceEpoch"))!=std::to_string(raceEpoch))return missionError(409,"measurement_identity_changed");
        if(!RelaySessionController::get()->isWorldHeld())return missionError(409,"measurement_requires_held_world");
        if(operation=="preflight"){
            auto* world=World::getWorld();if(!world||world->getTimeTicks()!=0)return missionError(409,"measurement_preflight_requires_prerace_world");
            const bool ticket=has(j,"inputMaintenanceOperationID");
            if(ticket&&(!MissionProtocol::uuid(text(j,"inputMaintenanceOperationID"))||!consumer.inputMaintenanceMatches(text(j,"inputMaintenanceOperationID"))))return missionError(409,"measurement_input_maintenance_mismatch");
            if(missionMeasurementActive()||(consumer.hasPreparedGrant()&&!ticket))return missionError(409,"measurement_preflight_requires_no_consumer_grant");
            const auto profile=missionNativeProfileReason(false);if(!profile.empty())return missionError(409,profile);
            std::string reason;measurementLoading=true;const bool anchors=prepareMissionAnchors(&reason);measurementLoading=false;
            if(crew.required())prepareCrewPartyZones(&reason);
            if(!anchors||!loadMeasurementFingerprints(&reason))return missionError(409,reason);
            const bool qualified=loadMissionQualification(&reason);
            return Reply(200,"{\"ok\":true,\"qualified\":"+std::string(qualified?"true":"false")+",\"reason\":"+ScintillaJSON::quote(reason)+"}");
        }
        if(operation=="stop"){
            if(!measurement.started)return missionError(409,"measurement_not_started");measurement.stopRequested=true;
            auto* world=dynamic_cast<LinearWorld*>(World::getWorld());auto* human=defenseHuman();
            if(world&&human)measurement.finishLap=world->getLapForKart(human->getWorldKartId());
            pollMissionMeasurement();if(!measurement.closed)return missionError(409,"measurement_gpu_pending");
            const bool previous=measurement.wasEnabled;if(!previous&&active)setEnabled(false);return Reply(200,missionMeasurementStateJSON());
        }
        const auto phase=text(j,"phase");if(phase!="shell"&&phase!="open")return missionError(400,"measurement_phase");
        const std::string diagnosticProfile=has(j,"crewProfile")?text(j,"crewProfile"):"";
        if(has(j,"crewProfile")&&diagnosticProfile!="sc5_crew_standard_v1"&&diagnosticProfile!="sc5_crew_party_v1")return missionError(400,"measurement_crew_profile");
        if(measurement.startRequest==text(j,"requestId")){
            if(measurement.race!=raceEpoch||measurement.phase!=phase||measurement.crewProfile!=diagnosticProfile)return missionError(409,"measurement_request_id_conflict");return Reply(200,missionMeasurementStateJSON());}
        if(missionMeasurementActive())return missionError(409,"measurement_already_recording");
        if(mission.required()||combat.required()||defense.required()||mission.configured()||combat.configured()||defense.configured()||consumer.prepared())return missionError(409,"measurement_requires_unprepared_diagnostic_world");
        const auto profile=missionNativeProfileReason(false);if(!profile.empty())return missionError(409,profile);
#ifndef SERVER_ONLY
        std::string reason;measurementLoading=true;const bool anchors=prepareMissionAnchors(&reason);measurementLoading=false;
        if(!diagnosticProfile.empty())prepareCrewPartyZones(&reason);
        if(!anchors||!loadMeasurementFingerprints(&reason))return missionError(409,reason);
        const auto config=ImmersiveRenderPipeline::get()->getConfig();
        if(config.m_projection_mode!=IMMERSIVE_PROJECTION_EQUIRECTANGULAR&&config.m_projection_mode!=IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER)return missionError(409,"measurement_projection_unavailable");
        if(config.m_debug_grid||config.m_camera_mode!=IMMERSIVE_CAMERA_STABILIZED_CHASE||!config.m_horizon_stabilization)return missionError(409,"measurement_requires_operational_camera");
        const bool previous=active;measurement=MeasurementRecorder();measurement.wasEnabled=previous;measurement.phase=phase;measurement.crewProfile=diagnosticProfile;measurement.race=raceEpoch;measurement.id=makeProducerUUID();measurement.startRequest=text(j,"requestId");measurement.started=true;
        measurement.fingerprint=measurementFingerprint(config);if(measurement.fingerprint.empty()){measurement=MeasurementRecorder();return missionError(409,"measurement_profile_unavailable");}
        std::ostringstream h;h<<"{\"type\":\"header\",\"contract\":"<<quote(effectiveCrewLayout()?"tholus.stk-scintilla-measurement/2":"tholus.stk-scintilla-measurement/1")<<",\"schemaVersion\":"<<(effectiveCrewLayout()?2:1)<<",\"recordingID\":"<<quote(measurement.id)<<",\"producerUUID\":"<<quote(producerID)
          <<",\"raceEpoch\":\""<<raceEpoch<<"\",\"provenance\":\"native_render_depth\",\"fingerprint\":"<<measurement.fingerprint<<",\"fingerprintJSON\":"<<quote(measurement.fingerprint)
          <<",\"fingerprintHash\":"<<quote(missionSHA256(measurement.fingerprint))<<",\"groups\":"<<measurementGroupsJSON()<<",\"limits\":{\"maxFrames\":18000,\"maxPending\":8,\"maxGapUs\":50000,\"liveProofMaxAgeUs\":"<<Measure::LiveProofMaxAgeUs<<"},\"phasePolicy\":\"diagnostic_constant_per_trace\",\"phase\":"<<quote(phase)<<",\"actualLayoutRevision\":"<<(effectiveCrewLayout()?3:0)<<"}\n";
        measurement.trace=h.str();setEnabled(true);return Reply(200,missionMeasurementStateJSON());
#else
        return missionError(409,"measurement_renderer_unavailable");
#endif
    }catch(const std::exception&){measurementLoading=false;return missionError(400,"measurement_invalid_request");}
}
Reply missionMeasurementTrace(const std::string& offset)
{
    if(!RelaySessionController::get()->isWorldHeld()||!measurement.closed)return missionError(409,"measurement_export_requires_closed_held_world");
    try{ScintillaJSON::J value;value.kind=ScintillaJSON::J::String;value.string=offset;const uint64_t from=Measure::wide(value);
        if(from>measurement.trace.size()||(from<measurement.trace.size()&&(static_cast<unsigned char>(measurement.trace[from])&192)==128))return missionError(400,"measurement_export_offset");
        size_t end=std::min<std::size_t>(measurement.trace.size(),from+32768);
        while(end<measurement.trace.size()&&end>from&&(static_cast<unsigned char>(measurement.trace[end])&192)==128)--end;
        ScintillaJSON::J text;text.kind=ScintillaJSON::J::String;text.string=measurement.trace.substr(from,end-from);
        std::ostringstream o;o<<"{\"ok\":true,\"recordingID\":"<<ScintillaJSON::quote(measurement.id)<<",\"offset\":\""<<from<<"\",\"nextOffset\":\""<<end<<"\",\"eof\":"<<(end==measurement.trace.size()?"true":"false")<<",\"data\":"<<ScintillaJSON::serialize(text)<<'}';return Reply(200,o.str());
    }catch(const std::exception&){return missionError(400,"measurement_export_offset");}
}
void observeMissionMeasurementNaturalFinish()
{
#ifndef SERVER_ONLY
    if(!missionMeasurementActive())return;auto* world=dynamic_cast<LinearWorld*>(World::getWorld());auto* human=defenseHuman();
    if(!world||!human||!human->hasFinishedRace()||world->getLapForKart(human->getWorldKartId())<RaceManager::get()->getNumLaps())return;
    measurement.natural=true;measurement.finishLap=world->getLapForKart(human->getWorldKartId());measurement.stopRequested=true;
#endif
}
void observeMissionMeasurementPhysics()
{
#ifndef SERVER_ONLY
    if(!missionMeasurementActive()&&!(mission.required()&&anchorStatus=="qualified"))return;
    auto* world=dynamic_cast<LinearWorld*>(World::getWorld());auto* human=defenseHuman();auto* graph=DriveGraph::get();if(!world||!human||!graph||graph->getLapLength()<=0)return;
    const int lap=world->getLapForKart(human->getWorldKartId());const double fraction=world->getDistanceDownTrackForKart(human->getWorldKartId(),true)/graph->getLapLength();
    const auto& p=human->getXYZ();const SpatialVec3 position(p.x(),p.y(),p.z());const SpatialVec3 delta(position.x-measurement.previousPosition.x,position.y-measurement.previousPosition.y,position.z-measurement.previousPosition.z);
    const bool animated=human->getKartAnimation()!=nullptr;const double bound=2+std::max(std::abs(double(human->getSpeed())),std::abs(double(human->getCurrentMaxSpeed())))/std::max(1,stk_config->getPhysicsFPS());
    const bool continuous=measurement.previousKnown&&!measurement.previousAnimated&&!animated&&dot(delta,delta)<=bound*bound&&lap>=0&&measurement.previousLap>=0&&
        lap+fraction>=measurement.previousLap+measurement.previousFraction&&std::abs(lap+fraction-measurement.previousLap-measurement.previousFraction)*graph->getLapLength()<=bound*4;
    if(!continuous){measurement.continuityBroken=true;measurement.activation.fill(0);}
    if(world->isRacePhase()&&!RelaySessionController::get()->isWorldHeld()&&measurement.inactive){measurement.inactive=false;measurement.continuityBroken=true;measurement.nextSampleUs=0;}
    if(world->isRacePhase()&&!RelaySessionController::get()->isWorldHeld())measurement.intervalMinimum=std::min(measurement.intervalMinimum,std::abs(double(human->getSpeed())));
    if(continuous)for(unsigned g=0;g<missionAnchors.groups.size();++g){const auto& group=missionAnchors.groups[g];for(unsigned l=0;l<12;++l){
        if(std::find(group.laps.begin(),group.laps.end(),l)==group.laps.end())continue;const double crossing=l+group.activationFraction;
        if(measurement.previousLap+measurement.previousFraction<crossing&&crossing<=lap+fraction)
            for(unsigned c=0;c<group.capsules.size();++c)measurement.activation[l*24+g*2+c]=nowUs();}}
    measurement.previousPosition=position;measurement.previousKnown=true;measurement.previousAnimated=animated;measurement.previousLap=lap;measurement.previousFraction=fraction;
#endif
}

const std::string& producerUUID(){return producerID;}
std::string missionStateJSON()
{
    using ScintillaJSON::quote;std::ostringstream out;out<<std::setprecision(17);
    const auto result=missionTerminalRecord.valid?missionTerminalRecord:RelaySessionController::get()->getMissionResult();
    const auto slots=result.valid?result.slots:mission.slots();const auto identity=missionIdentity();
    // Publish only qualification for the CURRENT cheap configuration fingerprint.
    // GET remains observational: no asset scan, trace validation or cache mutation.
    bool anchorQualified=false;
#ifndef SERVER_ONLY
    anchorQualified=anchorStatus=="qualified"&&!qualificationFingerprint.empty()&&
        measurementFingerprint(ImmersiveRenderPipeline::get()->getConfig())==qualificationFingerprint;
#endif
    const std::string publishedAnchorStatus=anchorStatus=="qualified"&&!anchorQualified?"preflight_required":anchorStatus;
    out<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-mission/1\",\"schemaVersion\":1,\"profileId\":\"sc4_cargo_three_slots_v1\",\"profileRevision\":\"1\",";
    writeMissionIdentity(out,identity);
    out<<",\"stateVersion\":\""<<mission.stateVersion()+missionAPIRevision<<"\",\"required\":"<<(mission.required()?"true":"false")
       <<",\"prepared\":"<<(missionPreparationBound()?"true":"false")<<",\"active\":"<<(missionPreparationBound()&&mission.active()&&RelaySessionController::get()->getSpatialGameplayState().m_active?"true":"false")
       <<",\"suspendedReason\":"<<quote(missionSuspendedReason())<<",\"anchorQualified\":"<<(anchorQualified?"true":"false")<<",\"anchorStatus\":"<<quote(publishedAnchorStatus)
       <<",\"simulationTick\":\""<<mission.tick()<<"\",\"securedCount\":"<<(result.valid?result.securedCount:mission.securedCount())<<",\"slots\":[";
    for(unsigned i=0;i<3;++i){if(i)out<<',';const auto& slot=slots[i];out<<"{\"state\":"<<quote(slot.state==ScintillaMission::SlotState::Secured?"secured":slot.state==ScintillaMission::SlotState::Tethered?"tethered":"empty")<<",\"moduleId\":"<<(slot.module?quote(std::to_string(slot.module)):"null")<<",\"everAcquired\":"<<(slot.everAcquired?"true":"false")<<",\"beaconLit\":"<<(slot.beaconLit?"true":"false")<<'}';}
    out<<"],\"beacons\":[";for(unsigned i=0;i<3;++i){if(i)out<<',';out<<(slots[i].beaconLit?"true":"false");}
    out<<"],\"tether\":";const auto tether=mission.snapshot(ScintillaMission::Runtime::TetherEntityId);
    if(!tether.id||result.valid)out<<"null";
    else {out<<"{\"entityId\":\""<<tether.id<<"\",\"instance\":\""<<tether.instance<<"\",\"moduleId\":\""<<tether.module<<"\",\"phaseRevision\":\""<<tether.phaseRevision<<"\",\"phase\":"<<quote(tether.phase==ScintillaMission::Phase::TetherAwaitingPresentation?"awaiting_presentation":"recoverable")<<",\"deadlineTick\":"<<(tether.phase==ScintillaMission::Phase::TetherAwaitingPresentation?"null":quote(std::to_string(tether.deadline)))<<",\"slot\":"<<tether.slot<<",\"owner\":"<<tether.owner<<",\"progress\":"<<tether.progress<<'}';}
    out<<",\"feedback\":[";bool first=true;for(const auto& f:mission.feedback()){
        if(!first)out<<',';first=false;const auto* status=ScintillaMission::Runtime::statusReason(f.status);const auto* outcome=ScintillaMission::Runtime::statusReason(f.outcome);
        out<<"{\"participantId\":"<<quote(f.participant)<<",\"controlEpoch\":\""<<f.controlEpoch<<"\",\"entityId\":"<<(f.entity?quote(std::to_string(f.entity)):"null")<<",\"owner\":"<<f.owner<<",\"status\":"<<quote(*status?status:"none")<<",\"outcome\":"<<quote(*outcome?outcome:"none")<<'}';
    }
    const auto crew=missionCrewCount();const auto variant=MissionProtocol::variantForCrew(crew);
    out<<"],\"intro\":{\"available\":"<<(missionIntroAvailable&&missionIntroVariant==variant?"true":"false")<<",\"manifestHash\":"<<quote(missionIntroHash)<<",\"variantId\":"<<quote(variant)<<",\"completed\":"<<(missionIntroComplete()?"true":"false")<<",\"completion\":"<<quote(missionIntroComplete()?missionIntroReceipt.completion:"pending")<<",\"playbackID\":"<<(missionIntroComplete()?quote(missionIntroReceipt.playbackID):"null")
       <<",\"manifestRelativePath\":"<<quote(missionIntroAvailable?"immersive_relay/scintilla/intro_it_v1/manifest.json":"")<<",\"manifestPath\":"<<quote(missionIntroAvailable?missionIntroPath:"")<<",\"locale\":\"it\",\"durationSeconds\":40,\"frameRateNumerator\":30,\"frameRateDenominator\":1,\"frameCount\":1200},\"result\":";
    ScintillaMission::writeResultJSON(out,result);out<<'}';return out.str();
}
Reply setMissionIntent(const std::string& body)
{
    if(missionMeasurementActive())return missionError(409,"mission_measurement_active");
    try{
        const auto request=MissionProtocol::request(body,"intent");refreshRoster();bool duplicate=false;
        if(crew.required()&&!request.required)return missionError(409,"crew_profile_locked");
        const auto gate=missionRequestGate(request,&duplicate);if(gate.status!=200)return gate;if(duplicate)return Reply(200,missionStateJSON());
        auto* world=World::getWorld();auto* controller=RelaySessionController::get();
        if(!world||!controller->isWorldHeld()||(world->getTimeTicks()>0&&request.required!=mission.required())||missionTerminal())return missionError(409,"mission_intent_requires_pre_race_hold");
        if(request.required){const auto profile=missionNativeProfileReason(false);if(!profile.empty())return missionError(409,profile);}
        // Preflight copies make all three policy changes one game-thread commit.
        auto nextMission=mission;auto nextCombat=combat;auto nextDefense=defense;
        if(!nextMission.setRequired(request.required)||(request.required&&
           ((!nextCombat.required()&&!nextCombat.setRequired(true))||(!nextDefense.required()&&!nextDefense.setRequired(true)))))return missionError(409,"mission_intent_requires_race_reset");
        controller->setScintillaStartPermitted(false);
        mission=std::move(nextMission);combat=std::move(nextCombat);defense=std::move(nextDefense);
        if(request.required){std::string reason;prepareMissionPresentationAssets(&reason);loadMissionIntro(&reason);prepareMissionAnchors(&reason);}
        ++missionAPIRevision;missionRequests.commit(request);refreshCombatInput();return Reply(200,missionStateJSON());
    }catch(const std::exception&){return missionError(400,"invalid_mission_intent");}
}
Reply completeMissionIntro(const std::string& body)
{
    try{
        const auto request=MissionProtocol::request(body,"intro-complete");refreshRoster();bool duplicate=false;
        const auto gate=missionRequestGate(request,&duplicate);if(gate.status!=200)return gate;if(duplicate)return Reply(200,missionStateJSON());
        auto* world=World::getWorld();if(!mission.required()||!world||!RelaySessionController::get()->isWorldHeld()||world->getTimeTicks()>0||missionTerminal())return missionError(409,"mission_intro_requires_pre_race_hold");
        if(!missionIntroAvailable||request.variantId!=MissionProtocol::variantForCrew(missionCrewCount())||request.variantId!=missionIntroVariant||missionIntroHash.empty())return missionError(409,"mission_intro_not_available");
        if(!missionIntroReceipt.accept(introReceiptKey(),request.completion,request.playbackID))return missionError(409,"mission_intro_receipt_conflict");
        ++missionAPIRevision;missionRequests.commit(request);refreshCombatInput();return Reply(200,missionStateJSON());
    }catch(const std::exception&){return missionError(400,"invalid_mission_intro_receipt");}
}
Reply prepareMission(const std::string& body)
{
    try{
        const auto request=MissionProtocol::request(body,"prepare");refreshRoster();bool duplicate=false;
        const auto gate=missionRequestGate(request,&duplicate);if(gate.status!=200)return gate;if(duplicate)return Reply(200,missionStateJSON());
        auto* controller=RelaySessionController::get();if(!controller->isWorldHeld())return missionError(409,"mission_prepare_requires_hold");
        std::string reason;if(!prepareMissionPresentationAssets(&reason))return missionError(409,reason);
        // A terminal retry may restore UI resources only. Never reopen the core.
        if(missionTerminal()){missionRequests.commit(request);return Reply(200,missionStateJSON());}
        if(!mission.required()||!combat.required()||!defense.required())return missionError(409,"mission_intent_required");
        reason=missionNativeProfileReason(true);if(!reason.empty())return missionError(409,reason);
        if(!missionIntroComplete())return missionError(409,"mission_intro_required");
        if(!active||!consumer.prepared()||!combat.configured()||combat.inputEpoch()!=consumer.inputEpoch()||combat.crew()!=lastRoster||!defenseBound())return missionError(409,"mission_requires_prepared_sc1_sc2_sc3");
        const auto time=nowUs();if(!consumer.defenseVideoReady(time,true))return missionError(409,"mission_fresh_presentation_required");
        const auto presentation=consumer.presentedDefenseFrames(time,true);
        if(std::none_of(presentation.begin(),presentation.end(),[](const PresentedFrame& p){return p.frame.missionRaceEpoch==raceEpoch&&p.frame.layout.mission;}))return missionError(409,"mission_fresh_presentation_required");
        if(!prepareMissionAnchors(&reason))return missionError(409,reason);
        if(request.expected.anchorManifestHash!=anchorHash)return missionError(409,"mission_identity_changed");
        if(anchorStatus!="qualified")return missionError(409,"mission_anchors_unqualified");
        auto* human=defenseHuman();if(!human||!humanInstance)return missionError(409,"mission_human_unavailable");
        if(!mission.configured()&&World::getWorld()->getTimeTicks()>0)return missionError(409,"mission_prepare_requires_race_reset");
        if(!tuningError.empty())return missionError(409,"tuning_invalid");
        if(!mission.prepare(raceEpoch,1,consumer.inputEpoch(),humanInstance,lastRoster,stk_config->getPhysicsFPS(),tuning.mission))return missionError(409,"mission_profile_conflict");
        missionPreparedIdentity=missionIdentity();missionAPIPrepared=true;++missionAPIRevision;
        missionRequests.commit(request);refreshCombatInput();return Reply(200,missionStateJSON());
    }catch(const std::exception&){return missionError(400,"invalid_mission_prepare");}
}
bool missionRequired(){return mission.required();}
bool missionTerminal()
{
    return missionTerminalRecord.valid || mission.result().valid ||
           RelaySessionController::get()->getMissionResult().valid;
}
bool missionStartPermitted()
{
    if(consumer.inputMaintenanceActive())return false;
    if(!mission.required()) return true;
#ifndef SERVER_ONLY
    if(!missionResultAtlas||missionAssetsInvalidated)return false;
#else
    return false;
#endif
    return missionPreparationBound()&&active&&mission.configured()&&!missionTerminal()&&consumer.prepared()&&
        localCombatSupported()&&mission.race()==raceEpoch&&
        mission.inputEpoch()==consumer.inputEpoch()&&mission.crew()==lastRoster&&
        mission.profile().fps==stk_config->getPhysicsFPS()&&missionStepFailure.empty()&&
        !RelaySessionController::get()->getSnapshot().m_mission_presentation_held;
}
int64_t missionSimulationTickBase(){return missionClockBase;}
const std::string& missionStepBlockedReason()
{
    static const std::string presentation="mission_presentation_unavailable";
    static const std::string assets="mission_presentation_assets_invalidated";
    if(!missionStepFailure.empty())return missionStepFailure;
#ifndef SERVER_ONLY
    if(missionAssetsInvalidated)return assets;
#endif
    return tetherPresentationHold?presentation:missionStepFailure;
}
void publishMissionSnapshot()
{
    const auto observation=missionObservation();Crew::Observation crewObservation;
    if(crewTerminalRecord.valid){crewObservation=crewTerminalObservation;crewObservation.snapshot=crewTerminalRecord;}
    else {
        synchronizeCrewTurn();
        crewObservation=Crew::observeResult(crew,producerID,observation.raceEpoch,observation.missionRevision,
            observation.finishTick,RelaySessionController::get()->getSnapshot().m_test_accelerated_progress,crewPartyZoneManifestHash());
    }
    RelaySessionController::get()->observeMission(mission.required(),observation,crewObservation);
}
namespace {
Crew::Result frozenCrewResult()
{
    const auto* controller=RelaySessionController::get();const auto result=controller->getCrewResult();
    if(result.raceEpoch==raceEpoch&&Crew::matchesMissionResult(result,controller->getMissionResult()))return result;
    return Crew::matchesMissionResult(crewTerminalRecord,missionObservation())?crewTerminalRecord:Crew::Result();
}
}
void setMissionPresentationHeld(bool held)
{
    RelaySessionController::get()->setMissionPresentationHeld(held);
    if(held)
    {
        mission.suspendInput();missionStepOpen=false;
        const auto gameplay=RelaySessionController::get()->getSpatialGameplayState();
        consumer.tick(nowUs(),false,gameplay.m_resume_revision,gameplay.m_resume_at_us);
    }
    publishMissionSnapshot();
}
bool beginMissionStep(int ticks)
{
    missionStepOpen=false;
    if(!mission.required()) return true;
    if(missionTerminal()) {closeMissionAdmission();return false;}
    defenseHuman();
    if(!missionPreparationBound()){
        mission.suspendInput();RelaySessionController::get()->setScintillaStartPermitted(false);return false;
    }
    if(!missionStepFailure.empty()){
        setMissionPresentationHeld(true);return false;
    }
#ifndef SERVER_ONLY
    if(missionAssetsInvalidated){mission.suspendInput();return false;}
#endif
    if(ticks<0 || ticks>1)
    {
        missionStepFailure="mission_unsupported_tick_step";
        setMissionPresentationHeld(true);return false;
    }
    World* world=World::getWorld();
    const bool gameplay=ticks==1 && world && world->isRacePhase() &&
        !world->isFinishPhase() && RelaySessionController::get()->getSpatialGameplayState().m_active;
    if(!mission.configured())
    {
        RelaySessionController::get()->setScintillaStartPermitted(false);return false;
    }
    if(!gameplay)
    {
        mission.advanceTo(mission.tick(),false);publishMissionSnapshot();return true;
    }
    if(!missionClockBound)
    {missionClockBase=simulationTick-mission.tick();missionClockBound=true;}
    if(simulationTick==std::numeric_limits<int64_t>::max() ||
       mission.tick()==std::numeric_limits<int64_t>::max() ||
       simulationTick-missionClockBase!=mission.tick() ||
       !mission.advanceTo(mission.tick()+1,true))
    {
        missionStepFailure="mission_clock_discontinuity";
        setMissionPresentationHeld(true);return false;
    }
    ++simulationTick;missionStepOpen=true;publishMissionSnapshot();return true;
}
ScintillaMission::Result freezeMissionNaturalFinish(unsigned position,double time)
{
    if(!mission.required() || missionTerminal()) return ScintillaMission::Result();
    const auto* human=defenseHuman();
    if(!missionStepOpen || !localCombatSupported() ||
       !human || mission.humanInstance()!=humanInstance ||
       !mission.freezeNaturalFinish(position,time,1))
    {
        terminateMission("native_finish_without_admitted_mission_step");
        return ScintillaMission::Result();
    }
    missionTerminalRecord=mission.result();
    synchronizeCrewTurn();
    crewTerminalObservation=Crew::observeResult(crew,producerID,missionTerminalRecord.raceEpoch,missionTerminalRecord.missionRevision,
        missionTerminalRecord.finishTick,RelaySessionController::get()->getSnapshot().m_test_accelerated_progress,crewPartyZoneManifestHash());
    crewTerminalRecord=Crew::freezeResult(crewTerminalObservation,missionTerminalRecord);
    closeMissionAdmission();publishMissionSnapshot();
    return missionTerminalRecord;
}
void terminateMission(const std::string& reason)
{
    if(!mission.required()) return;
    auto* controller=RelaySessionController::get();
    publishMissionSnapshot();controller->finishMissionArtificial(reason);
    const auto result=controller->getMissionResult();
    if(!result.valid) return;
    missionTerminalRecord=result;
    crewTerminalRecord=controller->getCrewResult();
    if(mission.configured() && !mission.result().valid)
        mission.abort(result.terminationReason,result.finishSequence);
    closeMissionAdmission();publishMissionSnapshot();
}
namespace {
// ---- Session telemetry: observations for balancing, never gameplay authority.
ScintillaTelemetry::Writer telemetryWriter;
uint64_t telemetryRace=0;
std::string telemetryError;
int64_t telemetryNextSample=0,telemetryNextPresentation=0;
// Longest main-loop interval since the last sample: stalls delay relay input
// and clock renewals as much as they delay frames.
int64_t telemetryLastTickUs=0,telemetryFrameMaxUs=0;
// Frames whose depth was not retained since the last sample, by reason. A
// proof naming such a frame is refused (depth_history_unavailable).
unsigned telemetryDepthPinned=0,telemetryDepthBudget=0,telemetryDepthInvalid=0;
std::map<uint64_t,ScintillaCombat::Snapshot> telemetryCombat;
std::map<uint64_t,ScintillaMission::Snapshot> telemetryMission;
std::map<std::string,BeamProof> telemetryBeams;
Defense::View telemetryDefense;
int64_t telemetryBoostUntil=0;
std::array<ScintillaMission::Slot,ScintillaMission::Profile::SlotCount> telemetrySlots;
bool telemetryResult=false,telemetryGameplay=false,telemetryFinished=false;
int telemetryLap=-1;
// The pilot kart was being rescued (after a crash or a fall) at the last tick.
bool telemetryRescue=false;
std::vector<std::string> telemetryRoster;
uint64_t telemetryPresentedFrame=0;
std::vector<double> telemetryPresentationMs;
struct FlashRecord {std::string source;uint64_t press=0,frame=0;int64_t pressedUs=0,poseUs=0,captureUs=0;};
std::deque<FlashRecord> telemetryFlashes;
std::map<std::string,uint64_t> telemetryFlashLogged;
const char* combatPhaseName(ScintillaCombat::Phase p)
{static const char* n[]={"shielded","acquiring","breached","stagger_pending","stagger","recovery"};return n[int(p)];}
const char* defensePhaseName(Defense::Phase p)
{static const char* n[]={"idle","await_warning","telegraph","launch_pending","flying","await_impact","cooldown"};return n[int(p)];}
const char* missionPhaseName(ScintillaMission::Phase p)
{static const char* n[]={"shell","open","collected","expired","tether_awaiting_presentation","tether_recoverable"};return n[int(p)];}
const char* slotName(ScintillaMission::SlotState s){return s==ScintillaMission::SlotState::Secured?"secured":s==ScintillaMission::SlotState::Tethered?"tethered":"empty";}
const char* targetKindName(TargetKind k)
{return k==TargetKind::Rival?"rival":k==TargetKind::MissionCapsule?"capsule":k==TargetKind::MissionTether?"tether":"diagnostic_pickup";}
bool telemetryEnabled()
{const char* v=std::getenv("STK_RELAY_TELEMETRY");return !(v&&(std::string(v)=="0"||std::string(v)=="off"));}
ScintillaTelemetry::Line telemetryLine(const char* type)
{ScintillaTelemetry::Line l(type);l.integer("t",nowUs()).integer("tick",simulationTick);return l;}
void telemetryWrite(const ScintillaTelemetry::Line& line){if(telemetryWriter.isOpen())telemetryWriter.write(line.str());}
std::string jsonStrings(const std::vector<std::string>& values)
{std::string out="[";for(size_t i=0;i<values.size();++i)out+=(i?",":"")+ScintillaTelemetry::escape(values[i]);return out+"]";}
void closeTelemetry(const std::string& reason)
{
    if(!telemetryWriter.isOpen())return;
    telemetryWrite(telemetryLine("close").text("reason",reason));
    telemetryWriter.close();consumer.setTelemetry(false);
    telemetryCombat.clear();telemetryMission.clear();telemetryBeams.clear();telemetryDefense=Defense::View();
    telemetryBoostUntil=0;telemetrySlots={};telemetryResult=telemetryGameplay=telemetryFinished=telemetryRescue=false;telemetryLap=-1;
    telemetryFlashes.clear();telemetryFlashLogged.clear();telemetryPresentationMs.clear();telemetryRoster.clear();
}
void openTelemetry()
{
    if(telemetryWriter.isOpen()&&telemetryRace==raceEpoch)return;
    closeTelemetry("race_generation_changed");
    if(!telemetryEnabled()||telemetryRace==raceEpoch)return; // one attempt per race
    telemetryRace=raceEpoch;
    const char* configured=std::getenv("STK_RELAY_TELEMETRY_DIR");
    std::string directory=configured&&*configured?configured:file_manager->getUserConfigDir()+"relay-telemetry";
    if(!directory.empty()&&directory.back()=='/')directory.pop_back();
    if(!file_manager->checkAndCreateDirectoryP(directory)){telemetryError="telemetry_directory_unavailable";return;}
    std::time_t clock=std::time(nullptr);std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc,&clock);
#else
    gmtime_r(&clock,&utc);
#endif
    char stamp[32],iso[32];std::strftime(stamp,sizeof(stamp),"%Y%m%d-%H%M%S",&utc);std::strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%SZ",&utc);
    const std::string path=directory+"/scintilla-"+stamp+"-race"+std::to_string(raceEpoch)+"-"+producerUUID().substr(0,8)+".ndjson";
    if(!telemetryWriter.open(path,&telemetryError))return;
    telemetryError.clear();consumer.setTelemetry(true);
    telemetryNextSample=telemetryNextPresentation=0;telemetryPresentedFrame=0;telemetryLastTickUs=telemetryFrameMaxUs=0;
    telemetryDepthPinned=telemetryDepthBudget=telemetryDepthInvalid=0;
    telemetryWrite(telemetryLine("header").text("schema",ScintillaTelemetry::Schema).text("startedAtUtc",iso)
        .text("producerUUID",producerUUID()).unsignedInteger("raceEpoch",raceEpoch)
        .text("track",RaceManager::get()->getTrackName()).integer("physicsFps",stk_config->getPhysicsFPS())
        .text("projection",current.projection.kind).integer("width",current.projection.width).integer("height",current.projection.height)
        .text("tuningSource",tuningSource).text("tuningSha256",tuningHash).text("tuningProfile",tuning.profileId)
        .raw("tuning",ScintillaTuning::effectiveJSON(tuning)).raw("crew",jsonStrings(lastRoster)));
}
// Nearest target to the judged reticle in the frame that was actually shown:
// edgeDeg < 0 is inside the marker, small positive values are near misses.
void telemetryAim(ScintillaTelemetry::Line& l,uint64_t frameId,double u,double v,double poseU,double poseV)
{
    auto f=std::find_if(publishedFrames.rbegin(),publishedFrames.rend(),[&](const Frame& x){return x.id==frameId;});
    if(f==publishedFrames.rend())return;
    const auto world=[&](double pu,double pv,SpatialVec3* out){
        SpatialVec3 local;if(!Runtime::projectionDirection(f->projection,pu,pv,&local))return false;
        *out=SpatialVec3(f->cameraRight.x*local.x+f->cameraUp.x*local.y+f->cameraForward.x*local.z,
            f->cameraRight.y*local.x+f->cameraUp.y*local.y+f->cameraForward.y*local.z,
            f->cameraRight.z*local.x+f->cameraUp.z*local.y+f->cameraForward.z*local.z);return true;};
    SpatialVec3 d;if(!world(u,v,&d))return;
    const double rad=180/3.14159265358979323846;
    SpatialVec3 pose;if(poseU>=0&&world(poseU,poseV,&pose))
        l.number("poseLagDeg",std::acos(std::max(-1.0,std::min(1.0,dot(d,pose))))*rad);
    const Target* best=nullptr;double bestEdge=1e9,bestOffset=0,bestRadius=0,bestDistance=0;
    for(const auto& t:f->targets){
        if(t.kind==TargetKind::DiagnosticPickup)continue;
        const SpatialVec3 delta(t.center.x-f->cameraPosition.x,t.center.y-f->cameraPosition.y,t.center.z-f->cameraPosition.z);
        const double distance=std::sqrt(dot(delta,delta));if(distance<=t.radius)continue;
        const double offset=std::acos(std::max(-1.0,std::min(1.0,dot(d,delta)/distance)))*rad;
        const double angular=std::asin(t.radius/distance)*rad;
        if(offset-angular<bestEdge){best=&t;bestEdge=offset-angular;bestOffset=offset;bestRadius=angular;bestDistance=distance;}
    }
    if(!best)return;
    const bool eligible=best->kind==TargetKind::Rival?best->combat.eligible:best->mission.eligible;
    l.raw("nearest",ScintillaTelemetry::Line("target").text("kind",targetKindName(best->kind)).unsignedInteger("id",best->id)
        .number("distanceM",bestDistance).number("angularRadiusDeg",bestRadius).number("offsetDeg",bestOffset)
        .number("edgeDeg",bestEdge).flag("eligible",eligible).str());
}
void noteLatencyFlash(const Reticle& r)
{
    if(!telemetryWriter.isOpen())return;
    auto& logged=telemetryFlashLogged[r.sourceId];if(logged==r.presses)return;logged=r.presses;
    FlashRecord record;record.source=r.sourceId;record.press=r.presses;record.frame=current.id;
    record.pressedUs=r.pressedUs;record.poseUs=r.pressPoseUs;record.captureUs=current.captureUs;
    telemetryFlashes.push_back(record);while(telemetryFlashes.size()>64)telemetryFlashes.pop_front();
    telemetryWrite(telemetryLine("latency_flash").text("source",r.sourceId).unsignedInteger("press",r.presses)
        .unsignedInteger("frame",current.id).integer("poseUs",r.pressPoseUs).integer("receivedUs",r.pressedUs).integer("captureUs",current.captureUs));
}
void telemetryCombatShot(const HitEvent& e,const ScintillaCombat::Outcome& result,bool applied)
{
    telemetryWrite(telemetryLine("combat_shot").text("participant",e.participantId).text("event",e.eventId)
        .unsignedInteger("target",e.targetId).text("reason",result.reason).flag("contributed",result.contributed)
        .flag("effectRequested",result.requestEffect).flag("effectApplied",applied).flag("automatic",e.automatic));
}
void telemetryMissionShot(const HitEvent& e,const ScintillaMission::Outcome& result)
{
    telemetryWrite(telemetryLine("mission_shot").text("participant",e.participantId).text("event",e.eventId)
        .unsignedInteger("entity",e.targetId).flag("accepted",result.accepted)
        .text("status",ScintillaMission::Runtime::statusReason(result.status)).flag("automatic",e.automatic));
}
void telemetrySample(int64_t now)
{
    auto* world=World::getWorld();auto* human=defenseHuman();if(!world||!human)return;
    double ahead=-1,behind=-1;unsigned attackable=0;
    for(unsigned i=0;i<world->getNumKarts();++i){
        auto* k=world->getKart(i);if(!k||k==human||k->isEliminated())continue;
        const double d=(k->getXYZ()-human->getXYZ()).length();
        if(k->getPosition()<human->getPosition()){if(ahead<0||d<ahead)ahead=d;}else if(behind<0||d<behind)behind=d;
    }
    for(const auto& t:current.targets)if(t.kind==TargetKind::Rival&&t.combat.eligible)++attackable;
    unsigned held=0,valid=0;for(const auto& b:consumer.beamProofs(now))held+=b.held;for(const auto& r:consumer.reticles())valid+=r.valid;
    const auto* linear=dynamic_cast<LinearWorld*>(world);
    telemetryWrite(telemetryLine("sample").number("speed",human->getSpeed()).integer("position",human->getPosition())
        .integer("lap",linear?linear->getLapForKart(human->getWorldKartId()):-1)
        .number("trackM",linear?linear->getDistanceDownTrackForKart(human->getWorldKartId(),false):-1)
        .number("aheadM",ahead).number("behindM",behind).integer("attackable",attackable)
        .integer("capsules",missionPlacements.size()).integer("secured",mission.securedCount())
        .text("defense",defensePhaseName(defense.view().phase)).number("energy",defense.energy()).number("heat",defense.heat())
        .flag("boostReady",defense.boostStatus(ScintillaDefensePhysics::eligibleHuman(human))==Defense::Status::BoostReady)
        .flag("humanTransient",ScintillaDefensePhysics::humanTransient(human))
        .integer("reticles",valid).integer("beams",held).flag("gameplay",telemetryGameplay)
        .number("frameMaxMs",telemetryFrameMaxUs/1000.0)
        .integer("depthSkippedPinned",telemetryDepthPinned).integer("depthSkippedBudget",telemetryDepthBudget)
        .integer("depthInvalid",telemetryDepthInvalid)
        .raw("visibilityLoss","["+std::to_string(visibilityLoss[LossLease])+","+std::to_string(visibilityLoss[LossRange])+","+
            std::to_string(visibilityLoss[LossFbo])+","+std::to_string(visibilityLoss[LossRead])+","+
            std::to_string(visibilityLoss[LossPoll])+","+std::to_string(visibilityLoss[StaleGlError])+"]"));
    for(auto& count:visibilityLoss)count=0;
    telemetryFrameMaxUs=0;telemetryDepthPinned=telemetryDepthBudget=telemetryDepthInvalid=0;
}
// Every frame, also while held or after the grant lapsed: the stall that ends
// an input grant is the one most worth recording.
void telemetryFrame(int64_t now)
{
    if(telemetryLastTickUs&&telemetryWriter.isOpen()){
        const int64_t gap=now-telemetryLastTickUs;telemetryFrameMaxUs=std::max(telemetryFrameMaxUs,gap);
        if(gap>=250000)telemetryWrite(telemetryLine("stall").number("ms",gap/1000.0));
    }
    telemetryLastTickUs=now;
}
void telemetryTick()
{
    if(!active||!combat.configured())return;
    openTelemetry();if(!telemetryWriter.isOpen())return;
    const int64_t now=nowUs();
    for(const auto& e:consumer.drainResolved()){
        auto l=telemetryLine("input");l.text("source",e.sourceId).text("participant",e.participantId).text("control",e.control)
            .text("event",e.eventId).text("result",e.result).flag("automatic",e.automatic).text("kind",targetKindName(e.kind))
            .unsignedInteger("target",e.targetId).unsignedInteger("frame",e.frameId).integer("poseUs",e.evaluatedUs)
            .number("u",e.u).number("v",e.v);
        if(e.frameId)telemetryAim(l,e.frameId,e.u,e.v,e.poseU,e.poseV);
        telemetryWrite(l);
    }
    for(const auto& n:consumer.drainNotes())
        telemetryWrite(telemetryLine("input_note").text("kind",n.kind).text("source",n.sourceId).text("participant",n.participantId)
            .text("control",n.control).text("reason",n.reason).integer("at",n.at));
    for(const auto& b:consumer.beamProofs(now)){
        auto& previous=telemetryBeams[b.participantId];
        if(previous.held!=b.held||previous.visible!=b.visible||previous.target!=b.target||previous.controlEpoch!=b.controlEpoch)
            telemetryWrite(telemetryLine("beam").text("participant",b.participantId).flag("held",b.held).flag("visible",b.visible)
                .unsignedInteger("target",b.target).unsignedInteger("controlEpoch",b.controlEpoch));
        previous=b;
    }
    for(const auto& s:combat.snapshots()){
        auto& previous=telemetryCombat[s.target];
        if(previous.phase!=s.phase||previous.count!=s.count||previous.window!=s.window||previous.instance!=s.instance){
            const auto& crew=combat.crew();
            telemetryWrite(telemetryLine("combat").unsignedInteger("target",s.target).unsignedInteger("instance",s.instance)
                .text("phase",combatPhaseName(s.phase)).integer("count",s.count).integer("required",s.required)
                .text("opener",s.opener>=0&&size_t(s.opener)<crew.size()?crew[s.opener]:"").unsignedInteger("window",s.window));
        }
        previous=s;
    }
    const auto& v=defense.view();
    if(v.phase!=telemetryDefense.phase||v.threat!=telemetryDefense.threat){
        auto l=telemetryLine("defense");l.text("phase",defensePhaseName(v.phase)).unsignedInteger("threat",v.threat)
            .unsignedInteger("attacker",v.attacker).integer("sector",int(v.sector)).text("reason",defense.reason())
            .number("energy",defense.energy()).number("heat",defense.heat());
        auto* human=defenseHuman();auto* attacker=defenseAttacker();
        if(human&&attacker)l.number("distanceM",(attacker->getXYZ()-human->getXYZ()).length());
        if(human)l.number("humanSpeed",human->getSpeed()).flag("humanTransient",ScintillaDefensePhysics::humanTransient(human));
        telemetryWrite(l);telemetryDefense=v;
    }
    if(defense.boostUntil()!=telemetryBoostUntil){
        if(defense.boostUntil()>telemetryBoostUntil)telemetryWrite(telemetryLine("boost").number("energy",defense.energy()));
        telemetryBoostUntil=defense.boostUntil();
    }
    if(mission.configured()){
        const auto& crew=mission.crew();const auto live=mission.snapshots();
        // Collected/expired capsules and a finished tether leave the snapshot list:
        // record their final state once instead of losing the transition.
        for(auto it=telemetryMission.begin();it!=telemetryMission.end();){
            if(std::any_of(live.begin(),live.end(),[&](const ScintillaMission::Snapshot& s){return s.id==it->first&&s.instance==it->second.instance;})){++it;continue;}
            const auto last=mission.snapshot(it->first);const bool same=last.id==it->first&&last.instance==it->second.instance;
            telemetryWrite(telemetryLine("mission_entity").text("kind",it->second.kind==ScintillaMission::Kind::Capsule?"capsule":"tether")
                .unsignedInteger("id",it->first).unsignedInteger("instance",it->second.instance).unsignedInteger("module",it->second.module)
                .text("phase",same?missionPhaseName(last.phase):"gone").text("owner","").integer("slot",it->second.slot));
            it=telemetryMission.erase(it);
        }
        for(const auto& s:live){
            auto& previous=telemetryMission[s.id];
            if(previous.phase!=s.phase||previous.instance!=s.instance||previous.owner!=s.owner)
                telemetryWrite(telemetryLine("mission_entity").text("kind",s.kind==ScintillaMission::Kind::Capsule?"capsule":"tether")
                    .unsignedInteger("id",s.id).unsignedInteger("instance",s.instance).unsignedInteger("module",s.module)
                    .text("phase",missionPhaseName(s.phase)).text("owner",s.owner>=0&&size_t(s.owner)<crew.size()?crew[s.owner]:"")
                    .integer("slot",s.slot));
            previous=s;
        }
        const auto& slots=mission.slots();bool changed=false;
        for(size_t i=0;i<slots.size();++i)changed=changed||slots[i].state!=telemetrySlots[i].state||slots[i].beaconLit!=telemetrySlots[i].beaconLit;
        if(changed){
            std::string states="[",beacons="[";
            for(size_t i=0;i<slots.size();++i){states+=(i?",":"")+ScintillaTelemetry::escape(slotName(slots[i].state));beacons+=(i?",":"")+std::string(slots[i].beaconLit?"true":"false");}
            telemetryWrite(telemetryLine("cargo").raw("slots",states+"]").raw("beacons",beacons+"]").integer("secured",mission.securedCount()));
            telemetrySlots=slots;
        }
        const auto& result=mission.result();
        if(result.valid&&!telemetryResult){
            telemetryResult=true;
            telemetryWrite(telemetryLine("mission_result").text("outcome",result.outcome).flag("natural",result.natural)
                .integer("position",result.officialTeamRacePosition).integer("secured",result.securedCount)
                .number("finishTime",result.nativeFinishTime).text("termination",result.terminationReason));
        }
    }
    if(lastRoster!=telemetryRoster){telemetryRoster=lastRoster;telemetryWrite(telemetryLine("roster").raw("crew",jsonStrings(lastRoster)));}
    const bool gameplay=RelaySessionController::get()->getSpatialGameplayState().m_active;
    if(gameplay!=telemetryGameplay){telemetryGameplay=gameplay;
        telemetryWrite(telemetryLine("gameplay").flag("active",gameplay).flag("held",RelaySessionController::get()->isWorldHeld()));}
    if(auto* human=defenseHuman()){
        auto* linear=dynamic_cast<LinearWorld*>(World::getWorld());
        const int lap=linear?linear->getLapForKart(human->getWorldKartId()):-1;
        if(lap!=telemetryLap){telemetryLap=lap;telemetryWrite(telemetryLine("lap").integer("lap",lap).integer("position",human->getPosition()));}
        const bool rescue=dynamic_cast<const RescueAnimation*>(human->getKartAnimation())!=nullptr;
        if(rescue&&!telemetryRescue)telemetryWrite(telemetryLine("rescue").integer("lap",lap)
            .number("trackM",linear?linear->getDistanceDownTrackForKart(human->getWorldKartId(),false):-1));
        telemetryRescue=rescue;
        if(human->hasFinishedRace()&&!telemetryFinished){telemetryFinished=true;
            telemetryWrite(telemetryLine("finish").integer("position",human->getPosition()).number("time",human->getFinishTime()));}
    }
    if(!telemetryFlashes.empty()||now>=telemetryNextPresentation){
        const auto times=consumer.presentedTimes(now);
        for(const auto& t:times){
            for(auto it=telemetryFlashes.begin();it!=telemetryFlashes.end();)
                if(it->frame==t.frame){
                    telemetryWrite(telemetryLine("latency_presented").text("source",it->source).unsignedInteger("press",it->press)
                        .unsignedInteger("frame",t.frame).integer("poseUs",it->poseUs).integer("receivedUs",it->pressedUs)
                        .integer("captureUs",t.captureUs).integer("presentedLowUs",t.low).integer("presentedHighUs",t.high));
                    it=telemetryFlashes.erase(it);
                }else ++it;
            if(t.frame>telemetryPresentedFrame){telemetryPresentedFrame=t.frame;telemetryPresentationMs.push_back((t.low-t.captureUs)/1000.0);}
        }
        if(now>=telemetryNextPresentation){
            telemetryNextPresentation=now+1000000;
            if(!telemetryPresentationMs.empty()){
                std::sort(telemetryPresentationMs.begin(),telemetryPresentationMs.end());const auto& p=telemetryPresentationMs;
                telemetryWrite(telemetryLine("presentation").integer("frames",p.size()).number("captureToPresentedP50Ms",p[p.size()/2])
                    .number("captureToPresentedP90Ms",p[p.size()*9/10]).number("captureToPresentedMaxMs",p.back()));
                telemetryPresentationMs.clear();
            }
        }
    }
    if(now>=telemetryNextSample){telemetryNextSample=now+500000;telemetrySample(now);}
}
}
std::string simulationViewJSON()
{
    // Diagnostic only ($STK_RELAY_SIMULATION=1): what the last published frame
    // showed, in output UV, for the software crew simulator. Never a proof.
    const char* enabled=std::getenv("STK_RELAY_SIMULATION");
    if(!enabled||std::string(enabled)!="1")return "";
    using ScintillaTelemetry::Line;
    std::ostringstream o;o<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-simulation/1\",\"diagnostic\":true"
        <<",\"nowUs\":"<<nowUs()<<",\"stream\":"<<ScintillaTelemetry::escape(stream)<<",\"frames\":[";
    size_t first=publishedFrames.size()>16?publishedFrames.size()-16:0;
    for(size_t i=first;i<publishedFrames.size();++i)
        o<<(i>first?",":"")<<"{\"id\":"<<publishedFrames[i].id<<",\"captureUs\":"<<publishedFrames[i].captureUs<<"}";
    o<<"]";
    if(publishedFrames.empty()){o<<",\"targets\":[],\"nodes\":[]}";return o.str();}
    const Frame& f=publishedFrames.back();
    o<<",\"frame\":{\"id\":"<<f.id<<",\"captureUs\":"<<f.captureUs<<"},\"projection\":"
     <<Line("projection").text("kind",f.projection.kind).integer("width",f.projection.width)
        .integer("height",f.projection.height).number("domeTiltDeg",f.projection.domeTiltDeg).str()<<",\"targets\":[";
    bool firstTarget=true;const auto& crew=combat.crew();
    for(const auto& t:f.targets){
        if(t.kind==TargetKind::DiagnosticPickup)continue;
        const SpatialVec3 delta(t.center.x-f.cameraPosition.x,t.center.y-f.cameraPosition.y,t.center.z-f.cameraPosition.z);
        const SpatialVec3 local(dot(delta,f.cameraRight),dot(delta,f.cameraUp),dot(delta,f.cameraForward));
        double u=0,v=0;if(!Runtime::projectionUV(f.projection,local,&u,&v))continue;
        const double distance=std::sqrt(dot(delta,delta));if(distance<=t.radius)continue;
        Line l("target");l.text("kind",targetKindName(t.kind)).unsignedInteger("id",t.id).unsignedInteger("instance",t.instance)
            .number("u",u).number("v",v).number("distanceM",distance).number("angularRadiusDeg",std::asin(t.radius/distance)*180/3.14159265358979323846);
        if(t.kind==TargetKind::Rival)
            l.flag("eligible",t.combat.eligible).text("phase",combatPhaseName(t.combat.phase)).unsignedInteger("window",t.combat.window)
             .integer("count",t.combat.count).integer("required",t.combat.required)
             .text("opener",t.combat.opener>=0&&size_t(t.combat.opener)<crew.size()?crew[t.combat.opener]:"");
        else l.flag("eligible",t.mission.eligible).text("phase",missionPhaseName(t.mission.phase));
        o<<(firstTarget?"":",")<<l.str();firstTarget=false;
    }
    o<<"],\"nodes\":[";
    if(f.layout.valid()){
        static const char* const names[]={"left","front","rear","right","boost"};
        for(unsigned i=0;i<5;++i){double x,y,r;f.layout.node(i,&x,&y,&r);
            o<<(i?",":"")<<Line("node").text("name",names[i]).number("u",x).number("v",y)
                .number("radiusU",r/f.layout.width).number("radiusV",r/f.layout.height).str();}
    }
    const auto& d=defense.view();
    o<<"],\"defense\":"<<Line("defense").flag("enabled",defenseBound()).text("phase",defensePhaseName(d.phase)).unsignedInteger("threat",d.threat)
        .integer("sector",int(d.sector)).number("progress",d.progress).number("energy",defense.energy())
        .flag("boostReady",defense.boostStatus(ScintillaDefensePhysics::eligibleHuman(defenseHuman()))==Defense::Status::BoostReady).str()
     <<",\"crew\":"<<jsonStrings(crew)<<"}";
    return o.str();
}
std::string telemetryStateJSON()
{
    return ScintillaTelemetry::Line("telemetry").flag("enabled",telemetryEnabled()).flag("open",telemetryWriter.isOpen())
        .text("path",telemetryWriter.path()).text("error",telemetryError)
        .unsignedInteger("dropped",telemetryWriter.dropped()).unsignedInteger("written",telemetryWriter.written()).str();
}
void loadTuning()
{
    ScintillaTuning::Tuning loaded;std::string error,hash,bytes;
    const char* path=std::getenv(TuningEnvironment);
    if(path&&*path){
        if(!readMissionFile(path,65536,&bytes))error="tuning:file:unreadable";
        else {hash=missionSHA256(bytes);ScintillaTuning::parse(bytes,&loaded,&error);}
    }
    // An invalid profile keeps the defaults but blocks preparation visibly.
    tuning=error.empty()?loaded:ScintillaTuning::Tuning();
    tuningSource=path&&*path?path:"builtin";tuningHash=hash;tuningError=error;
    consumer.setTuning(tuning.input);
}
std::string tuningStateJSON()
{
    std::ostringstream o;
    o<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-tuning-state/1\",\"environment\":\""<<TuningEnvironment<<'"'
     <<",\"source\":"<<ScintillaJSON::quote(tuningSource)<<",\"sha256\":"<<ScintillaJSON::quote(tuningHash)
     <<",\"error\":"<<ScintillaJSON::quote(tuningError)<<",\"raceEpoch\":\""<<raceEpoch<<'"'
     <<",\"locked\":"<<(combat.configured()||defense.configured()||mission.configured()?"true":"false")
     <<",\"active\":"<<ScintillaTuning::effectiveJSON(tuning)<<",\"parameters\":"<<ScintillaTuning::parametersJSON(tuning)
     <<",\"telemetry\":"<<telemetryStateJSON()<<"}";
    return o.str();
}
Reply reloadTuning(const std::string& body)
{
    std::string empty;for(char c:body)if(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')empty+=c;
    if(empty!="{}")return Reply(400,"{\"ok\":false,\"error\":\"invalid_tuning_reload\"}");
    // A prepared race keeps its parameters; reload applies to the next one.
    if(combat.configured()||defense.configured()||mission.configured())return Reply(409,"{\"ok\":false,\"error\":\"tuning_locked_by_prepared_race\"}");
    loadTuning();return Reply(tuningError.empty()?200:409,tuningStateJSON());
}
void newWorld(bool preserveSpatialGrant)
{
    closeTelemetry("race_generation_changed");
    loadTuning();
    interruptMissionMeasurement("race_generation_changed");resetMissionMeasurementProofs();
    if(!preserveSpatialGrant)invalidateMissionQualificationCache();
    invalidateState("race_generation_changed",preserveSpatialGrant&&active&&defense.required());
    combat.reset(true);defense.reset(true);mission.reset(true);crew.reset(true);
    crewCreditSerial=crewObservedAdministrative=0;crewDefenseThreatTick=-1;clearCrewCoordinationEvidence();resetCrewAuthority();resetCrewPartyZones();
    missionTerminalRecord=ScintillaMission::Result();missionStepOpen=missionClockBound=false;
    crewTerminalRecord=Crew::Result();crewTerminalObservation=Crew::Observation();
    missionRequests.reset();missionIntroReceipt=MissionProtocol::IntroReceipt();missionPreparedIdentity=MissionProtocol::Identity();
    missionIntroHash.clear();missionIntroPath.clear();missionIntroVariant.clear();missionIntroProfile.clear();missionIntroAvailable=missionAPIPrepared=false;missionIntroError="mission_intro_not_loaded";missionAPIRevision=0;
    missionClockBase=0;missionStepFailure.clear();
    missionTraversal.reset();missionPlacements.clear();missionAnchors=Anchors::Manifest();
    anchorRace=0;anchorCrew=0;anchorHash.clear();anchorStatus="not_loaded";
    missionPositionKnown=missionPreviousAnimated=false;
    missionProgressKnown=false;
#ifndef SERVER_ONLY
    missionAssetsInvalidated=false;
#endif
    clearMissionPresentationQueries();tetherPlacementInstance=0;tetherSide=-1;
    tetherCreatedUs=0;tetherPresentationHold=false;
    missionCueRace=0;missionCueFinish=0;missionCueSlots={};
    humanPointer=nullptr;humanInstance=0;soloIdleSince=0;combatOwners.clear();
    simulationTick=0;suppressedStaggerMotion=0;++raceEpoch;++rosterEpoch;lastRoster.clear();
    synchronizeCrewTurn();const auto observation=missionObservation();
    const auto crewObservation=Crew::observeResult(crew,producerID,observation.raceEpoch,observation.missionRevision,
        observation.finishTick,RelaySessionController::get()->getSnapshot().m_test_accelerated_progress,crewPartyZoneManifestHash());
    RelaySessionController::get()->setCrewStartState(crew.required(),false);
    RelaySessionController::get()->beginMissionRace(mission.required(),observation,crewObservation);
}
void retireWorld()
{
    interruptMissionMeasurement("world_destroyed");resetMissionMeasurementProofs();
    invalidateMissionQualificationCache();
    terminateMission("world_destroyed");closeTelemetry("world_retired");invalidateState("world_retired",false);
    combat.reset(true);defense.reset(true);humanPointer=nullptr;humanInstance=0;
    combatOwners.clear();lastRoster.clear();
}
void shutdown(){closeTelemetry("shutdown");invalidateMissionQualificationCache();invalidate("output_closed");}
bool combatPolicyActive(){return combat.configured();}
bool combatRequired(){return combat.required();}
void refreshCombatInput()
{
    const auto terminal=RelaySessionController::get()->getMissionResult();
    if(mission.required() && terminal.valid && !missionTerminalRecord.valid)
        terminateMission(terminal.terminationReason);
    refreshRoster();synchronizeCrewTurn();
    consumer.allowParticipantRebind(defense.required());consumer.setParticipants(lastRoster);
    consumer.setMissionLayoutRequired(mission.required());
    consumer.setCrewContext(crew.required(),crew.partyEnabled(),raceEpoch,crew.driverTurnId(),
        crew.intentRevision(),crew.partyView().proposalId,rosterEpoch);
    consumer.setPartyFeedback(crew.partyFeedback());
    const bool terminalMission=mission.required()&&missionTerminal();
    consumer.setCombatContext(!terminalMission&&active&&combat.configured()&&consumer.prepared()&&consumer.inputEpoch()==combat.inputEpoch(),simulationTick,combat.snapshots(),raceEpoch);
    consumer.setCombatToolFeedback(combat.toolSnapshots());
    consumer.setDefenseContext(!terminalMission&&defenseBound(),defense.feedback());
    // Policy/history remain bound while a technical hold awaits real video.
    // The consumer's gameplay-active gate separately forbids all input commits.
    const bool missionContext=mission.required()&&active&&mission.configured()&&
        !terminalMission&&consumer.prepared()&&localCombatSupported()&&
        mission.race()==raceEpoch&&mission.inputEpoch()==consumer.inputEpoch()&&
        mission.crew()==lastRoster&&mission.profile().fps==stk_config->getPhysicsFPS();
    consumer.setMissionContext(missionContext,mission.revision(),
        mission.snapshots(),mission.feedback());
    const bool base=active&&combat.configured()&&consumer.prepared()&&combat.inputEpoch()==consumer.inputEpoch()&&combat.crew()==lastRoster&&localCombatSupported();
    RelaySessionController::get()->setScintillaStartPermitted((!combat.required()||base)&&
        (!defense.required()||defenseBound())&&missionStartPermitted());
    RelaySessionController::get()->setCrewStartState(crew.required(),crewPreparationBound());
    publishMissionSnapshot();
}
bool combatStartPermitted()
{
    if(consumer.inputMaintenanceActive())return false;
    if(!combat.configured())return !mission.required()&&!defense.required()&&combat.permitsStart(false);
    refreshCombatInput();
    const auto gameplay=RelaySessionController::get()->getSpatialGameplayState();
    consumer.tick(nowUs(),gameplay.m_active&&!missionTerminal(),gameplay.m_resume_revision,gameplay.m_resume_at_us);
    return !missionTerminal()&&missionStartPermitted()&&crewStartPermitted()&&
        combat.permitsStart(active&&localCombatSupported()&&consumer.prepared()&&combat.inputEpoch()==consumer.inputEpoch()&&combat.crew()==lastRoster)&&
        (!defense.required()||(defenseBound()&&RelaySessionController::get()->getSnapshot().m_presentation_ready));
}
std::string combatStateJSON()
{
    refreshRoster();
    const bool ready=active&&combat.configured()&&consumer.prepared()&&combat.inputEpoch()==consumer.inputEpoch()&&combat.crew()==lastRoster;
    std::ostringstream out;
    out<<"{\"ok\":true,\"contract\":\"tholus.stk-scintilla-combat/1\",\"profile\":\"crew_standard_sc2_v1\",\"raceEpoch\":\""<<raceEpoch
       <<"\",\"rosterRevision\":\""<<rosterEpoch<<"\",\"inputSessionID\":\""<<consumer.inputSessionID()<<"\",\"prepared\":"<<(ready?"true":"false")
       <<",\"policyActive\":"<<(combat.configured()?"true":"false")<<",\"required\":"<<(combat.required()?"true":"false")<<",\"intentRevision\":\""<<combat.intentRevision()<<"\",\"suppressedStaggerMotion\":"<<suppressedStaggerMotion<<",\"simulationTick\":"<<simulationTick<<",\"targets\":[";
    bool first=true;for(const auto& s:combat.snapshots()){
        if(!first)out<<',';first=false;
        out<<"{\"targetId\":"<<s.target<<",\"instance\":"<<s.instance<<",\"window\":"<<s.window<<",\"phase\":"<<int(s.phase)
           <<",\"count\":"<<s.count<<",\"required\":"<<s.required<<",\"deadlineTick\":"<<s.deadline<<'}';
    }
    out<<"]}";return out.str();
}
Reply setCombatIntent(const std::string& body)
{
    if(missionMeasurementActive())return Reply(409,"{\"ok\":false,\"error\":\"mission_measurement_active\"}");
    bool required=false;
    const Reply valid=consumer.validateCombatIntent(body,std::to_string(raceEpoch),std::to_string(combat.intentRevision()),&required);
    if(valid.status!=200)return valid;
    if(!required&&mission.required())return Reply(409,"{\"ok\":false,\"error\":\"mission_requires_combat\"}");
    auto* controller=RelaySessionController::get();
    auto* world=World::getWorld();
    if(!world||world->getIdent()!="IMMERSIVE_RELAY"||!controller->isWorldHeld()||
       (required&&(controller->getSnapshot().m_crew_mode!="disabled"||!localCombatSupported())))
        return Reply(409,"{\"ok\":false,\"error\":\"combat_intent_requires_held_local_world\"}");
    if(combat.intentRevision()==std::numeric_limits<uint64_t>::max())
        return Reply(409,"{\"ok\":false,\"error\":\"combat_intent_exhausted\"}");
    if(!required&&defense.required())return Reply(409,"{\"ok\":false,\"error\":\"defense_requires_combat\"}");
    if(!combat.setRequired(required))return Reply(409,"{\"ok\":false,\"error\":\"combat_requires_race_reset\"}");
    refreshCombatInput();return Reply(200,combatStateJSON());
}
Reply prepareCombat(const std::string& body)
{
    refreshRoster();
    Reply valid=consumer.validateCombatPrepare(body,std::to_string(raceEpoch),std::to_string(rosterEpoch));
    if(valid.status!=200)return valid;
    auto* controller=RelaySessionController::get();
    if(!active||!consumer.prepared()||!World::getWorld()||World::getWorld()->getIdent()!="IMMERSIVE_RELAY"||!controller->isWorldHeld()||
       controller->getSnapshot().m_crew_mode!="disabled"||lastRoster!=consumer.participants()||!localCombatSupported())
        return Reply(409,"{\"ok\":false,\"error\":\"combat_requires_prepared_local_world\"}");
    const bool first=!combat.configured();
    // Initial policy changes cannot leave vanilla projectiles already in flight.
    // Rebinding an existing SC2 race remains supported without clearing recovery.
    if(first&&World::getWorld()->getTimeTicks()>0)return Reply(409,"{\"ok\":false,\"error\":\"combat_requires_race_reset\"}");
    if(!tuningError.empty())return Reply(409,"{\"ok\":false,\"error\":\"tuning_invalid\",\"detail\":"+ScintillaJSON::quote(tuningError)+"}");
    if(!combat.prepare(raceEpoch,consumer.inputEpoch(),lastRoster,stk_config->getPhysicsFPS(),tuning.combat))return Reply(409,"{\"ok\":false,\"error\":\"combat_profile_conflict\"}");
    if(first){
        // This boundary occurs while held, before any combat can be committed.
        World* world=World::getWorld();
        for(unsigned i=0;i<world->getNumKarts();++i){auto* kart=world->getKart(i);if(!kart)continue;
            auto* powerup=kart->getPowerup();if(powerup&&powerup->getType()!=PowerupManager::POWERUP_ZIPPER)powerup->reset();
            if(kart->getAttachment()->getType()==Attachment::ATTACH_BOMB||kart->isShielded())kart->getAttachment()->clear();
        }
    }
    refreshCombatInput();return Reply(200,combatStateJSON());
}
void updateCombat(int ticks,bool gameplayActive)
{
    telemetryFrame(nowUs());
    if(!gameplayActive||RelaySessionController::get()->isWorldHeld())breakMissionMeasurementContinuity();
    // Presentation recovery is serviced even during the technical world hold.
    // It uses real presented frames, never a gameplay-active synthetic proof.
    if(mission.required())serviceMissionPresentation(nowUs());
    // A native countdown->RACE_PHASE transition did not open a mission step.
    // Its first complete admitted SC4 step is the following physics tick.
    if(mission.required()) gameplayActive=gameplayActive&&missionStepOpen&&!missionTerminal();
    if(!combat.configured())return;
    World* world=World::getWorld();
    if(!world){combat.suspendInput();defense.suspend("world_unavailable");consumer.setCombatContext(false,simulationTick,{},raceEpoch);return;}
    if(ticks>0 && !mission.required())simulationTick+=ticks;
    refreshRoster();
    const auto gameplay=RelaySessionController::get()->getSpatialGameplayState();
    consumer.tick(nowUs(),gameplayActive&&gameplay.m_active&&!missionTerminal(),gameplay.m_resume_revision,gameplay.m_resume_at_us);
    const bool admitted=gameplayActive&&gameplay.m_active&&active&&consumer.prepared()&&combat.inputEpoch()==consumer.inputEpoch()&&lastRoster==combat.crew();
    if(!admitted)combat.suspendInput();
    std::set<uint64_t> present;
    for(unsigned i=0;i<world->getNumKarts()&&present.size()<32;++i){
        AbstractKart* kart=world->getKart(i);const uint64_t id=1u+i;
        if(!TargetLifetime::eligibleKart(kart,world->getPlayerKart(0)))continue;
        present.insert(id);auto& owner=combatOwners[id];
        if(owner.pointer!=kart){owner.pointer=kart;owner.instance=++combatNextInstance;owner.effectRunning=false;}
        combat.observeTarget(id,owner.instance,!kart->getKartAnimation()&&!kart->isInvulnerable()&&!kart->isShielded());
    }
    for(auto it=combatOwners.begin();it!=combatOwners.end();)if(!present.count(it->first)){combat.retireTarget(it->first);it=combatOwners.erase(it);}else ++it;
    std::vector<ScintillaCombat::Beam> beams;
    if(admitted)for(const auto& p:consumer.beamProofs(nowUs())){
        ScintillaCombat::Beam b;b.participant=p.participantId;b.target=p.target;b.instance=p.instance;b.controlEpoch=p.controlEpoch;b.held=p.held;b.visible=p.visible;
        beams.push_back(b);
    }
    const auto previousCrewTargets=combat.snapshots();
    combat.update(simulationTick,admitted,beams);
    if(admitted&&crew.required())for(const auto& target:combat.snapshots()) {
        const auto before=std::find_if(previousCrewTargets.begin(),previousCrewTargets.end(),[&](const ScintillaCombat::Snapshot& old){return old.target==target.target&&old.instance==target.instance;});
        if(target.phase==ScintillaCombat::Phase::Breached&&target.opener>=0&&size_t(target.opener)<lastRoster.size()&&
           (before==previousCrewTargets.end()||before->phase!=ScintillaCombat::Phase::Breached||before->window!=target.window))
            recordCrewCredit(lastRoster[target.opener],Crew::ContributionKind::Opening);
    }
    // Native animation lifetime is reconciled on the authoritative tick, never a GL callback.
    if(gameplayActive)for(auto& e:combatOwners){
        auto* kart=world->getKart(unsigned(e.first-1));
        if(e.second.effectRunning&&kart->getKartAnimation()!=e.second.effectAnimation){
            e.second.effectRunning=false;combat.animationFinished(e.first,e.second.instance);
        }
    }
    std::vector<HitEvent> missionShots;
    for(const auto& e:consumer.drainCombatShots()){
        if(e.kind==TargetKind::MissionCapsule || e.kind==TargetKind::MissionTether){
            missionShots.push_back(e);continue;
        }
        if(!admitted||e.inputEpoch!=combat.inputEpoch()||e.result!="hit")continue;
        if(e.kind==TargetKind::DiagnosticPickup){
            consumer.combatFeedback(e,FeedbackKind::Rejected,"diagnostic_pickup_only",nowUs());continue;
        }
        const auto owner=combatOwners.find(e.targetId);
        if(owner==combatOwners.end()||owner->second.instance!=e.historical.instance){
            consumer.combatFeedback(e,FeedbackKind::Rejected,"target_instance_retired",nowUs());continue;
        }
#ifndef SERVER_ONLY
        if(!TargetLifetime::isLive(registered,e.targetId,e.targetInstance,currentTarget)){
            consumer.combatFeedback(e,FeedbackKind::Rejected,"target_instance_retired",nowUs());continue;
        }
#endif
        ScintillaCombat::Shot shot;shot.race=e.raceEpoch;shot.serial=e.serial;shot.inputEpoch=e.inputEpoch;shot.target=e.targetId;shot.instance=e.historical.instance;
        shot.admitted=e.admittedTick;shot.participant=e.participantId;shot.event=e.eventId;
        shot.window=e.historical.window;shot.historicalEligible=e.historicalEligible;shot.historicalOpen=e.historicalOpen;
        for(const auto& at:e.admittedTargets)if(at.target==e.targetId&&at.instance==shot.instance&&at.window==shot.window&&at.phase==ScintillaCombat::Phase::Breached)shot.admittedOpen=true;
        auto result=combat.shoot(shot);bool applied=false;
        if(result.requestEffect){
            AbstractKart* kart=world->getKart(unsigned(e.targetId-1));
            ExplosionAnimation* animation=localCombatSupported()?ExplosionAnimation::createRelayStagger(kart,combat.profile().stagger):nullptr;
            applied=animation!=nullptr;if(animation&&animation->relayMotionSuppressed())++suppressedStaggerMotion;
            combat.confirmEffect(result.effect,applied);owner->second.effectRunning=applied;owner->second.effectAnimation=animation;
            result.reason=applied?"stagger_applied":"stagger_rejected";
        }
        const FeedbackKind feedback=applied?FeedbackKind::Stagger:
            result.contributed&&!result.requestEffect?FeedbackKind::Contribution:
            result.reason=="shield_rebound"?FeedbackKind::Rebound:FeedbackKind::Rejected;
        if(result.contributed&&(!result.requestEffect||applied))recordCrewCredit(e.participantId,Crew::ContributionKind::ComboContribution);
        if(applied)recordCrewCredit(e.participantId,Crew::ContributionKind::Stagger);
        consumer.combatFeedback(e,feedback,result.reason,nowUs());
        telemetryCombatShot(e,result,applied);
    }
    updateDefense(admitted);
    // Crossing precedes new capsule collections: a collection in this step
    // cannot retroactively light a confirmation station already crossed.
    if(admitted&&mission.active()&&!missionTerminal())updateMissionCrossings();
    if(admitted&&mission.required()&&mission.active()&&!missionTerminal()){
        for(const auto& e:missionShots){
            if(e.result!="hit" || e.inputEpoch!=mission.inputEpoch() ||
               e.raceEpoch!=mission.race() || e.missionRevision!=mission.revision())continue;
            if(!liveMissionTarget(e.targetId,e.targetInstance)){
                consumer.combatFeedback(e,FeedbackKind::Rejected,"target_instance_retired",nowUs());continue;
            }
            if(e.kind==TargetKind::MissionTether){
                consumer.combatFeedback(e,FeedbackKind::Rejected,"use_tool",nowUs());continue;
            }
            ScintillaMission::Shot shot;
            shot.race=e.raceEpoch;shot.revision=e.missionRevision;shot.inputEpoch=e.inputEpoch;
            shot.serial=e.serial;shot.entity=e.targetId;shot.instance=e.targetInstance;
            shot.phaseRevision=e.historicalMission.phaseRevision;
            shot.participant=e.participantId;shot.event=e.eventId;
            const auto& historical=e.historicalMission;
            shot.historicalEligible=e.historicalEligible&&historical.eligible&&
                historical.id==e.targetId&&historical.instance==e.targetInstance&&
                historical.kind==ScintillaMission::Kind::Capsule;
            for(const auto& at:e.admittedMissionTargets)
                if(at.id==shot.entity && at.instance==shot.instance &&
                   at.phaseRevision==shot.phaseRevision && at.module==historical.module &&
                   at.kind==historical.kind && at.phase==historical.phase && at.eligible)
                    shot.admittedEligible=true;
            const auto result=mission.openCapsule(shot);
            if(result.accepted&&result.changed)recordCrewCredit(e.participantId,Crew::ContributionKind::ShellOpened);
            consumer.combatFeedback(e,result.accepted?FeedbackKind::Hit:FeedbackKind::Rejected,
                ScintillaMission::Runtime::statusReason(result.status),nowUs());
            telemetryMissionShot(e,result);
        }
        const auto previousCrewModules=mission.snapshots();
        const auto previousCrewSlots=mission.slots();
        mission.updateBeams(consumer.missionBeams(nowUs()));
        for(const auto& before:previousCrewModules)if(before.owner>=0&&size_t(before.owner)<lastRoster.size()) {
            for(unsigned slot=0;slot<3;++slot)if(mission.slots()[slot].state==ScintillaMission::SlotState::Secured&&
                mission.slots()[slot].module==before.module&&previousCrewSlots[slot].state!=ScintillaMission::SlotState::Secured)
                recordCrewCredit(lastRoster[before.owner],before.kind==ScintillaMission::Kind::Tether?Crew::ContributionKind::ModuleRecovered:Crew::ContributionKind::ModuleCollected);
        }
    }else if(mission.required())mission.suspendInput();
    commitOptionalService(admitted&&!missionTerminal());
    updateCrewCoordination(admitted&&!missionTerminal());
    updateMissionCues();missionStepOpen=false;publishMissionSnapshot();
    telemetryTick();refreshCombatInput();
}
#include "modes/immersive_relay/spatial/scintilla_crew_adapter.hpp"
#include "modes/immersive_relay/spatial/scintilla_crew_wire.hpp"
#include "modes/immersive_relay/spatial/scintilla_crew_services.hpp"
#include "modes/immersive_relay/spatial/scintilla_crew_zones_native.hpp"

void beforeCapture(const ImmersiveCameraRig& rig,const ImmersiveRenderConfig& config)
{
    if(!active)return;
    pollMissionMeasurement();
    consumer.setParticipants(RelaySessionController::get()->getSpatialCrewIDs());
#ifndef SERVER_ONLY
    pollRetiredGraphics();
    // Poll before reclaiming idle cache entries. Requested results, including
    // shared/completed queries, outlive the requesting actions' deadlines.
    ReadbackCache::collectIdle(queries,nowUs(),poll,
        [](Query&& q){retiredQueries.push_back(std::move(q));});
    pollRetiredGraphics();
    current=Frame();current.stream=stream;current.id=++sequence;current.generation=generation;current.captureUs=nowUs();
    current.routeRevision=consumer.activeRouteRevision();current.projectionContextId=consumer.activeProjectionContextId();
    current.projection.kind=projectionKindToString(config.m_projection_mode);current.projection.width=config.m_projection_width;current.projection.height=config.m_projection_height;current.projection.domeTiltDeg=config.m_dome_tilt_deg;
    const auto& pose=rig.getPose();
    current.cameraPosition=SpatialVec3(pose.m_position.x(),pose.m_position.y(),pose.m_position.z());
    current.cameraRight=SpatialVec3(pose.m_right.x(),pose.m_right.y(),pose.m_right.z());
    current.cameraUp=SpatialVec3(pose.m_up.x(),pose.m_up.y(),pose.m_up.z());
    current.cameraForward=SpatialVec3(pose.m_forward.x(),pose.m_forward.y(),pose.m_forward.z());
    current.combatMaxRange=tuning.combat.maxRangeM;current.combatMinAngularSine=combatMinAngularSine();
    current.nearPlane=0.2;current.farPlane=Track::getCurrentTrack()?Track::getCurrentTrack()->getCameraFar():1000;current.faceFovDeg=config.m_face_fov_deg;
    current.missionEnabled=mission.configured()&&!missionTerminal();
    current.missionRaceEpoch=raceEpoch;current.missionRevision=mission.revision();
    current.missionResult=missionTerminalRecord.valid?missionTerminalRecord:RelaySessionController::get()->getMissionResult();
    current.missionSlots=current.missionResult.valid?current.missionResult.slots:mission.slots();
    current.missionTick=mission.tick();current.missionHUDEnabled=mission.required();
    current.missionResultAtlasReady=missionResultAtlas!=nullptr;
    current.crewEnabled=effectiveCrewLayout();current.partyEnabled=current.crewEnabled&&effectiveCrewProfile()=="sc5_crew_party_v1";
    current.crewRaceEpoch=raceEpoch;current.crewDriverTurnId=crew.driverTurnId();current.crewIntentRevision=crew.intentRevision();
    current.crewInputEpoch=consumer.inputEpoch();current.crewRosterRevision=rosterEpoch;current.crewProposalId=crew.partyView().proposalId;
    current.crewParty=crew.partyView();current.crewAssignments=crew.assignments();current.crewSuggestions=crew.suggestions();
    current.crewReleaseAdvice=crew.releaseAdvice();
    if(defenseBound()||current.missionHUDEnabled||current.crewEnabled)
        current.layout=Defense::Layout::create(current.projection.width,current.projection.height,
            current.missionHUDEnabled||current.crewEnabled,current.missionResult.valid,current.crewEnabled);
    if(defenseBound())current.defense=defense.view();
    const auto tether=mission.snapshot(ScintillaMission::Runtime::TetherEntityId);
    if(tether.id&&tether.phase==ScintillaMission::Phase::TetherRecoverable&&mission.profile().fps>0)
        current.missionTetherSeconds=double(std::max<int64_t>(0,tether.deadline-mission.tick()))/mission.profile().fps;
    auto* missionHuman=defenseHuman();
    if(current.missionHUDEnabled&&missionHuman){
        const auto kartPose=missionHuman->getSmoothedTrans();
        for(unsigned i=0;i<3;++i){
            const ::Vec3 p(kartPose(::Vec3((int(i)-1)*.25f,missionHuman->getKartHeight()+.25f,0)));
            current.missionNucleus[i]=SpatialVec3(p.x(),p.y(),p.z());
        }
    }
    auto* graph=DriveGraph::get();
    if(current.missionHUDEnabled&&anchorRace==raceEpoch&&graph&&missionHuman){
        current.missionDecorations=true;
        for(unsigned i=0;i<3;++i){
            const auto& station=missionAnchors.confirmations[i];
            const ::Vec3 p=graph->getNode(station.node)->getLowerCenter()+::Vec3(0,7.f,0);
            current.missionBeacons[i]=SpatialVec3(p.x(),p.y(),p.z());
        }
    }
    if(current.missionEnabled&&missionHuman&&humanInstance==mission.humanInstance()){
        current.missionHumanInstance=humanInstance;
        const auto pose=missionHuman->getSmoothedTrans();
        for(unsigned side=0;side<2;++side){
            const ::Vec3 p(pose(::Vec3(side?ScintillaMission::TetherLateralM:-ScintillaMission::TetherLateralM,ScintillaMission::TetherHeightM,ScintillaMission::TetherForwardM)));
            current.missionTetherCandidates[side]=SpatialVec3(p.x(),p.y(),p.z());
        }
        const auto snapshot=mission.snapshot(ScintillaMission::Runtime::TetherEntityId);
        if(snapshot.id&&snapshot.instance==tetherPlacementInstance&&tetherSide>=0){
            Target value;value.kind=TargetKind::MissionTether;value.id=snapshot.id;
            value.instance=snapshot.instance;value.mission=snapshot;value.radius=.5;
            value.center=current.missionTetherCandidates[unsigned(tetherSide)];
            const SpatialVec3 delta(value.center.x-current.cameraPosition.x,
                value.center.y-current.cameraPosition.y,value.center.z-current.cameraPosition.z);
            value.mission.eligible=value.mission.eligible&&
                ScintillaMission::Profile::inRange(snapshot.kind,dot(delta,delta),value.radius)&&
                pointInOutput(current,value.center,value.radius,false);
            current.targets.push_back(value);
            const ::Vec3 socket(pose(::Vec3(0,missionHuman->getKartHeight()+.35f,0)));
            current.missionCableStart=SpatialVec3(socket.x(),socket.y(),socket.z());
            current.missionCableEnd=value.center;current.missionCableRadius=.025;
        }
    }
    // Publish Relay geometry before rivals and diagnostics. Its core-owned
    // instance is immutable; the last-render pointer registry is not its owner.
    if(current.missionEnabled)for(const auto& entry:missionPlacements){
        const auto snapshot=mission.snapshot(entry.first);
        if(snapshot.instance!=entry.second.instance||
           (snapshot.phase!=ScintillaMission::Phase::Shell&&snapshot.phase!=ScintillaMission::Phase::Open))continue;
        Target value;value.kind=TargetKind::MissionCapsule;value.id=snapshot.id;
        value.instance=snapshot.instance;value.center=entry.second.center;value.radius=entry.second.radius;
        value.mission=snapshot;
        const SpatialVec3 delta(value.center.x-current.cameraPosition.x,
            value.center.y-current.cameraPosition.y,value.center.z-current.cameraPosition.z);
        value.mission.eligible=value.mission.eligible&&
            ScintillaMission::Profile::inRange(snapshot.kind,dot(delta,delta),value.radius)&&
            pointInOutput(current,value.center,value.radius,false);
        current.targets.push_back(value);
    }
    std::map<uint64_t,bool> wasPresent;for(auto& item:registered){wasPresent[item.first]=item.second.present;item.second.present=false;}
    World* world=World::getWorld();
    if(world)for(unsigned i=0;i<world->getNumKarts() && current.targets.size()<32;i++) {
        AbstractKart* kart=world->getKart(i);
        if(!kart||!TargetLifetime::eligibleKart(kart,world->getPlayerKart(0)))continue;
        const uint64_t id=1u+i;registered[id].present=wasPresent[id];
        const ::Vec3 p(kart->getSmoothedTrans()(::Vec3(0,kart->getKartHeight()+0.5f,0)));
        target(id,kart,SpatialVec3(p.x(),p.y(),p.z()),tuning.combat.markerRadiusM);
        current.targets.back().kind=TargetKind::Rival;
        current.targets.back().combat=combat.snapshot(id);
        const SpatialVec3 delta(p.x()-current.cameraPosition.x,p.y()-current.cameraPosition.y,p.z()-current.cameraPosition.z);
        if(!ScintillaCombat::Profile::inRange(dot(delta,delta),tuning.combat.markerRadiusM,current.combatMaxRange,current.combatMinAngularSine))current.targets.back().combat.eligible=false;
    }
    ItemManager* items=Track::getCurrentTrack()?Track::getCurrentTrack()->getItemManager():NULL;
    if(items)for(unsigned i=0;i<items->getNumberOfItems() && current.targets.size()<32;i++) {
        const ItemState* item=items->getItem(i);
        if(!TargetLifetime::eligibleItem(item,ItemState::ITEM_BONUS_BOX))continue;
        const uint64_t id=100000u+i;registered[id].present=wasPresent[id];const ::Vec3& p=item->getXYZ();
        target(id,item,SpatialVec3(p.x(),p.y()+0.8,p.z()),0.25);
        current.targets.back().kind=TargetKind::DiagnosticPickup;
    }
    for(auto it=registered.begin();it!=registered.end();)if(!it->second.present)it=registered.erase(it);else ++it;
    if(defenseBound()){
        for(const auto&t:current.targets)if(t.id==current.defense.attacker){current.defenseSource=t.center;current.defenseSourceRadius=t.radius;current.defenseSourceLineClear=ScintillaDefensePhysics::cameraLineClear(Defense::Vec(current.cameraPosition.x,current.cameraPosition.y,current.cameraPosition.z),defenseHuman(),defenseAttacker());break;}
        if(current.defense.phase==Defense::Phase::LaunchPending){Defense::Candidate c;Defense::Ballistic b;if(defenseGeometry(&c,&b))current.defense.pulse=c.origin;}
    }
    prepareMissionMeasurementFrame(config);
    const RelaySpatialGameplayState gameplay=RelaySessionController::get()->getSpatialGameplayState();
    refreshCombatInput();consumer.tick(nowUs(),world && gameplay.m_active&&!missionTerminal(),gameplay.m_resume_revision,gameplay.m_resume_at_us);encodeStamp();
#endif
}
void retainDepth(const FullSphereCapture& capture)
{
    if(!active)return;
#ifndef SERVER_ONLY
    const unsigned size=capture.getSize();
    const size_t bytes=(size_t)size*size*6*4;
    const size_t slots=std::min<size_t>(12,bytes?depthBudget/bytes:0);
    if(!slots){++telemetryDepthBudget;return;}
    pollRetiredGraphics();
    while(!depths.empty() && (depths.size()>=slots || allocatedDepthBytes>depthBudget-bytes)) {
        // An outstanding read holds its depth frame. Never recycle its storage,
        // or submit another allocation while a copy has unproven completion.
        if(depths.front().use_count()!=1 || !signaled(depths.front()->copyFence)){++telemetryDepthPinned;return;}
        depths.pop_front();
    }
    if(allocatedDepthBytes>depthBudget-bytes){++telemetryDepthBudget;return;}
    GLState saved;
    if(!depthFbo) {
            glGenFramebuffers(1,&depthFbo);glBindFramebuffer(GL_FRAMEBUFFER,depthFbo);
            glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
        }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,depthFbo);glReadBuffer(GL_NONE);
    std::shared_ptr<DepthFrame> next(new DepthFrame(size));next->id=current.id;next->stream=stream;
    glGenTextures(6,next->textures);
    bool valid=true;
    for(unsigned i=0;i<6;i++) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,capture.getFaceDepthTextures()[i],0);
        if(glCheckFramebufferStatus(GL_READ_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){valid=false;break;}
        glBindTexture(GL_TEXTURE_2D,next->textures[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_NONE);
        glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,0,0,size,size,0);
        GLint allocated=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&allocated);
        if(allocated!=(GLint)size || glGetError()!=GL_NO_ERROR){valid=false;break;}
    }
    next->copyFence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);glFlush();
    if(!next->copyFence)valid=false;
    if(valid)depths.push_back(next);else{retiredDepths.push_back(next);++telemetryDepthInvalid;}
#endif
}
void finishFrame(bool published)
{
    finishMissionMeasurementFrame(published);
    if(!active)return;
    if(published)
    {
        publishedFrames.push_back(current);while(publishedFrames.size()>64)publishedFrames.pop_front();
        consumer.addFrame(current);
    }
#ifndef SERVER_ONLY
    else if(!depths.empty() && depths.back()->id==current.id) {
        retiredDepths.push_back(std::move(depths.back()));depths.pop_back();
    }
#endif
}
void measureMissionProjection(unsigned int program,const std::function<void()>& draw)
{
#ifndef SERVER_ONLY
    auto& f=measurement.frame;if(!f.sample||!f.frame.id||f.submitted||f.objects.empty())return;
    pollMissionMeasurement();
    if(measurement.pending.size()>=Measure::MaxPending){
        if(f.record){measurement.reason="measurement_gpu_overflow";measurement.stopRequested=true;}
        // Drop the new sample. Existing bounded queries retain their real completion.
        f.submitted=true;return;
    }
    GLint sampleBuffers=0,counterBits=0,drawBuffers=0,previousProgram=0,previousVAO=0;
    glGetIntegerv(GL_SAMPLE_BUFFERS,&sampleBuffers);glGetQueryiv(GL_SAMPLES_PASSED,GL_QUERY_COUNTER_BITS,&counterBits);glGetIntegerv(GL_MAX_DRAW_BUFFERS,&drawBuffers);glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&previousVAO);
    GLint viewport[4]={0},polygon[2]={0};glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_POLYGON_MODE,polygon);
    if(viewport[0]!=0||viewport[1]!=0||viewport[2]!=GLint(current.projection.width)||viewport[3]!=GLint(current.projection.height)||polygon[0]!=GL_FILL||polygon[1]!=GL_FILL)return;
    const uint64_t pixels=uint64_t(current.projection.width)*current.projection.height;
    if(sampleBuffers!=0||counterBits<1||(counterBits<64&&pixels>=(uint64_t(1)<<counterBits))||drawBuffers<1||drawBuffers>32){
        if(f.record){measurement.reason="measurement_query_capability_unavailable";measurement.stopRequested=true;}return;
    }
    GLint activeQuery=0;glGetQueryiv(GL_SAMPLES_PASSED,GL_CURRENT_QUERY,&activeQuery);if(activeQuery)return;
    // GL 4.1: write masks are per draw buffer; save each, including disabled MRTs.
    std::vector<std::array<GLboolean,4> > masks(drawBuffers);
    std::vector<GLboolean> blends(drawBuffers);
    for(GLint i=0;i<drawBuffers;++i){glGetBooleani_v(GL_COLOR_WRITEMASK,i,masks[i].data());blends[i]=glIsEnabledi(GL_BLEND,i);}
    for(GLint i=0;i<drawBuffers;++i){GLint buffer=0;glGetIntegerv(GL_DRAW_BUFFER0+i,&buffer);if((i==0&&buffer!=GL_COLOR_ATTACHMENT0)||(i>0&&buffer!=GL_NONE))return;}
    GLboolean depthMask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    GLint stencilFront=0,stencilBack=0;glGetIntegerv(GL_STENCIL_WRITEMASK,&stencilFront);glGetIntegerv(GL_STENCIL_BACK_WRITEMASK,&stencilBack);
    const GLboolean depth=glIsEnabled(GL_DEPTH_TEST),stencil=glIsEnabled(GL_STENCIL_TEST),scissor=glIsEnabled(GL_SCISSOR_TEST),cull=glIsEnabled(GL_CULL_FACE),discard=glIsEnabled(GL_RASTERIZER_DISCARD);
    for(GLint i=0;i<drawBuffers;++i)glColorMaski(i,GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
    glDepthMask(GL_FALSE);glStencilMaskSeparate(GL_FRONT,0);glStencilMaskSeparate(GL_BACK,0);
    glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);for(GLint i=0;i<drawBuffers;++i)glDisablei(GL_BLEND,i);glDisable(GL_CULL_FACE);glDisable(GL_RASTERIZER_DISCARD);glUseProgram(program);
    const GLint mode=glGetUniformLocation(program,"scintilla_measure_mode"),index=glGetUniformLocation(program,"scintilla_measure_index");
    for(unsigned i=0;mode>=0&&index>=0&&i<f.objects.size();++i)for(unsigned pass=0;pass<3;++pass){
        GLuint& query=f.objects[i].queries[pass];glGenQueries(1,&query);if(!query)continue;
        glUniform1i(index,i);glUniform1i(mode,pass+1);glBeginQuery(GL_SAMPLES_PASSED,query);draw();glEndQuery(GL_SAMPLES_PASSED);
    }
    glUniform1i(mode,0);glUniform1i(index,0);glUseProgram(previousProgram);glBindVertexArray(previousVAO);
    for(GLint i=0;i<drawBuffers;++i)glColorMaski(i,masks[i][0],masks[i][1],masks[i][2],masks[i][3]);
    glDepthMask(depthMask);glStencilMaskSeparate(GL_FRONT,stencilFront);glStencilMaskSeparate(GL_BACK,stencilBack);
    auto restore=[](GLenum flag,GLboolean enabled){if(enabled)glEnable(flag);else glDisable(flag);};
    restore(GL_DEPTH_TEST,depth);restore(GL_STENCIL_TEST,stencil);restore(GL_SCISSOR_TEST,scissor);for(GLint i=0;i<drawBuffers;++i){if(blends[i])glEnablei(GL_BLEND,i);else glDisablei(GL_BLEND,i);}restore(GL_CULL_FACE,cull);restore(GL_RASTERIZER_DISCARD,discard);
    glFlush();f.submitted=true;measurement.pending.push_back(f);
#else
    (void)program;(void)draw;
#endif
}

namespace {
#ifndef SERVER_ONLY
void bindCrewProjectionUniforms(unsigned int program,const Crew::Result& terminal,bool crewAtlas)
{
    glUniform1i(glGetUniformLocation(program,"scintilla_crew_enabled"),current.crewEnabled?1:0);
    glUniform1i(glGetUniformLocation(program,"scintilla_crew_text_ready"),crewAtlas?1:0);
    glUniform1i(glGetUniformLocation(program,"scintilla_measure_layout_revision"),effectiveLayoutRevision());
    GLfloat badges[20],tasks[20],targetMasks[32]={0};std::fill(badges,badges+20,-1.f);std::fill(tasks,tasks+20,-1.f);
    if(crewAtlas&&terminal.valid){
        // The terminal branch reads only the frozen result. Reuse uniforms
        // whose live target/Party consumers are disabled by the SC4 result.
        for(unsigned i=0;i<terminal.participants.size()&&i<6;++i){const auto& person=terminal.participants[i];
            const auto found=std::find(crewTextParticipantIDs.begin(),crewTextParticipantIDs.end(),person.assignment.participantId);
            const unsigned name=unsigned(found-crewTextParticipantIDs.begin());uint64_t total=0;
            for(uint64_t count:person.contributions){total+=std::min<uint64_t>(1000-total,count);if(total==1000)break;}
            targetMasks[4*i]=name;targetMasks[4*i+1]=std::min<uint64_t>(999,total);targetMasks[4*i+2]=total>=1000?1:0;
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_crew_target_masks[0]"),8,targetMasks);
        glUniform4f(glGetUniformLocation(program,"scintilla_crew_party_state"),terminal.partyEnabled?2:1,
            std::min<uint64_t>(999,terminal.completedHandoffs),std::min<uint64_t>(999,terminal.partyActivations),terminal.participants.size());
        glUniform4f(glGetUniformLocation(program,"scintilla_crew_party_flags"),terminal.comparable?1:0,
            terminal.completedHandoffs>=1000?1:0,terminal.partyActivations>=1000?1:0,2);
        return;
    }
    std::map<std::string,int> slots;std::set<std::string> sourceBound;std::array<bool,5> used={{false,false,false,false,false}};
    for(const auto& a:current.crewAssignments)if(!a.isPilot)for(const auto& r:consumer.reticles())if(r.participantId==a.participantId&&r.slot<5&&!used[r.slot]){slots[a.participantId]=r.slot;used[r.slot]=true;sourceBound.insert(a.participantId);break;}
    for(const auto& a:current.crewAssignments)if(!a.isPilot&&!slots.count(a.participantId))for(unsigned slot=0;slot<5;++slot)if(!used[slot]){slots[a.participantId]=slot;used[slot]=true;break;}
    auto sourceSlot=[&](const std::string& id){const auto it=slots.find(id);return it==slots.end()?-1:it->second;};
    const auto session=RelaySessionController::get()->getSnapshot();
    for(const auto& a:current.crewAssignments)if(!a.isPilot){const int slot=sourceSlot(a.participantId);if(slot<0||slot>=5)continue;
        badges[4*slot]=a.rosterIndex;badges[4*slot+1]=CrewRole+int(a.competence);
        badges[4*slot+2]=session.m_rotation.m_handoff_active?CrewChange:RelaySessionController::get()->crewHandoffReadyRequired()?(crewPreparationBound()?CrewReady:CrewPointer):CrewWait;
        if(!sourceBound.count(a.participantId))badges[4*slot+2]=CrewPointer;
        for(const auto& binding:consumer.bindingStates())if(binding.participantId==a.participantId&&!binding.tracked)badges[4*slot+2]=CrewPointer;
    }
    if(crew.active())for(const auto& hint:current.crewSuggestions){const int slot=sourceSlot(hint.participantId);if(slot<0||slot>=5||hint.expiresAtTick<simulationTick)continue;
        const Target* hinted=nullptr;unsigned targetIndex=0;
        for(unsigned n=0;n<current.targets.size()&&n<32;++n){const auto& t=current.targets[n];
            const bool bound=hint.kind==Crew::TaskKind::Offense?
                t.kind==TargetKind::Rival&&t.combat.instance==hint.instance&&t.combat.window==hint.phaseRevision&&t.combat.eligible:
                t.instance==hint.instance&&t.mission.phaseRevision==hint.phaseRevision&&t.mission.eligible;
            if(t.id==hint.targetId&&bound){hinted=&t;targetIndex=n;break;}
        }
        if((hint.kind==Crew::TaskKind::Offense||hint.kind==Crew::TaskKind::CapsuleShell||hint.kind==Crew::TaskKind::CapsuleCollect||hint.kind==Crew::TaskKind::TetherRecover)&&!hinted)continue;
        if(hint.kind==Crew::TaskKind::Defense&&(!current.defense.enabled||current.defense.threat!=hint.threatId))continue;
        int verb=CrewTask+int(hint.kind),targetCell=-1;
        if(hint.kind==Crew::TaskKind::Offense){
            if(hint.targetId>0&&hint.targetId<=4)targetCell=CrewKart+hint.targetId-1;
            if(!hinted||!Crew::offenseActionCurrent(hint,crewOffenseBundle(hinted->combat),crew.profile().crew))continue;
            switch(hint.offenseAction){
                case Crew::OffenseAction::OpenShield:verb=107;break;
                case Crew::OffenseAction::MaintainShield:verb=109;break;
                case Crew::OffenseAction::WaitForBreach:verb=117;break;
                case Crew::OffenseAction::Fire:verb=108;break;
                case Crew::OffenseAction::ContributionComplete:verb=118;break;
                default:continue;
            }
        }else if(hint.kind==Crew::TaskKind::CapsuleShell||hint.kind==Crew::TaskKind::CapsuleCollect){
            if(hint.targetId>=ScintillaMission::Runtime::CapsuleEntityBase)targetCell=CrewCapsule+(hint.targetId-ScintillaMission::Runtime::CapsuleEntityBase)%24;
        }else if(hint.kind==Crew::TaskKind::TetherRecover)targetCell=CrewTether;
        else if(hint.kind==Crew::TaskKind::Defense)targetCell=103+int(current.defense.sector);
        else targetCell=hint.kind==Crew::TaskKind::NormalBoost?CrewBoost:CrewParty;
        tasks[4*slot]=verb;tasks[4*slot+1]=targetCell;
        // The same participant colour marks the actual rendered instance.
        // No target exists solely to make a suggestion look actionable.
        if(hinted)targetMasks[targetIndex]=float(unsigned(targetMasks[targetIndex])|(1u<<slot));
    }
    GLfloat tools[20]={0};std::array<bool,5> partySource={{false,false,false,false,false}};
    for(const auto& r:consumer.reticles())if(r.slot<5&&sourceSlot(r.participantId)==int(r.slot)){const unsigned slot=r.slot;const auto f=Runtime::partyFeedbackForRendering(r);
        tools[slot*4]=f.progress;tools[slot*4+1]=f.consenting?1:0;tools[slot*4+2]=int(f.reason);
        const bool partyContext=r.intent.context==Defense::Context::PartyBoost||
            (f.participantId==r.participantId&&f.controlEpoch==r.toolEpoch&&f.reason==Crew::PartyReason::ReleaseRequired)||
            (r.valid&&current.layout.classify(r.u,r.v).context==Defense::Context::PartyBoost);
        if(current.partyEnabled&&(partyContext||f.consenting||f.progress>0)){
            partySource[slot]=true;
            tasks[slot*4]=CrewParty;tasks[slot*4+1]=CrewReason+int(f.reason==Crew::PartyReason::Ready?current.crewParty.reason:f.reason);
            if(f.reason==Crew::PartyReason::Ready&&current.crewParty.reason==Crew::PartyReason::Ready&&!f.consenting)tasks[slot*4+1]=109;
            if(f.reason==Crew::PartyReason::ReleaseRequired||f.reason==Crew::PartyReason::ConsensusExpired)badges[slot*4+2]=CrewRelease;
        }
    }
    auto ownerName=[&](int slot,int owner,const std::vector<std::string>& people){
        if(owner<0||size_t(owner)>=people.size())return;
        for(const auto& a:current.crewAssignments)if(a.participantId==people[owner]){badges[slot*4+3]=a.rosterIndex;break;}
    };
    auto missionStatus=[](ScintillaMission::Status status){const int code=int(status);return code==1?int(CrewRelease):code>=2&&code<=9?21+code:code==10?43:-1;};
    // SC5 replaces these old badge pixels, so preserve their actual source
    // feedback explicitly instead of painting names over a rearm instruction.
    if(crew.active())for(const auto& r:consumer.reticles())if(r.slot<5&&sourceSlot(r.participantId)==int(r.slot)&&!partySource[r.slot]){
        const unsigned slot=r.slot;const auto combatTool=Runtime::toolFeedbackForRendering(r);
        const auto missionTool=Runtime::missionFeedbackForRendering(r);
        if(combatTool.status==ScintillaCombat::ToolStatus::ReleaseRequired)badges[slot*4+2]=CrewRelease;
        else if(combatTool.status==ScintillaCombat::ToolStatus::TargetBusy){badges[slot*4+2]=23;ownerName(slot,combatTool.owner,combat.crew());}
        const auto ms=missionTool.status;
        if(ms!=ScintillaMission::Status::None){badges[slot*4+2]=missionStatus(ms);badges[slot*4+3]=-1;
            if(ms==ScintillaMission::Status::TargetBusy)ownerName(slot,missionTool.owner,mission.crew());}
        if(missionTool.outcome!=ScintillaMission::Status::None){tasks[slot*4]=missionStatus(missionTool.outcome);tasks[slot*4+1]=ms==ScintillaMission::Status::ReleaseRequired?CrewRelease:-1;}
        const auto& df=r.defenseFeedback;
        if(r.toolHeld&&df.participant==r.participantId&&df.epoch==r.toolEpoch&&r.intent.context!=Defense::Context::WorldOffense){
            const int cells[]={-1,CrewRelease,23,39,31,32,33,34,CrewReason,35,36,37,38};
            const int code=int(df.status);if(code>0&&code<13){badges[slot*4+2]=cells[code];badges[slot*4+3]=-1;if(code==2)ownerName(slot,df.owner,defense.crew());}
        }
        const auto event=Runtime::feedbackForRendering(r,nowUs());
        if(event.kind!=FeedbackKind::None&&!event.hasPoint){
            const int cells[]={-1,47,40,41,30,44,45,46};const int code=int(event.kind);
            if(code>0&&code<8){tasks[slot*4]=cells[code];tasks[slot*4+1]=badges[slot*4+2]==CrewRelease?CrewRelease:-1;}
        }
    }
    // Urgent advice overrides optional hints/outcomes, never the player's input.
    // Stop colouring the old target as that person's simultaneous assignment.
    if(crew.active()&&current.defense.enabled&&defense.view().phase==Defense::Phase::Telegraph)
        for(const auto& advice:current.crewReleaseAdvice) {
            const int slot=sourceSlot(advice.participantId);
            if(slot<0||slot>=5||advice.expiresAtTick<=simulationTick||advice.threatId!=current.defense.threat||
               advice.threatId!=defense.view().threat||defense.deadline()<=simulationTick)continue;
            tasks[slot*4]=119;tasks[slot*4+1]=103+int(current.defense.sector);
            badges[slot*4+2]=119;badges[slot*4+3]=-1;
            for(unsigned i=0;i<32;++i)targetMasks[i]=float(unsigned(targetMasks[i])&~(1u<<slot));
        }
    if(current.partyEnabled&&current.crewParty.reason!=Crew::PartyReason::Ready){
        for(unsigned slot=0;slot<5;++slot)if(badges[slot*4]>=0&&tasks[slot*4]<0){tasks[slot*4]=CrewParty;tasks[slot*4+1]=CrewReason+int(current.crewParty.reason);break;}
    }
    const auto& p=current.crewParty;const double seconds=std::max<int64_t>(0,p.deadlineTick-simulationTick)/double(std::max(1,stk_config->getPhysicsFPS()));
    glUniform4fv(glGetUniformLocation(program,"scintilla_crew_badges[0]"),5,badges);
    glUniform4fv(glGetUniformLocation(program,"scintilla_crew_tasks[0]"),5,tasks);
    glUniform4fv(glGetUniformLocation(program,"scintilla_crew_target_masks[0]"),8,targetMasks);
    glUniform4fv(glGetUniformLocation(program,"scintilla_crew_tools[0]"),5,tools);
    glUniform4f(glGetUniformLocation(program,"scintilla_crew_party_state"),int(p.state),int(p.reason),seconds,p.consentingParticipantIds.size());
    glUniform4f(glGetUniformLocation(program,"scintilla_crew_party_flags"),p.requiredCount,p.cost,p.usedThisTurn?1:0,current.partyEnabled?1:0);
}
#endif
}
void bindProjectionUniforms(unsigned int program)
{
#ifndef SERVER_ONLY
    GLint previous=0;glGetIntegerv(GL_CURRENT_PROGRAM,&previous);glUseProgram(program);
    // Units 0..14 belong to the scene/capture projection. The result atlas uses
    // the remaining OpenGL 4.1 fragment unit and is restored after the draw.
    finishProjectionUniforms();
    glGetIntegerv(GL_ACTIVE_TEXTURE,&missionSavedActiveTexture);
    glActiveTexture(GL_TEXTURE15);glGetIntegerv(GL_TEXTURE_BINDING_2D,&missionSavedAtlasTexture);
    const auto terminal=current.missionResult.valid?frozenCrewResult():Crew::Result();
    bool terminalNames=terminal.valid&&terminal.participants.size()>=2&&terminal.participants.size()<=6&&
        Crew::matchesMissionResult(terminal,current.missionResult)&&crewTextParticipantIDs.size()==crewTextParticipantNames.size();
    for(const auto& person:terminal.participants){const auto found=std::find(crewTextParticipantIDs.begin(),crewTextParticipantIDs.end(),person.assignment.participantId);
        terminalNames=terminalNames&&found!=crewTextParticipantIDs.end()&&
            crewTextParticipantNames[size_t(found-crewTextParticipantIDs.begin())]==person.assignment.name;}
    const bool crewAtlas=current.crewEnabled&&crewHUDAssetsReady()&&(!current.missionResult.valid||terminalNames);
    auto* text=crewAtlas?crewTextAtlas:missionResultAtlas;
    glBindTexture(GL_TEXTURE_2D,text?text->getTextureHandler():0);
    glActiveTexture(missionSavedActiveTexture);missionAtlasBound=true;
    bindCrewProjectionUniforms(program,terminal,crewAtlas);
    glUniform1i(glGetUniformLocation(program,"scintilla_measure_mode"),0);
    glUniform1i(glGetUniformLocation(program,"scintilla_measure_count"),measurement.frame.objects.size());
    glUniform1i(glGetUniformLocation(program,"scintilla_measure_show"),missionMeasurementActive()&&!measurement.stopRequested?1:0);
    GLfloat measureSpheres[16]={0},measureStyles[16]={0};GLint measureActual[4]={-1,-1,-1,-1};
    for(size_t i=0;i<measurement.frame.objects.size()&&i<4;++i){const auto& target=measurement.frame.objects[i];
        const SpatialVec3 delta(target.center.x-current.cameraPosition.x,target.center.y-current.cameraPosition.y,target.center.z-current.cameraPosition.z);
        measureSpheres[i*4]=dot(delta,current.cameraRight);measureSpheres[i*4+1]=dot(delta,current.cameraUp);measureSpheres[i*4+2]=dot(delta,current.cameraForward);measureSpheres[i*4+3]=target.radius;
        measureStyles[i*4]=target.tether?2:1;measureStyles[i*4+1]=target.tether?(measurement.phase=="shell"?4:5):(measurement.phase=="shell"?0:1);
        measureStyles[i*4+2]=target.range?1:0;measureStyles[i*4+3]=target.rectangle?1:0;
        const uint64_t entity=target.tether?ScintillaMission::Runtime::TetherEntityId:
            ScintillaMission::Runtime::CapsuleEntityBase+std::max(0,measurement.frame.lap)*24+target.group*2+target.capsule;
        for(size_t n=0;n<current.targets.size()&&n<32;++n)if(current.targets[n].id==entity){
            const auto& center=current.targets[n].center;const SpatialVec3 difference(center.x-target.center.x,center.y-target.center.y,center.z-target.center.z);
            if(!target.tether||dot(difference,difference)<1e-10)measureActual[i]=GLint(n);break;
        }
    }
    glUniform1iv(glGetUniformLocation(program,"scintilla_measure_actual_indices[0]"),4,measureActual);
    glUniform4fv(glGetUniformLocation(program,"scintilla_measure_spheres[0]"),4,measureSpheres);
    glUniform4fv(glGetUniformLocation(program,"scintilla_measure_styles[0]"),4,measureStyles);
    glUniform1i(glGetUniformLocation(program,"scintilla_mission_result_text"),15);
    glUniform1i(glGetUniformLocation(program,"scintilla_mission_hud_enabled"),current.missionHUDEnabled?1:0);
    const auto& result=current.missionResult;
    const unsigned outcome=!result.valid?0:!result.natural?5:
        result.outcome=="mission_victory"?1:result.outcome=="race_won_cargo_incomplete"?2:
        result.outcome=="cargo_delivered_race_lost"?3:4;
    glUniform4f(glGetUniformLocation(program,"scintilla_mission_result"),outcome,
        result.natural?result.officialTeamRacePosition:0,result.securedCount,current.missionResultAtlasReady&&missionResultAtlas?1:0);
    GLfloat slots[12]={0},beacons[12]={0},nucleus[12]={0};
    for(unsigned i=0;i<3;++i){
        const auto& slot=current.missionSlots[i];slots[4*i]=int(slot.state);
        slots[4*i+1]=slot.state==ScintillaMission::SlotState::Tethered?current.missionTetherSeconds:0;
        slots[4*i+2]=slot.beaconLit?1:0;slots[4*i+3]=slot.everAcquired?1:0;
        const auto point=[&](const SpatialVec3& p,GLfloat* out,double radius){
            const SpatialVec3 d(p.x-current.cameraPosition.x,p.y-current.cameraPosition.y,p.z-current.cameraPosition.z);
            out[0]=dot(d,current.cameraRight);out[1]=dot(d,current.cameraUp);out[2]=dot(d,current.cameraForward);out[3]=radius;
        };
        point(current.missionBeacons[i],beacons+4*i,current.missionDecorations?.6:0);
        point(current.missionNucleus[i],nucleus+4*i,current.missionDecorations?.12:0);
    }
    glUniform4fv(glGetUniformLocation(program,"scintilla_mission_slots[0]"),3,slots);
    glUniform4fv(glGetUniformLocation(program,"scintilla_mission_beacons[0]"),3,beacons);
    glUniform4fv(glGetUniformLocation(program,"scintilla_mission_nucleus[0]"),3,nucleus);
    glUniform4f(glGetUniformLocation(program,"scintilla_mission_beacon_states"),
        current.missionSlots[0].beaconLit?1:0,current.missionSlots[1].beaconLit?1:0,current.missionSlots[2].beaconLit?1:0,0);
    glUniform1i(glGetUniformLocation(program,"scintilla_enabled"),active?1:0);
    glUniform1i(glGetUniformLocation(program,"scintilla_combat_enabled"),combat.configured()?1:0);
    glUniform1i(glGetUniformLocation(program,"scintilla_defense_enabled"),current.defense.enabled?1:0);
    glUniform1i(glGetUniformLocation(program,"scintilla_layout_enabled"),current.layout.valid()?1:0);
    if(current.layout.valid()){
        const auto&l=current.layout;const auto&v=current.defense;
        glUniform4f(glGetUniformLocation(program,"scintilla_rear_rect"),l.rear[0]/l.width,l.rear[1]/l.height,l.rear[2]/l.width,l.rear[3]/l.height);
        GLfloat nodes[20]={0},badges[20]={0},bars[8]={0},tools3[20]={0};const double size=std::min(l.width,l.height);
        const int sectors[5]={3,0,2,1,4};
        for(unsigned i=0;i<5;++i){double x,y,r;l.node(i,&x,&y,&r);nodes[4*i]=x;nodes[4*i+1]=y;nodes[4*i+2]=r;nodes[4*i+3]=sectors[i];
            badges[4*i]=(.5*l.width+(-.32+.16*i)*size*.5)/l.width;badges[4*i+1]=(.5*l.height+.08*size*.5)/l.height;
            badges[4*i+2]=badges[4*i];badges[4*i+3]=(.5*l.height+.25*size*.5)/l.height;
        }
        for(unsigned i=0;i<2;++i){bars[4*i]=.5-.21*size/l.width;bars[4*i+1]=.5+(.21+.03*i)*size/l.height;bars[4*i+2]=.5+.21*size/l.width;bars[4*i+3]=.5+(.23+.03*i)*size/l.height;}
        for(const auto&r:consumer.reticles())if(r.slot<5){const auto&f=r.defenseFeedback;tools3[4*r.slot]=int(f.status);tools3[4*r.slot+1]=-1;tools3[4*r.slot+2]=int(r.intent.context);tools3[4*r.slot+3]=int(r.intent.sector);
            if(f.owner>=0&&size_t(f.owner)<defense.crew().size())for(const auto&o:consumer.reticles())if(o.participantId==defense.crew()[f.owner])tools3[4*r.slot+1]=o.slot;
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_defense_nodes[0]"),5,nodes);glUniform4fv(glGetUniformLocation(program,"scintilla_defense_badges[0]"),5,badges);glUniform4fv(glGetUniformLocation(program,"scintilla_defense_bars[0]"),2,bars);glUniform4fv(glGetUniformLocation(program,"scintilla_defense_tools[0]"),5,tools3);
        glUniform4f(glGetUniformLocation(program,"scintilla_defense_state"),int(v.phase),int(v.sector),v.progress,v.outcome);
        const auto boost=defense.boostStatus(ScintillaDefensePhysics::eligibleHuman(defenseHuman()));
        glUniform1f(glGetUniformLocation(program,"scintilla_boost_progress"),defense.boostProgress());
        glUniform1f(glGetUniformLocation(program,"scintilla_boost_cost"),defense.profile().q?double(defense.profile().boostCost)/defense.profile().q:tuning.defense.boostCost);
        glUniform4f(glGetUniformLocation(program,"scintilla_defense_resources"),defense.energy(),double(defense.profile().reserve)/defense.profile().q,defense.heat(),int(boost));
        auto bindPoint=[&](const char*name,const SpatialVec3&p,double radius){SpatialVec3 d(p.x-current.cameraPosition.x,p.y-current.cameraPosition.y,p.z-current.cameraPosition.z);glUniform4f(glGetUniformLocation(program,name),dot(d,current.cameraRight),dot(d,current.cameraUp),dot(d,current.cameraForward),radius);};
        const bool threat=v.phase!=Defense::Phase::Idle&&v.phase!=Defense::Phase::Cooldown;
        bindPoint("scintilla_defense_source",current.defenseSource,threat?current.defenseSourceRadius:0);
        bindPoint("scintilla_defense_pulse",SpatialVec3(v.pulse.x,v.pulse.y,v.pulse.z),v.hasPulse?.12:0);
    }
    if(active) {
        const auto cablePoint=[&](const char* name,const SpatialVec3& p,double fourth){
            const SpatialVec3 d(p.x-current.cameraPosition.x,p.y-current.cameraPosition.y,p.z-current.cameraPosition.z);
            glUniform4f(glGetUniformLocation(program,name),dot(d,current.cameraRight),
                dot(d,current.cameraUp),dot(d,current.cameraForward),fourth);
        };
        cablePoint("scintilla_mission_cable_start",current.missionCableStart,current.missionCableRadius);
        cablePoint("scintilla_mission_cable_end",current.missionCableEnd,current.missionCableRadius>0?1:0);
        glUniform1uiv(glGetUniformLocation(program,"scintilla_stamp[0]"),8,stamp.data());
        glUniform2f(glGetUniformLocation(program,"scintilla_near_far"),current.nearPlane,current.farPlane);
        std::vector<GLfloat> targets;
        for(const auto& target:current.targets) {
            SpatialVec3 delta(target.center.x-current.cameraPosition.x,target.center.y-current.cameraPosition.y,target.center.z-current.cameraPosition.z);
            targets.push_back(dot(delta,current.cameraRight));targets.push_back(dot(delta,current.cameraUp));targets.push_back(dot(delta,current.cameraForward));targets.push_back(target.radius);
        }
        glUniform1i(glGetUniformLocation(program,"scintilla_target_count"),current.targets.size());
        if(!targets.empty())glUniform4fv(glGetUniformLocation(program,"scintilla_targets[0]"),current.targets.size(),targets.data());
        GLfloat combatStates[128]={0},combatActors[128]={0},missionTargets[128]={0};
        GLfloat missionOwners[32];std::fill(missionOwners,missionOwners+32,-1.f);
        for(size_t i=0;i<current.targets.size()&&i<32;++i){
            const auto& target=current.targets[i];
            if(target.kind==TargetKind::MissionCapsule||target.kind==TargetKind::MissionTether){
                missionTargets[4*i]=target.kind==TargetKind::MissionCapsule?1:2;
                missionTargets[4*i+1]=int(target.mission.phase);
                missionTargets[4*i+2]=target.mission.progress;
                missionTargets[4*i+3]=target.mission.eligible?1:0;
                if(target.mission.owner>=0&&size_t(target.mission.owner)<mission.crew().size())
                    for(const auto& source:consumer.reticles())if(source.slot<5&&
                        source.participantId==mission.crew()[target.mission.owner])missionOwners[i]=source.slot;
                continue;
            }
            const auto& state=current.targets[i].combat;
            combatStates[4*i]=int(state.phase);combatStates[4*i+1]=state.count;combatStates[4*i+2]=state.required;combatStates[4*i+3]=state.progress;
            int opener=-1;unsigned mask=0;
            const auto& crew=combat.crew();
            for(const auto& source:consumer.reticles())for(size_t person=0;person<crew.size();++person)if(source.participantId==crew[person]&&source.slot<5){
                if(int(person)==state.opener)opener=source.slot;
                if(state.contributors&(1u<<person))mask|=1u<<source.slot;
            }
            combatActors[4*i]=opener;combatActors[4*i+1]=mask;combatActors[4*i+2]=state.eligible?1:0;
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_combat_targets[0]"),32,combatStates);
        glUniform4fv(glGetUniformLocation(program,"scintilla_combat_actors[0]"),32,combatActors);
        glUniform4fv(glGetUniformLocation(program,"scintilla_mission_targets[0]"),32,missionTargets);
        glUniform1fv(glGetUniformLocation(program,"scintilla_mission_owners[0]"),32,missionOwners);
        GLfloat reticles[20]={0};const auto& sources=consumer.reticles();
        // The drawn reticles become frame metadata for the drawn_reticle hit test.
        current.drawnReticles.clear();
        for(const auto& r:sources)if(r.slot<5 && r.valid){reticles[r.slot*4]=r.u;reticles[r.slot*4+1]=r.v;reticles[r.slot*4+2]=1;reticles[r.slot*4+3]=r.toolHeld?2:r.primaryHeld?1:0;
            DrawnReticle drawn;drawn.sourceId=r.sourceId;drawn.u=r.u;drawn.v=r.v;current.drawnReticles.push_back(drawn);}
        glUniform4fv(glGetUniformLocation(program,"scintilla_reticles[0]"),5,reticles);
        GLfloat flashes[20]={0};const int64_t flashNow=nowUs();
        const int64_t flashUs=std::llround(tuning.diagnostics.latencyFlashS*1e6);
        if(tuning.diagnostics.latencyProbe)for(const auto& r:sources)
            if(r.slot<5&&r.presses&&flashNow>=r.pressedUs&&flashNow-r.pressedUs<=flashUs){
                flashes[r.slot*4]=r.pressU;flashes[r.slot*4+1]=r.pressV;flashes[r.slot*4+2]=1;
                flashes[r.slot*4+3]=.06*std::min(current.projection.width,current.projection.height);
                noteLatencyFlash(r);
            }
        glUniform4fv(glGetUniformLocation(program,"scintilla_latency_flash[0]"),5,flashes);
        // Persistent tool status has its own channel: primary feedback cannot
        // replace it. Slots preserve source identity in both projection modes.
        GLfloat tools[20]={0};
        for(const auto& r:sources)if(r.slot<5){
            const auto tool=Runtime::toolFeedbackForRendering(r);
            tools[r.slot*4]=static_cast<int>(tool.status);tools[r.slot*4+1]=-1;
            if(tool.owner>=0&&size_t(tool.owner)<combat.crew().size())
                for(const auto& owner:sources)if(owner.slot<5&&owner.participantId==combat.crew()[tool.owner])tools[r.slot*4+1]=owner.slot;
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_tools[0]"),5,tools);
        GLfloat missionTools[20]={0};
        for(const auto& r:sources)if(r.slot<5){
            const auto tool=Runtime::missionFeedbackForRendering(r);
            missionTools[4*r.slot]=int(tool.status);missionTools[4*r.slot+1]=-1;
            missionTools[4*r.slot+2]=int(tool.outcome);
            if(tool.owner>=0&&size_t(tool.owner)<mission.crew().size())
                for(const auto& owner:sources)if(owner.slot<5&&owner.participantId==mission.crew()[tool.owner])
                    missionTools[4*r.slot+1]=owner.slot;
            for(const auto& target:current.targets)if(target.id==tool.entity&&
                (target.kind==TargetKind::MissionCapsule||target.kind==TargetKind::MissionTether))
                missionTools[4*r.slot+3]=target.mission.progress;
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_mission_tools[0]"),5,missionTools);
        GLfloat feedback[20]={0};const int64_t feedbackNow=nowUs();
        for(const auto& r:sources)if(r.slot<5) {
            const Feedback result=Runtime::feedbackForRendering(r,feedbackNow);
            if(result.kind==FeedbackKind::None)continue;
            const bool spatial=result.hasPoint&&std::isfinite(result.u)&&std::isfinite(result.v)&&
                result.u>=0&&result.u<=1&&result.v>=0&&result.v<=1;
            // A source badge sits inside both supported projection domains. It
            // reports rejection without inventing a spatial hit location.
            feedback[r.slot*4]=spatial?result.u:current.defense.enabled?(.5*current.layout.width+(-.32+.16*r.slot)*std::min(current.layout.width,current.layout.height)*.5)/current.layout.width:0.34+0.08*r.slot;
            feedback[r.slot*4+1]=spatial?result.v:current.defense.enabled?(.5*current.layout.height+.25*std::min(current.layout.width,current.layout.height)*.5)/current.layout.height:0.70;
            feedback[r.slot*4+2]=static_cast<int>(result.kind);
            feedback[r.slot*4+3]=std::max(0.0,std::min(1.0,(result.untilUs-feedbackNow)/100000.0));
        }
        glUniform4fv(glGetUniformLocation(program,"scintilla_feedback[0]"),5,feedback);
    }
    glUseProgram(previous);
#endif
}
void finishProjectionUniforms()
{
#ifndef SERVER_ONLY
    if(!missionAtlasBound)return;
    glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_2D,missionSavedAtlasTexture);
    glActiveTexture(missionSavedActiveTexture);missionAtlasBound=false;
#endif
}
}}
#endif
