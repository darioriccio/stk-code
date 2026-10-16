// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Included once inside the engine's anonymous namespace. No independent owner,
// player, spatial grant, world target or cargo transaction is created here.
namespace Measure=ScintillaMissionMeasurement;
struct MeasurementObject {
    bool tether=false,range=false,rectangle=false,activation=false,inWindow=false;
    int group=-1,capsule=-1,side=-1;SpatialVec3 center;double radius=0;int64_t activationUs=0;
    std::array<unsigned,3> pixels={{0,0,0}};
#ifndef SERVER_ONLY
    std::array<GLuint,3> queries={{0,0,0}};
#endif
    std::string status="unavailable";
};
struct MeasurementFrame {
    Frame frame;uint64_t index=0;int64_t tick=0,readbackUs=0,readbackTick=0;int lap=-1;double fraction=0,speed=0,minimumSpeed=0,maxSpeed=0;
    SpatialVec3 kart;std::array<SpatialVec3,2> candidates;std::array<int,6> flags={{0,0,0,0,0,0}};
    bool active=false,continuous=false,published=false,finished=false,record=false,submitted=false,sample=false;
    std::vector<MeasurementObject> objects;
};
struct MeasurementRecorder {
    bool started=false,closed=false,stopRequested=false,natural=false,wasEnabled=false;
    bool previousKnown=false,previousAnimated=false,continuityBroken=true,inactive=true;int previousLap=-1,finishLap=-1;
    double previousFraction=0,intervalMinimum=200;
    SpatialVec3 previousPosition;
    uint64_t race=0,frames=0;int64_t lastCapturedTick=-1,nextSampleUs=0;
    std::string id,phase="shell",fingerprint,reason,trace,hash,startRequest,crewProfile;
    std::array<int64_t,288> activation={{0}};
    MeasurementFrame frame;
    std::deque<MeasurementFrame> pending;
};
MeasurementRecorder measurement;
std::string effectiveCrewProfile()
{return crew.required()?crew.profileId():measurement.started&&!measurement.closed?measurement.crewProfile:"";}
bool effectiveCrewLayout(){return !effectiveCrewProfile().empty();}
unsigned effectiveLayoutRevision(){return effectiveCrewLayout()?3:2;}
std::string qualificationReportAsset()
{return effectiveCrewLayout()?"immersive_relay/scintilla/qualification_v2/"+effectiveCrewProfile()+"/report.json":"immersive_relay/scintilla/qualification_v1/report.json";}
struct MeasurementCoverage {int64_t captureUs=0,readbackUs=0,tick=0;bool visible=false;};
std::array<MeasurementCoverage,288> measurementCoverage;
std::deque<std::pair<uint64_t,std::array<bool,2> > > measuredTetherFrames;
std::string qualificationFingerprint;
std::string measurementProfileReason;
bool measurementProofsInvalidated=false;

// Texture/context teardown must not call GL or touch cargo. Clear admission
// synchronously; the next GL-safe poll retires the bounded outstanding queries
// before any old result can enter the new resource generation's proof cache.
void invalidateMissionQualificationCache()
{
    const bool wasQualified=!qualificationFingerprint.empty();
    qualificationFingerprint.clear();measurementAssetsHash.clear();
    measurementShaderHash.clear();measurementExecutableHash.clear();
    qualifiedBounds=Measure::Bounds();measurementCoverage.fill(MeasurementCoverage());
    measuredTetherFrames.clear();measurement.frame=MeasurementFrame();
    measurementProofsInvalidated=true;missionAPIPrepared=false;
    missionStepOpen=false;mission.suspendInput();
    if(wasQualified)anchorStatus="preflight_required";
    measurementProfileReason="mission_qualification_preflight_required";
    if(measurement.started&&!measurement.closed){
        if(measurement.reason.empty())measurement.reason="measurement_resources_changed";
        measurement.stopRequested=true;
    }
    if(auto* controller=RelaySessionController::getExisting())
        if(mission.required())controller->setScintillaStartPermitted(false);
}

