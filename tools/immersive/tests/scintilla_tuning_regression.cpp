// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Built-in tuning must reproduce the qualified SC0-SC4 profiles exactly; profiles
// are strict documents. Expected values below are the historical literals.
#include "modes/immersive_relay/scintilla_tuning.hpp"
#include "modes/immersive_relay/scintilla_combat.hpp"
#include "modes/immersive_relay/scintilla_defense.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"
#include "modes/immersive_relay/scintilla_mission.hpp"
#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#define CHECK(x) do { if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);} } while(false)
using namespace ScintillaTuning;
namespace {
int64_t ceilTicks(int64_t us,int fps){return (us*fps+999999)/1000000;}
int64_t floorTicks(int64_t us,int fps){return us*fps/1000000;}
std::string profile(const std::string& sections)
{return "{\"contract\":\"tholus.stk-scintilla-tuning/1\",\"profileId\":\"case\""+sections+"}";}
std::string rejection(const std::string& document)
{Tuning t;std::string error;CHECK(!parse(document,&t,&error));CHECK(error.compare(0,7,"tuning:")==0);return error;}

void defaultsReproduceQualifiedCombat()
{
    for(int fps:{60,120,144}){
        ScintillaCombat::Profile legacy,tuned;
        CHECK(ScintillaCombat::Profile::create(fps,&legacy));CHECK(ScintillaCombat::Profile::create(fps,Combat(),&tuned));
        CHECK(tuned.acquire==ceilTicks(600000,fps)&&tuned.breach==floorTicks(3500000,fps));
        CHECK(tuned.cooperation==floorTicks(1250000,fps)&&tuned.soloGap==floorTicks(2500000,fps));
        CHECK(tuned.soloTotal==floorTicks(8000000,fps)&&tuned.stagger==ceilTicks(1000000,fps)&&tuned.recovery==ceilTicks(4000000,fps));
        CHECK(legacy.acquire==tuned.acquire&&legacy.breach==tuned.breach&&legacy.recovery==tuned.recovery&&legacy.stagger==tuned.stagger);
    }
    const double sine=std::sin(0.75*3.14159265358979323846/180.0);
    ScintillaCombat::Profile p;CHECK(ScintillaCombat::Profile::create(120,&p));
    for(double radius:{0.2,0.35,0.8})for(int step=0;step<=4000;++step){
        const double d=step*0.01,d2=d*d;
        const bool expected=d2>radius*radius&&d2<=std::min(900.0,radius*radius/(sine*sine));
        CHECK(ScintillaCombat::Profile::inRange(d2,radius)==expected);CHECK(p.admits(d2,radius)==expected);
    }
}
void defaultsReproduceQualifiedDefense()
{
    for(int fps:{60,120,144})for(unsigned crew=1;crew<=5;++crew){
        ScintillaDefense::Profile p;CHECK(ScintillaDefense::Profile::create(fps,crew,Defense(),&p));
        const int64_t q=1000*int64_t(fps),warning=ceilTicks(crew==1?3000000:2500000,fps),margin=ceilTicks(250000,fps);
        CHECK(p.q==q&&p.warning==warning&&p.hold==ceilTicks(600000,fps)&&p.margin==margin);
        CHECK(p.gap==ceilTicks(8000000,fps)&&p.attackerGap==ceilTicks(12000000,fps)&&p.protection==ceilTicks(5000000,fps));
        CHECK(p.boostAcquire==ceilTicks(500000,fps)&&p.boostCooldown==ceilTicks(6000000,fps)&&p.maxFlight==floorTicks(900000,fps));
        CHECK(p.reserve==5*q+20000*(warning+margin)&&p.admitHeat==100*q-25000*(warning+margin));
        CHECK(p.activation==5*q&&p.boostCost==int64_t(ScintillaDefense::Profile::BoostCost)*q);
        CHECK(p.counterCost==20000&&p.maintainCost==2000&&p.counterHeat==25000&&p.maintainHeat==2000&&p.regen==5000&&p.cooling==20000);
        CHECK(p.technicalRetry==0&&p.boostPolicy==BoostPolicy::Cancel&&!p.inheritVelocity&&p.solveDuringTelegraph);
        CHECK(p.speed==40&&p.minDistance==8&&p.maxDistance==24&&p.impactRetained==0.85);
    }
    // Legacy and profile intercepts agree, and every solution meets the pulse.
    std::mt19937 random(7);std::uniform_real_distribution<double> position(-30,30),speed(-35,35);
    ScintillaDefense::Profile p;CHECK(ScintillaDefense::Profile::create(120,2,&p));unsigned solved=0;
    for(int i=0;i<20000;++i){
        const ScintillaDefense::Vec r(position(random),position(random)*0.2,position(random)),v(speed(random),0,speed(random));
        ScintillaDefense::Ballistic a,b;const bool legacy=ScintillaDefense::Ballistic::solve(r,v,120,&a),tuned=ScintillaDefense::Ballistic::solve(r,v,p,&b);
        CHECK(legacy==tuned);if(!legacy)continue;++solved;
        CHECK(a.seconds==b.seconds&&a.direction.x==b.direction.x&&a.direction.z==b.direction.z);
        CHECK(a.seconds>=double(ceilTicks(350000,120))/120&&a.seconds<=double(floorTicks(900000,120))/120);
        const ScintillaDefense::Vec miss=r+v*a.seconds-a.direction*(40*a.seconds);CHECK(miss.length()<1e-6);
    }
    CHECK(solved>1000);
}
void defaultsReproduceQualifiedMission()
{
    for(int fps:{60,120,144}){
        ScintillaMission::Profile legacy,tuned;
        CHECK(ScintillaMission::Profile::create(fps,&legacy));CHECK(ScintillaMission::Profile::create(fps,Mission(),&tuned));
        CHECK(tuned.collect==ceilTicks(600000,fps)&&tuned.recover==tuned.collect&&tuned.recoveryWindow==floorTicks(3000000,fps));
        CHECK(legacy.collect==tuned.collect&&legacy.recover==tuned.recover&&legacy.recoveryWindow==tuned.recoveryWindow);
    }
}
void builtinDefaultsAreValidAndDocumented()
{
    const Tuning t;std::string error;CHECK(valid(t,&error));CHECK(error.empty());
    std::set<std::string> paths;
    for(const auto& p:parameters(t)){
        CHECK(paths.insert(p.path).second);CHECK(!p.description.empty());
        if(p.unit!="boolean"&&p.unit!="choice"&&p.unit!="list"){
            const double v=std::strtod(p.value.c_str(),nullptr);CHECK(p.minimum<p.maximum&&v>=p.minimum&&v<=p.maximum);
        }
    }
    CHECK(paths.count("combat.markerRadiusM")&&paths.count("defense.boostPolicy")&&paths.count("mission.disabledGroups"));
    const ScintillaJSON::J state=ScintillaJSON::Parser("{\"parameters\":"+parametersJSON(t)+"}").parse();
    CHECK(ScintillaJSON::at(state,"parameters").array.size()==paths.size());
}
void partialProfileOverridesOnlyItsKeys()
{
    Tuning t;std::string error;
    CHECK(parse(profile(",\"description\":\"test\",\"combat\":{\"markerRadiusM\":0.8},\"input\":{\"hitTest\":\"drawn_reticle\",\"beamGraceS\":0.12},"
        "\"defense\":{\"boostPolicy\":\"pause\",\"technicalRetryS\":2},\"mission\":{\"disabledGroups\":[\"snow_supply_2\"]},\"diagnostics\":{\"latencyProbe\":true}"),&t,&error));
    CHECK(error.empty()&&t.profileId=="case"&&t.combat.markerRadiusM==0.8&&t.combat.maxRangeM==30);
    CHECK(t.input.hitTest==HitTest::DrawnReticle&&t.input.beamGraceS==0.12&&t.input.fireIntervalS==0.25);
    CHECK(t.defense.boostPolicy==BoostPolicy::Pause&&t.defense.technicalRetryS==2&&t.defense.gapS==8);
    CHECK(t.mission.disabledGroups.size()==1&&t.mission.disabledGroups[0]=="snow_supply_2"&&t.diagnostics.latencyProbe);
    // The effective document is itself a valid, equivalent profile.
    Tuning again;CHECK(parse(effectiveJSON(t),&again,&error));CHECK(effectiveJSON(again)==effectiveJSON(t));
}
void strictDocumentsAreRejectedWithTheirPath()
{
    CHECK(rejection("{\"profileId\":\"x\"}")=="tuning:contract:missing");
    CHECK(rejection("{\"contract\":\"tholus.stk-scintilla-tuning/2\",\"profileId\":\"x\"}")=="tuning:contract:unsupported");
    CHECK(rejection("{\"contract\":\"tholus.stk-scintilla-tuning/1\"}")=="tuning:profileId:missing");
    CHECK(rejection(profile(",\"weather\":{}"))=="tuning:weather:unknown_section");
    CHECK(rejection(profile(",\"combat\":{\"markerRadius\":1}"))=="tuning:combat.markerRadius:unknown_parameter");
    CHECK(rejection(profile(",\"combat\":{\"markerRadiusM\":3}"))=="tuning:combat.markerRadiusM:out_of_range");
    CHECK(rejection(profile(",\"combat\":{\"markerRadiusM\":\"1\"}"))=="tuning:combat.markerRadiusM:expected_number");
    CHECK(rejection(profile(",\"input\":{\"fireRateFeedback\":1}"))=="tuning:input.fireRateFeedback:expected_boolean");
    CHECK(rejection(profile(",\"input\":{\"hitTest\":\"eyes\"}"))=="tuning:input.hitTest:unknown_choice");
    CHECK(rejection(profile(",\"mission\":{\"disabledGroups\":[\"a\",\"a\"]}"))=="tuning:mission.disabledGroups:duplicate_group");
    CHECK(rejection(profile(",\"mission\":{\"disabledGroups\":[\"../a\"]}"))=="tuning:mission.disabledGroups:invalid_group");
    CHECK(rejection(profile(",\"input\":{\"autoFireIntervalS\":0.1}"))=="tuning:input.autoFireIntervalS:below_fire_interval");
    CHECK(rejection(profile(",\"defense\":{\"minDistanceM\":30}"))=="tuning:defense.minDistanceM:not_below_max");
    CHECK(rejection(profile(",\"defense\":{\"minFlightS\":1}"))=="tuning:defense.minFlightS:not_below_max");
    CHECK(rejection(profile(",\"defense\":{\"counterCostPerS\":40}"))=="tuning:defense.counterCostPerS:reserve_exceeds_capacity");
    CHECK(rejection(profile(",\"defense\":{\"holdS\":2.8}"))=="tuning:defense.holdS:exceeds_telegraph");
    CHECK(rejection(profile(",\"mission\":{\"collectS\":3}"))=="tuning:mission.recoveryWindowS:not_above_hold");
    CHECK(rejection(profile(",\"combat\":{\"markerRadiusM\":1,\"markerRadiusM\":2}"))=="tuning:document:json_duplicate_key");
    CHECK(rejection("[]")=="tuning:document:json_root");
}
void shippedProfilesAreValid()
{
    const char* directory=std::getenv("SCINTILLA_TUNING_DIR");CHECK(directory&&*directory);
    DIR* listing=opendir(directory);CHECK(listing);unsigned count=0;
    while(dirent* entry=readdir(listing)){
        const std::string name=entry->d_name;
        if(name.size()<6||name.compare(name.size()-5,5,".json")!=0)continue;
        std::ifstream file(std::string(directory)+"/"+name,std::ios::binary);std::stringstream bytes;bytes<<file.rdbuf();
        Tuning t;std::string error;if(!parse(bytes.str(),&t,&error))std::cerr<<name<<": "<<error<<std::endl;
        CHECK(error.empty());CHECK(t.profileId+".json"==name);++count;
    }
    closedir(listing);CHECK(count>0);
}
}
int main(int argc,char** argv)
{
    struct Case{const char* name;void(*run)();};
    const Case cases[]={
#define TUNING_CASE(name) {#name,name},
#include "scintilla_tuning_cases.inc"
#undef TUNING_CASE
    };
    const bool selected=argc==3&&std::string(argv[1])=="--case";
    if(argc!=1&&!selected)return 2;
    bool found=false;
    for(const auto& c:cases)if(!selected||c.name==std::string(argv[2])){found=true;std::cout<<c.name<<std::endl;c.run();}
    return found?0:2;
}