std::string measurementParent(std::string path){while(path.size()>1&&path.back()=='/')path.pop_back();return StringUtils::getPath(path);}
bool measurementNoSymlink(const std::string& path)
{
#ifndef _WIN32
    for(size_t i=1;i<=path.size();++i)if(i==path.size()||path[i]=='/'){
        struct stat st;if(lstat(path.substr(0,i).c_str(),&st)!=0||S_ISLNK(st.st_mode))return false;}
#endif
    return true;
}
bool measurementFileDigest(const std::string& path,std::string* hash,size_t maximum=128u*1024u*1024u)
{
    std::string bytes;if(!readMissionFile(path,maximum,&bytes))return false;*hash=missionSHA256(bytes);return true;
}
bool measurementAssetTree(const std::string& root,const std::string& relative,unsigned depth,
                          std::map<std::string,std::string>* files,uint64_t* bytes)
{
    if(depth>12||files->size()>32768)return false;
    const std::string dir=root+(relative.empty()?"":"/"+relative);
    std::set<std::string> entries;file_manager->listFiles(entries,dir,false);
    for(const auto& name:entries){if(name=="."||name=="..")continue;if(files->size()>=32768)return false;
        if(name.empty()||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos)return false;
        const std::string rel=relative.empty()?name:relative+"/"+name;
        // Report/trace output is deliberately outside the applied content hash.
        if(rel=="immersive_relay/scintilla/qualification_v1"||rel=="immersive_relay/scintilla/qualification_v2")continue;
        const std::string path=root+"/"+rel;
#ifndef _WIN32
        struct stat state;if(lstat(path.c_str(),&state)!=0||S_ISLNK(state.st_mode))return false;
#endif
        if(FileManager::isDirectory(path)){if(!measurementAssetTree(root,rel,depth+1,files,bytes))return false;}
        else {std::string data;if(!readMissionFile(path,128u*1024u*1024u,&data)){struct stat st;if(stat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size!=0)return false;}
            *bytes+=data.size();if(*bytes>uint64_t(4)*1024*1024*1024)return false;
            (*files)[rel]=missionSHA256(data);}
    }return true;
}
bool loadMeasurementFingerprints(std::string* reason)
{
#ifndef SERVER_ONLY
    auto fail=[&](const char*s){if(reason)*reason=s;return false;};
    auto* track=Track::getCurrentTrack();if(!track)return fail("measurement_track_unavailable");
    const std::string config=file_manager->getAsset("stk_config.xml");
    if(config.empty())return fail("measurement_assets_unavailable");
    const std::string codeRoot=measurementParent(config),trackRoot=measurementParent(measurementParent(track->getTrackFile("")));
    std::string assets;uint64_t total=0;
    for(const auto& root:std::vector<std::string>{codeRoot,trackRoot}){
        std::map<std::string,std::string> files;if(!measurementAssetTree(root,"",0,&files,&total)||files.empty())return fail("measurement_asset_fingerprint_failed");
        for(const auto& file:files)assets+=file.first+"\n"+file.second+"\n";
    }
    measurementAssetsHash=missionSHA256(assets);std::string shaders;
    for(const auto* name:{"immersive_equirectangular.frag","immersive_fisheye_domemaster.frag","utils/immersive_scintilla.frag","utils/immersive_sphere_sampling.frag","immersive_domemaster.vert"}){
        std::string bytes;if(!readMissionFile(file_manager->getAsset(FileManager::SHADER,name),1024*1024,&bytes))return fail("measurement_shader_unavailable");shaders+=name;shaders+='\n';shaders+=bytes;}
    measurementShaderHash=missionSHA256(shaders);
    if(!measurementFileDigest(CommandLine::getExecName(),&measurementExecutableHash,256u*1024u*1024u))return fail("measurement_executable_unavailable");
    if(reason)reason->clear();return true;
#else
    if(reason)*reason="measurement_renderer_unavailable";return false;
#endif
}
std::string measurementFingerprint(const ImmersiveRenderConfig& c)
{
#ifndef SERVER_ONLY
    auto* human=defenseHuman();const auto snapshot=RelaySessionController::get()->getSnapshot();
    if(!human||!Track::getCurrentTrack())return "";
    const double base=human->getKartProperties()->getEngineMaxSpeed();
    // Production renders overlapping 120 degree faces; projection, depth and
    // measurement shaders all use the configured face tangent. Any cubemap
    // face from 90 to under 180 degrees is measurable; the fingerprint pins it.
    if(!std::isfinite(base)||base<=0||base>200||!(c.m_face_fov_deg>=90&&c.m_face_fov_deg<180))return "";
    std::ostringstream o;o<<std::setprecision(17);
    o<<"{\"anchorHash\":"<<ScintillaJSON::quote(anchorHash)<<",\"trackHash\":"<<ScintillaJSON::quote(missionAnchors.trackHash)
     <<",\"trackAssetsHash\":"<<ScintillaJSON::quote(measurementAssetsHash)<<",\"shaderHash\":"<<ScintillaJSON::quote(measurementShaderHash)
     <<",\"executableHash\":"<<ScintillaJSON::quote(measurementExecutableHash)<<",\"profileId\":"<<ScintillaJSON::quote(effectiveCrewLayout()?effectiveCrewProfile():"sc4_cargo_three_slots_v1")<<",\"profileRevision\":\"1\""
     <<",\"P\":"<<snapshot.m_player_count<<",\"C\":"<<missionCrewCount()<<",\"laps\":"<<RaceManager::get()->getNumLaps()<<",\"F\":"<<stk_config->getPhysicsFPS()
     <<",\"width\":"<<c.m_projection_width<<",\"height\":"<<c.m_projection_height<<",\"projection\":"<<ScintillaJSON::quote(projectionKindToString(c.m_projection_mode))
     <<",\"output\":"<<ScintillaJSON::quote(outputModeToString(c.m_output_mode))<<",\"near\":0.20000000000000001,\"far\":"<<Track::getCurrentTrack()->getCameraFar()
     <<",\"faceFovDeg\":"<<c.m_face_fov_deg<<",\"domeTiltDeg\":"<<c.m_dome_tilt_deg<<",\"cameraMode\":"<<ScintillaJSON::quote(cameraModeToString(c.m_camera_mode))
     <<",\"camera\":["<<c.m_yaw_deg<<','<<c.m_pitch_deg<<','<<c.m_roll_deg<<','<<c.m_forward_offset<<','<<c.m_vertical_offset<<','<<c.m_lookahead_offset<<','<<c.m_smoothing_factor<<','<<c.m_roll_limit_deg<<']'
     <<",\"horizonStabilization\":"<<(c.m_horizon_stabilization?"true":"false")<<",\"capsuleRadius\":2,\"tetherRadius\":0.5,\"tetherOffsets\":[[-"<<ScintillaMission::TetherLateralM<<','<<ScintillaMission::TetherHeightM<<','<<ScintillaMission::TetherForwardM<<"],["<<ScintillaMission::TetherLateralM<<','<<ScintillaMission::TetherHeightM<<','<<ScintillaMission::TetherForwardM<<"]],\"evaluatedLayoutRevision\":"<<effectiveLayoutRevision()<<",\"cubemapSize\":"<<c.m_cubemap_size<<",\"nativeBaseSpeed\":"<<base;
    if(effectiveCrewLayout()){
        const int fps=stk_config->getPhysicsFPS();const auto zoneHash=crewPartyZoneManifestHash();
        if(fps<60||fps>1000||!MissionProtocol::hash(zoneHash))return "";
        o<<",\"crewRulesRevision\":\"1\",\"zoneManifestHash\":"<<ScintillaJSON::quote(zoneHash)
         <<",\"partyEnabled\":"<<(effectiveCrewProfile()=="sc5_crew_party_v1"?"true":"false")
         <<",\"partyCost\":15,\"partyGain\":1,\"partyHoldTicks\":"<<(3*fps+3)/4
         <<",\"partyWindowTicks\":"<<3*fps<<",\"partyDurationTicks\":"<<fps/2<<",\"partyFadeTicks\":"<<fps/4<<",\"partyCooldownTicks\":"<<6*fps;
    }
    o<<'}';return o.str();
#else
    (void)c;return "";
#endif
}
std::string measurementGroupsJSON()
{
    std::ostringstream out;out<<std::setprecision(17)<<'[';bool first=true;
    for(const auto& group:missionAnchors.groups){if(!first)out<<',';first=false;
        out<<"{\"id\":"<<ScintillaJSON::quote(group.id)<<",\"activationFraction\":"<<group.activationFraction<<",\"expiryFraction\":"<<group.expiryFraction<<",\"laps\":[";
        for(size_t i=0;i<group.laps.size();++i){if(i)out<<',';out<<group.laps[i];}out<<"],\"capsules\":[";
        for(size_t i=0;i<group.capsules.size();++i){if(i)out<<',';const auto& c=group.capsules[i];out<<"{\"center\":"<<Measure::vecJSON(Measure::Vec(c.center.x,c.center.y,c.center.z))<<",\"radius\":"<<c.radius<<'}';}out<<"]}";
    }out<<']';return out.str();
}
void closeMeasurementTrace()
{
    if(!measurement.started||measurement.closed||!measurement.stopRequested||!measurement.pending.empty())return;
    std::ostringstream footer;footer<<"{\"type\":\"footer\",\"frameCount\":\""<<measurement.frames<<"\",\"complete\":"<<(measurement.reason.empty()?"true":"false")
        <<",\"reason\":"<<ScintillaJSON::quote(measurement.reason)<<",\"naturalFinish\":"<<(measurement.natural?"true":"false")<<",\"officialLap\":"<<measurement.finishLap<<"}\n";
    if(measurement.trace.size()+footer.str().size()<=Measure::MaxBytes)measurement.trace+=footer.str();
    measurement.hash=missionSHA256(measurement.trace);measurement.closed=true;
}
void interruptMissionMeasurement(const std::string& reason)
{
    if(!measurement.started||measurement.closed)return;
    if(measurement.reason.empty())measurement.reason=reason;measurement.stopRequested=true;
#ifndef SERVER_ONLY
    for(auto& f:measurement.pending)for(auto& o:f.objects)for(auto& q:o.queries)if(q){glDeleteQueries(1,&q);q=0;}
#endif
    measurement.pending.clear();closeMeasurementTrace();
}
std::string measurementFrameJSON(const MeasurementFrame& f)
{
    using ScintillaJSON::quote;auto vec=[](const SpatialVec3& v){return Measure::vecJSON(Measure::Vec(v.x,v.y,v.z));};
    bool any=false;for(int n:f.flags)any=any||n>0;
    const std::string scenario=f.flags[2]>0?"nitro":f.flags[0]>0?"zipper":any?"other":"normal";
    std::ostringstream o;o<<std::setprecision(17)<<"{\"type\":\"frame\",\"index\":\""<<f.index<<"\",\"frameID\":\""<<f.frame.id<<"\",\"captureUs\":\""<<f.frame.captureUs
     <<"\",\"readbackUs\":\""<<f.readbackUs<<"\",\"readbackTick\":\""<<f.readbackTick<<"\",\"tick\":\""<<f.tick<<"\",\"officialLap\":"<<f.lap<<",\"fraction\":"<<f.fraction<<",\"speed\":"<<f.speed<<",\"minimumSpeed\":"<<f.minimumSpeed<<",\"maxSpeed\":"<<f.maxSpeed
     <<",\"scenario\":"<<quote(scenario)<<",\"speedIncreaseTicks\":[";for(size_t i=0;i<f.flags.size();++i){if(i)o<<',';o<<f.flags[i];}
    o<<"],\"active\":"<<(f.active?"true":"false")<<",\"continuous\":"<<(f.continuous?"true":"false")<<",\"published\":"<<(f.published?"true":"false")
     <<",\"camera\":{\"position\":"<<vec(f.frame.cameraPosition)<<",\"right\":"<<vec(f.frame.cameraRight)<<",\"up\":"<<vec(f.frame.cameraUp)<<",\"forward\":"<<vec(f.frame.cameraForward)<<"},\"kartPosition\":"<<vec(f.kart)
     <<",\"tetherCandidates\":["<<vec(f.candidates[0])<<','<<vec(f.candidates[1])<<"],\"objects\":[";
    for(size_t i=0;i<f.objects.size();++i){if(i)o<<',';const auto& t=f.objects[i];o<<"{\"kind\":"<<quote(t.tether?"tether":"capsule")<<",\"group\":"<<t.group<<",\"capsule\":"<<t.capsule<<",\"side\":"<<t.side
       <<",\"phase\":"<<quote(t.tether?(measurement.phase=="shell"?"awaiting_presentation":"recoverable"):measurement.phase)<<",\"center\":"<<vec(t.center)<<",\"radius\":"<<t.radius
       <<",\"range\":"<<(t.range?"true":"false")<<",\"rectangleFree\":"<<(t.rectangle?"true":"false")<<",\"activation\":"<<(t.activation?"true":"false")<<",\"activationUs\":\""<<t.activationUs<<"\",\"inWindow\":"<<(t.inWindow?"true":"false")
       <<",\"idealPixels\":"<<t.pixels[0]<<",\"admissiblePixels\":"<<t.pixels[1]<<",\"visiblePixels\":"<<t.pixels[2]<<",\"status\":"<<quote(t.status)<<'}';}o<<"]}\n";return o.str();
}
void pollMissionMeasurement()
{
    if(measurementProofsInvalidated)resetMissionMeasurementProofs();
#ifndef SERVER_ONLY
    while(!measurement.pending.empty()){
        auto& f=measurement.pending.front();if(!f.finished)break;bool ready=true;
        for(auto& o:f.objects)for(GLuint q:o.queries)if(q){GLuint available=0;glGetQueryObjectuiv(q,GL_QUERY_RESULT_AVAILABLE,&available);ready=ready&&available!=0;}
        if(!ready&&nowUs()-f.frame.captureUs<PendingTimeoutUs)break;
        std::array<bool,2> tetherVisible={{false,false}};
        for(auto& o:f.objects){bool complete=true;
            for(unsigned i=0;i<3;++i)if(o.queries[i]){GLuint available=0;glGetQueryObjectuiv(o.queries[i],GL_QUERY_RESULT_AVAILABLE,&available);
                if(available)glGetQueryObjectuiv(o.queries[i],GL_QUERY_RESULT,&o.pixels[i]);else complete=false;
                glDeleteQueries(1,&o.queries[i]);o.queries[i]=0;}else complete=false;
            o.status=complete?"ready":ready?"unavailable":"pending_timeout";
            if(o.tether&&o.side>=0&&o.side<2)tetherVisible[o.side]=f.published&&o.range&&o.rectangle&&o.status=="ready"&&o.pixels[0]>0&&o.pixels[0]==o.pixels[1]&&o.pixels[0]==o.pixels[2];
        }
        // Stamp actual CPU availability AFTER all query reads. The timestamp is
        // intentionally not the GPU completion estimate or the capture time.
        f.readbackUs=nowUs();auto* world=World::getWorld();
        f.readbackTick=world?std::max(0,world->getTimeTicks()):f.tick;
        for(const auto& o:f.objects)if(!o.tether&&f.lap>=0&&f.lap<12){const unsigned key=f.lap*24+o.group*2+o.capsule;
            auto& proof=measurementCoverage[key];proof.captureUs=f.frame.captureUs;proof.readbackUs=f.readbackUs;proof.tick=f.tick;
            proof.visible=f.published&&o.range&&o.rectangle&&o.status=="ready"&&o.pixels[0]>0&&o.pixels[0]==o.pixels[1]&&o.pixels[0]==o.pixels[2];}
        measuredTetherFrames.push_back(std::make_pair(f.frame.id,tetherVisible));while(measuredTetherFrames.size()>64)measuredTetherFrames.pop_front();
        if(f.record){const std::string line=measurementFrameJSON(f);
            if(line.size()>Measure::MaxLine||measurement.trace.size()+line.size()+1024>Measure::MaxBytes){measurement.reason="measurement_trace_overflow";measurement.stopRequested=true;}
            else{measurement.trace+=line;++measurement.frames;}}
        measurement.pending.pop_front();
    }
#endif
    closeMeasurementTrace();
}
void prepareMissionMeasurementFrame(const ImmersiveRenderConfig& config)
{
#ifndef SERVER_ONLY
    measurement.frame=MeasurementFrame();
    const bool record=measurement.started&&!measurement.closed&&!measurement.stopRequested;
    const bool runtime=mission.required()&&anchorStatus=="qualified"&&!missionTerminal();
    if(!record&&!runtime)return;
    const auto fingerprint=measurementFingerprint(config);
    const bool runtimeMismatch=runtime&&(qualificationFingerprint.empty()||fingerprint!=qualificationFingerprint);
    if((record&&measurement.race!=raceEpoch)||runtimeMismatch||(record&&fingerprint!=measurement.fingerprint)){
        interruptMissionMeasurement("measurement_configuration_changed");
        // Runtime queries also exist when no diagnostic trace was ever opened.
        // Retire those proofs and require an explicit coherent reprepare while
        // preserving the race's cargo and immutable qualification receipt.
        resetMissionMeasurementProofs();
        if(runtimeMismatch){missionAPIPrepared=false;mission.suspendInput();
            anchorStatus="preflight_required";measurementProfileReason="mission_qualification_preflight_required";
            RelaySessionController::get()->setScintillaStartPermitted(false);}
        return;
    }
    auto* human=defenseHuman();auto* world=dynamic_cast<LinearWorld*>(World::getWorld());auto* graph=DriveGraph::get();
    if(!human||!world||!graph||graph->getLapLength()<=0)return;
    MeasurementFrame f;f.frame=current;f.frame.targets.clear();f.record=record;f.index=measurement.frames+measurement.pending.size()+1;
    const int64_t sampleNow=nowUs();
    if(!measurement.nextSampleUs)measurement.nextSampleUs=sampleNow;
    f.sample=sampleNow>=measurement.nextSampleUs-2000;
    if(f.sample)do{measurement.nextSampleUs+=33333;}while(measurement.nextSampleUs<=sampleNow);
    f.record=f.record&&f.sample;
    f.tick=std::max(0,world->getTimeTicks());f.lap=world->getLapForKart(human->getWorldKartId());
    f.fraction=world->getDistanceDownTrackForKart(human->getWorldKartId(),true)/graph->getLapLength();
    f.fraction=std::max(0.0,std::min(1.0,f.fraction));f.speed=human->getSpeed();f.maxSpeed=human->getCurrentMaxSpeed();
    f.minimumSpeed=measurement.intervalMinimum==200?std::abs(f.speed):std::min(measurement.intervalMinimum,std::abs(f.speed));
    if(f.sample&&f.tick!=measurement.lastCapturedTick){measurement.intervalMinimum=200;measurement.lastCapturedTick=f.tick;}
    for(unsigned i=0;i<6;++i)f.flags[i]=std::max(0,human->getSpeedIncreaseTicksLeft(i));
    const auto& p=human->getXYZ();f.kart=SpatialVec3(p.x(),p.y(),p.z());
    f.active=world->isRacePhase()&&!RelaySessionController::get()->isWorldHeld()&&!human->hasFinishedRace()&&!human->isEliminated();
    f.continuous=measurement.previousKnown&&!measurement.previousAnimated&&!human->getKartAnimation()&&!measurement.continuityBroken;
    if(f.sample)measurement.continuityBroken=false;
    const double step=2+std::max(std::abs(f.speed),std::abs(f.maxSpeed))/std::max(1,stk_config->getPhysicsFPS());
    const SpatialVec3 delta(f.kart.x-measurement.previousPosition.x,f.kart.y-measurement.previousPosition.y,f.kart.z-measurement.previousPosition.z);
    if(dot(delta,delta)>step*step*64)f.continuous=false;
    const auto pose=human->getSmoothedTrans();
    for(unsigned side=0;side<2;++side){const ::Vec3 q(pose(::Vec3(side?ScintillaMission::TetherLateralM:-ScintillaMission::TetherLateralM,ScintillaMission::TetherHeightM,ScintillaMission::TetherForwardM)));f.candidates[side]=SpatialVec3(q.x(),q.y(),q.z());}
    auto add=[&](MeasurementObject t){const SpatialVec3 d(t.center.x-current.cameraPosition.x,t.center.y-current.cameraPosition.y,t.center.z-current.cameraPosition.z);
        t.range=ScintillaMission::Profile::inRange(t.tether?ScintillaMission::Kind::Tether:ScintillaMission::Kind::Capsule,dot(d,d),t.radius);
        t.rectangle=Measure::footprint(current.projection.kind,current.projection.domeTiltDeg,current.projection.width,current.projection.height,
            Measure::Vec(dot(d,current.cameraRight),dot(d,current.cameraUp),dot(d,current.cameraForward)),t.radius,effectiveLayoutRevision());f.objects.push_back(t);};
    if(f.lap>=0&&f.lap<RaceManager::get()->getNumLaps()){
        unsigned selected=unsigned(missionAnchors.groups.size());double next=2;
        for(unsigned g=0;g<missionAnchors.groups.size();++g){const auto& group=missionAnchors.groups[g];
            if(std::find(group.laps.begin(),group.laps.end(),unsigned(f.lap))==group.laps.end()||f.fraction>=group.expiryFraction)continue;
            if(group.activationFraction<next){next=group.activationFraction;selected=g;}}
        if(selected<missionAnchors.groups.size()){const auto& group=missionAnchors.groups[selected];const unsigned count=missionCrewCount()==1?1:group.capsules.size();
            for(unsigned c=0;c<count;++c){MeasurementObject t;t.group=selected;t.capsule=c;t.center=SpatialVec3(group.capsules[c].center.x,group.capsules[c].center.y,group.capsules[c].center.z);t.radius=group.capsules[c].radius;
                const auto key=f.lap*24+selected*2+c;t.activationUs=measurement.activation[key];t.activation=t.activationUs!=0;if(f.sample)measurement.activation[key]=0;t.inWindow=f.fraction>=group.activationFraction;add(t);}}
        for(unsigned side=0;side<2;++side){MeasurementObject t;t.tether=true;t.side=side;t.center=f.candidates[side];t.radius=.5;t.inWindow=true;add(t);}
    }
    measurement.frame=std::move(f);
#else
    (void)config;
#endif
}
void finishMissionMeasurementFrame(bool published)
{
    auto& f=measurement.frame;if(!f.frame.id||!f.sample)return;
    if(!f.submitted){f.finished=true;f.published=published;measurement.pending.push_back(f);f.submitted=true;}
    for(auto& p:measurement.pending)if(p.frame.id==f.frame.id){p.finished=true;p.published=published;break;}
    if(measurement.started&&measurement.frames+measurement.pending.size()>=Measure::MaxFrames)measurement.stopRequested=true;
    pollMissionMeasurement();
}
bool measurementPlacementPermitted(unsigned group,unsigned capsule,unsigned lap,const Anchors::Capsule& shape)
{
    if(lap>=12||group>=12||capsule>=2||qualificationFingerprint.empty())return false;
    auto* human=defenseHuman();if(!human)return false;
    const unsigned index=lap*24+group*2+capsule;const auto& proof=measurementCoverage[index];
    if(std::abs(double(human->getSpeed()))>Measure::qualifiedEnvelope(qualifiedBounds.capsules[index])||!proof.visible||!Measure::capsuleProofAvailable(proof.captureUs,proof.readbackUs,nowUs()))return false;
    const SpatialVec3 d(shape.center.x-current.cameraPosition.x,shape.center.y-current.cameraPosition.y,shape.center.z-current.cameraPosition.z);
    return ScintillaMission::Profile::inRange(ScintillaMission::Kind::Capsule,dot(d,d),shape.radius)&&Measure::footprint(current.projection.kind,current.projection.domeTiltDeg,current.projection.width,current.projection.height,
        Measure::Vec(dot(d,current.cameraRight),dot(d,current.cameraUp),dot(d,current.cameraForward)),shape.radius,effectiveLayoutRevision());
}
// The macOS build links data/ into the app bundle (CMake POST_BUILD ln -s),
// so the asset root itself may be reached through links. Resolve only that
// root; every directory below it, the report and the traces must still be
// real, unlinked files. Ends with '/'.
std::string measurementEvidenceDirectory()
{
    const std::string relative=qualificationReportAsset(),report=file_manager->getAsset(relative);
    if(report.size()<=relative.size()||report.compare(report.size()-relative.size(),relative.size(),relative)!=0)return "";
    const std::string root=report.substr(0,report.size()-relative.size());
#ifndef _WIN32
    char resolved[PATH_MAX];if(!realpath(root.c_str(),resolved))return "";
    return std::string(resolved)+"/"+StringUtils::getPath(relative)+"/";
#else
    return root+StringUtils::getPath(relative)+"/";
#endif
}
bool loadMissionQualification(std::string* reason)
{
    anchorStatus=missionAnchors.authoringStatus;qualificationFingerprint.clear();qualifiedBounds=Measure::Bounds();
    auto fail=[&](const char*s){measurementProfileReason=s;if(reason)*reason=s;return false;};
    const auto config=ImmersiveRenderPipeline::get()->getConfig();const auto fingerprint=measurementFingerprint(config);
    const std::string evidenceDirectory=measurementEvidenceDirectory(),reportPath=evidenceDirectory.empty()?"":evidenceDirectory+"report.json";
    std::string bytes;if(reportPath.empty()||!measurementNoSymlink(reportPath)||!readMissionFile(reportPath,65536,&bytes))return fail("mission_qualification_evidence_missing");
    try{using namespace ScintillaJSON;const J report=Parser(bytes).parse();closed(report,{"contract","schemaVersion","scope","status","provenance","fingerprintJSON","fingerprintHash","traceFiles","reasons"});
        if(text(report,"contract")!=(effectiveCrewLayout()?"tholus.stk-scintilla-qualification/2":"tholus.stk-scintilla-qualification/1")||num(report,"schemaVersion")!=(effectiveCrewLayout()?2:1)||text(report,"scope")!="native_render_geometry"||text(report,"status")!="qualified_geometry"||text(report,"provenance")!="native_render_depth"||
           text(report,"fingerprintJSON")!=fingerprint||text(report,"fingerprintHash")!=missionSHA256(fingerprint)||!arr(at(report,"reasons"),0,32).empty())return fail("mission_qualification_fingerprint_mismatch");
        Measure::Evidence evidence(missionAnchors,fingerprint);std::set<std::string> paths;uint64_t total=0;
        for(const auto& entry:arr(at(report,"traceFiles"),1,Measure::MaxTraces)){
            closed(entry,{"path","sha256","bytes"});const auto rel=MissionProtocol::path(at(entry,"path"));if(!paths.insert(rel).second)return fail("mission_qualification_duplicate_trace");
            const auto expected=integer(at(entry,"bytes"));if(expected<1||expected>int64_t(Measure::MaxBytes))return fail("mission_qualification_trace_size");total+=expected;if(total>uint64_t(Measure::MaxBytes)*Measure::MaxTraces)return fail("mission_qualification_trace_budget");
            const std::string path=evidenceDirectory+rel;std::string trace;
            if(!measurementNoSymlink(path)||!readMissionFile(path,Measure::MaxBytes,&trace)||trace.size()!=std::size_t(expected)||missionSHA256(trace)!=text(entry,"sha256"))return fail("mission_qualification_trace_hash");
            if(trace.back()!='\n')return fail("mission_qualification_trace_newline");
            std::istringstream lines(trace);std::string line;evidence.beginTrace();unsigned n=0;
            while(std::getline(lines,line)){if(line.empty()||line.size()>Measure::MaxLine)return fail("mission_qualification_trace_line");const J value=Parser(line).parse();const auto type=text(value,"type");
                if(type=="header"){if(n++)return fail("mission_qualification_trace_header");if(text(value,"fingerprintHash")!=missionSHA256(fingerprint))return fail("mission_qualification_trace_fingerprint");evidence.header(value);}
                else if(type=="frame")evidence.frame(value);else if(type=="footer")evidence.footer(value);else return fail("mission_qualification_trace_type");}
            if(!evidence.traceComplete())return fail("mission_qualification_trace_incomplete");
        }
        std::string error;if(!evidence.qualified(&qualifiedBounds,&error)){measurementProfileReason=error;if(reason)*reason=error;return false;}
        anchorStatus="qualified";qualificationFingerprint=fingerprint;measurementProfileReason.clear();if(reason)reason->clear();return true;
    }catch(const std::exception&){return fail("mission_qualification_evidence_invalid");}
}

bool refreshMissionQualification(std::string* reason)
{
    const auto fingerprint=measurementFingerprint(ImmersiveRenderPipeline::get()->getConfig());
    if(!qualificationFingerprint.empty()&&fingerprint==qualificationFingerprint){anchorStatus="qualified";if(reason)reason->clear();return true;}
    anchorStatus=missionAnchors.authoringStatus;
    const auto path=file_manager->getAsset(qualificationReportAsset());
    struct stat reportStat;const bool present=!path.empty()&&stat(path.c_str(),&reportStat)==0&&S_ISREG(reportStat.st_mode)&&reportStat.st_size>0&&reportStat.st_size<=65536;
    measurementProfileReason=present?"mission_qualification_preflight_required":"mission_qualification_evidence_missing";
    if(present)anchorStatus="preflight_required";
    if(reason)*reason=measurementProfileReason;return false;
}

bool missionTetherFullMask(uint64_t frame,int side)
{
    if(side<0||side>1)return false;
    for(auto it=measuredTetherFrames.rbegin();it!=measuredTetherFrames.rend();++it)if(it->first==frame)return it->second[side];
    return false;
}
void resetMissionMeasurementProofs()
{
#ifndef SERVER_ONLY
    for(auto& f:measurement.pending)for(auto& o:f.objects)for(auto& q:o.queries)if(q){glDeleteQueries(1,&q);q=0;}
#endif
    measurement.pending.clear();measurement.frame=MeasurementFrame();measurement.nextSampleUs=0;
    measurementProofsInvalidated=false;
    measurementCoverage.fill(MeasurementCoverage());measuredTetherFrames.clear();
    measurement.previousKnown=false;measurement.continuityBroken=true;measurement.activation.fill(false);
}

void breakMissionMeasurementContinuity()
{
    if(!measurement.inactive)measurement.nextSampleUs=0;
    measurement.inactive=true;measurement.continuityBroken=true;measurement.intervalMinimum=200;measurement.activation.fill(false);
}
