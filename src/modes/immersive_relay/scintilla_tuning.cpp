// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_tuning.hpp"
#include "modes/immersive_relay/scintilla_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <set>
#include <sstream>

namespace ScintillaTuning
{
const char* const Contract="tholus.stk-scintilla-tuning/1";
namespace
{
using ScintillaJSON::J;
struct NumberSpec
{
    const char* section;
    const char* key;
    const char* unit;
    double minimum, maximum;
    const char* description;
    std::function<double&(Tuning&)> field;
};
struct FlagSpec
{
    const char* section;
    const char* key;
    const char* description;
    std::function<bool&(Tuning&)> field;
};

const std::vector<NumberSpec>& numbers()
{
    static const std::vector<NumberSpec> table={
        {"combat","markerRadiusM","m",0.15,2,"Rendered rival marker radius, which is also its hit volume.",[](Tuning& t)->double&{return t.combat.markerRadiusM;}},
        {"combat","maxRangeM","m",5,80,"Farthest attackable rival.",[](Tuning& t)->double&{return t.combat.maxRangeM;}},
        {"combat","minAngularRadiusDeg","deg",0.05,5,"Smallest apparent marker radius that stays attackable.",[](Tuning& t)->double&{return t.combat.minAngularRadiusDeg;}},
        {"combat","acquireS","s",0.05,5,"Opener hold before a shield breach opens.",[](Tuning& t)->double&{return t.combat.acquireS;}},
        {"combat","breachS","s",0.2,15,"Longest open breach.",[](Tuning& t)->double&{return t.combat.breachS;}},
        {"combat","cooperationS","s",0.1,10,"Window for the second shooter once the first has hit (3+ supporters).",[](Tuning& t)->double&{return t.combat.cooperationS;}},
        {"combat","soloGapS","s",0.2,15,"Longest pause between two hits of the solo five-hit chain.",[](Tuning& t)->double&{return t.combat.soloGapS;}},
        {"combat","soloTotalS","s",0.5,60,"Longest solo five-hit chain.",[](Tuning& t)->double&{return t.combat.soloTotalS;}},
        {"combat","staggerS","s",0.1,5,"Rival stagger animation.",[](Tuning& t)->double&{return t.combat.staggerS;}},
        {"combat","recoveryS","s",0.1,30,"Protection of a rival after its stagger.",[](Tuning& t)->double&{return t.combat.recoveryS;}},
        {"input","fireIntervalS","s",0.05,2,"Shortest interval between two shots of one controller.",[](Tuning& t)->double&{return t.input.fireIntervalS;}},
        {"input","autoFireIntervalS","s",0,2,"Automatic fire while primary is held; 0 disables it.",[](Tuning& t)->double&{return t.input.autoFireIntervalS;}},
        {"input","beamGraceS","s",0,0.4,"Tolerated loss of a held beam before it is interrupted.",[](Tuning& t)->double&{return t.input.beamGraceS;}},
        {"defense","telegraphSoloS","s",0.3,10,"Threat warning with one supporter.",[](Tuning& t)->double&{return t.defense.telegraphSoloS;}},
        {"defense","telegraphCrewS","s",0.3,10,"Threat warning with two or more supporters.",[](Tuning& t)->double&{return t.defense.telegraphCrewS;}},
        {"defense","holdS","s",0.05,5,"Sector hold required before the warning ends.",[](Tuning& t)->double&{return t.defense.holdS;}},
        {"defense","marginS","s",0,2,"Energy reserve margin beyond the warning.",[](Tuning& t)->double&{return t.defense.marginS;}},
        {"defense","gapS","s",0.5,120,"Pause after a threat ends.",[](Tuning& t)->double&{return t.defense.gapS;}},
        {"defense","attackerGapS","s",0.5,300,"Pause before the same rival may attack again.",[](Tuning& t)->double&{return t.defense.attackerGapS;}},
        {"defense","technicalRetryS","s",0,120,"Pause after a technical cancellation; 0 uses gapS and attackerGapS.",[](Tuning& t)->double&{return t.defense.technicalRetryS;}},
        {"defense","protectionS","s",0,60,"Protection of the human kart after an impact.",[](Tuning& t)->double&{return t.defense.protectionS;}},
        {"defense","boostAcquireS","s",0.05,5,"Hold on the boost node before the service boost fires.",[](Tuning& t)->double&{return t.defense.boostAcquireS;}},
        {"defense","boostCooldownS","s",0.1,120,"Pause between two service boosts.",[](Tuning& t)->double&{return t.defense.boostCooldownS;}},
        {"defense","boostCost","energy",0,99,"Energy spent by one service boost.",[](Tuning& t)->double&{return t.defense.boostCost;}},
        {"defense","activationCost","energy",0,100,"Energy spent when a sector hold starts.",[](Tuning& t)->double&{return t.defense.activationCost;}},
        {"defense","counterCostPerS","energy/s",0,500,"Energy drain of a hold on the threatened sector.",[](Tuning& t)->double&{return t.defense.counterCostPerS;}},
        {"defense","maintainCostPerS","energy/s",0,500,"Energy drain of any other sector hold.",[](Tuning& t)->double&{return t.defense.maintainCostPerS;}},
        {"defense","counterHeatPerS","heat/s",0,500,"Heat of a hold on the threatened sector.",[](Tuning& t)->double&{return t.defense.counterHeatPerS;}},
        {"defense","maintainHeatPerS","heat/s",0,500,"Heat of any other sector hold.",[](Tuning& t)->double&{return t.defense.maintainHeatPerS;}},
        {"defense","regenPerS","energy/s",0,500,"Energy regeneration without a hold.",[](Tuning& t)->double&{return t.defense.regenPerS;}},
        {"defense","coolingPerS","heat/s",0,500,"Heat dissipation without a hold.",[](Tuning& t)->double&{return t.defense.coolingPerS;}},
        {"defense","minDistanceM","m",1,100,"Nearest admissible attacker.",[](Tuning& t)->double&{return t.defense.minDistanceM;}},
        {"defense","maxDistanceM","m",2,150,"Farthest admissible attacker.",[](Tuning& t)->double&{return t.defense.maxDistanceM;}},
        {"defense","projectileSpeed","m/s",5,200,"Pulse speed, relative to the attacker when inheritAttackerVelocity is set.",[](Tuning& t)->double&{return t.defense.projectileSpeed;}},
        {"defense","minFlightS","s",0.02,5,"Shortest admissible pulse flight.",[](Tuning& t)->double&{return t.defense.minFlightS;}},
        {"defense","maxFlightS","s",0.05,10,"Longest admissible pulse flight.",[](Tuning& t)->double&{return t.defense.maxFlightS;}},
        {"defense","impactSpeedRetained","ratio",0,1,"Tangential speed kept by the human kart after an impact.",[](Tuning& t)->double&{return t.defense.impactSpeedRetained;}},
        {"defense","maxPauseS","s",0,30,"Longest warning pause with boostPolicy pause.",[](Tuning& t)->double&{return t.defense.maxPauseS;}},
        {"mission","collectS","s",0.05,5,"Beam hold that collects an open capsule.",[](Tuning& t)->double&{return t.mission.collectS;}},
        {"mission","recoverS","s",0.05,5,"Beam hold that recovers a tethered module.",[](Tuning& t)->double&{return t.mission.recoverS;}},
        {"mission","recoveryWindowS","s",0.2,30,"Time a detached module stays recoverable.",[](Tuning& t)->double&{return t.mission.recoveryWindowS;}},
        {"diagnostics","latencyFlashS","s",0.02,1,"Duration of the latency probe flash.",[](Tuning& t)->double&{return t.diagnostics.latencyFlashS;}},
    };
    return table;
}
const std::vector<FlagSpec>& flags()
{
    static const std::vector<FlagSpec> table={
        {"input","fireRateFeedback","Show a rejection when a shot exceeds the fire rate.",[](Tuning& t)->bool&{return t.input.fireRateFeedback;}},
        {"defense","inheritAttackerVelocity","Launch the pulse relative to the attacker's velocity.",[](Tuning& t)->bool&{return t.defense.inheritAttackerVelocity;}},
        {"defense","launchSolutionDuringTelegraph","Require an intercept solution on every warning tick, not only at launch.",[](Tuning& t)->bool&{return t.defense.launchSolutionDuringTelegraph;}},
        {"mission","withdrawAboveQualifiedSpeed","Withdraw a capsule while the kart exceeds its qualified speed.",[](Tuning& t)->bool&{return t.mission.withdrawAboveQualifiedSpeed;}},
        {"diagnostics","latencyProbe","Flash the reticle on every primary press for optical latency measurement.",[](Tuning& t)->bool&{return t.diagnostics.latencyProbe;}},
    };
    return table;
}
const char* const Sections[]={"combat","input","defense","mission","diagnostics"};

std::string fail(const std::string& path,const std::string& reason)
{return std::string("tuning:")+path+":"+reason;}

std::string number(double value)
{
    // Shortest decimal that reads back exactly: stable, readable output.
    char buffer[40];
    for(int precision=6;precision<=17;++precision)
    {
        std::snprintf(buffer,sizeof(buffer),"%.*g",precision,value);
        if(std::strtod(buffer,nullptr)==value) break;
    }
    return buffer;
}
bool groupToken(const std::string& value)
{
    if(value.empty()||value.size()>64) return false;
    for(char c:value)
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.')) return false;
    return true;
}
template<class Spec>
const Spec* find(const std::vector<Spec>& table,const std::string& section,const std::string& key)
{
    for(const auto& spec:table) if(section==spec.section&&key==spec.key) return &spec;
    return nullptr;
}
bool parseSection(const std::string& section,const J& object,Tuning* t,std::string* error)
{
    if(object.kind!=J::Object){*error=fail(section,"expected_object");return false;}
    for(const auto& entry:object.object)
    {
        const std::string path=section+"."+entry.first;
        const J& value=entry.second;
        if(const NumberSpec* spec=find(numbers(),section,entry.first))
        {
            if(value.kind!=J::Number||!std::isfinite(value.number)){*error=fail(path,"expected_number");return false;}
            if(value.number<spec->minimum||value.number>spec->maximum){*error=fail(path,"out_of_range");return false;}
            spec->field(*t)=value.number;continue;
        }
        if(const FlagSpec* spec=find(flags(),section,entry.first))
        {
            if(value.kind!=J::Bool){*error=fail(path,"expected_boolean");return false;}
            spec->field(*t)=value.boolean;continue;
        }
        if(path=="input.hitTest"||path=="defense.boostPolicy")
        {
            if(value.kind!=J::String){*error=fail(path,"expected_string");return false;}
            const std::string& v=value.string;
            if(path=="input.hitTest")
            {
                if(v=="pose") t->input.hitTest=HitTest::Pose;
                else if(v=="drawn_reticle") t->input.hitTest=HitTest::DrawnReticle;
                else {*error=fail(path,"unknown_choice");return false;}
            }
            else if(v=="cancel") t->defense.boostPolicy=BoostPolicy::Cancel;
            else if(v=="pause") t->defense.boostPolicy=BoostPolicy::Pause;
            else if(v=="ignore") t->defense.boostPolicy=BoostPolicy::Ignore;
            else {*error=fail(path,"unknown_choice");return false;}
            continue;
        }
        if(path=="mission.disabledGroups")
        {
            if(value.kind!=J::Array||value.array.size()>32){*error=fail(path,"expected_array");return false;}
            std::set<std::string> seen;t->mission.disabledGroups.clear();
            for(const J& group:value.array)
            {
                if(group.kind!=J::String||!groupToken(group.string)){*error=fail(path,"invalid_group");return false;}
                if(!seen.insert(group.string).second){*error=fail(path,"duplicate_group");return false;}
                t->mission.disabledGroups.push_back(group.string);
            }
            continue;
        }
        *error=fail(path,"unknown_parameter");return false;
    }
    return true;
}
}

const char* name(BoostPolicy policy)
{return policy==BoostPolicy::Pause?"pause":policy==BoostPolicy::Ignore?"ignore":"cancel";}
const char* name(HitTest mode)
{return mode==HitTest::DrawnReticle?"drawn_reticle":"pose";}

bool valid(const Tuning& t,std::string* error)
{
    std::string ignored;if(!error)error=&ignored;
    const Input& i=t.input;const Defense& d=t.defense;const Mission& m=t.mission;
    if(i.autoFireIntervalS!=0&&i.autoFireIntervalS<i.fireIntervalS)
    {*error=fail("input.autoFireIntervalS","below_fire_interval");return false;}
    if(d.minDistanceM>=d.maxDistanceM){*error=fail("defense.minDistanceM","not_below_max");return false;}
    if(d.minFlightS>=d.maxFlightS){*error=fail("defense.minFlightS","not_below_max");return false;}
    const double longest=std::max(d.telegraphSoloS,d.telegraphCrewS)+d.marginS;
    const double reserve=d.activationCost+d.counterCostPerS*longest;
    if(d.holdS>std::min(d.telegraphSoloS,d.telegraphCrewS)){*error=fail("defense.holdS","exceeds_telegraph");return false;}
    if(reserve>100){*error=fail("defense.counterCostPerS","reserve_exceeds_capacity");return false;}
    if(d.counterHeatPerS*longest>100){*error=fail("defense.counterHeatPerS","heat_exceeds_capacity");return false;}
    if(reserve+d.boostCost>100){*error=fail("defense.boostCost","unreachable");return false;}
    if(m.collectS>=m.recoveryWindowS||m.recoverS>=m.recoveryWindowS)
    {*error=fail("mission.recoveryWindowS","not_above_hold");return false;}
    return true;
}

bool parse(const std::string& bytes,Tuning* out,std::string* error)
{
    std::string ignored;if(!error)error=&ignored;
    if(!out){*error=fail("document","no_output");return false;}
    Tuning t;
    try
    {
        const J root=ScintillaJSON::Parser(bytes).parse();
        for(const auto& entry:root.object)
        {
            const std::string& key=entry.first;
            if(key=="contract")
            {
                if(entry.second.kind!=J::String||entry.second.string!=Contract){*error=fail("contract","unsupported");return false;}
            }
            else if(key=="profileId")
            {
                if(entry.second.kind!=J::String||!groupToken(entry.second.string)){*error=fail("profileId","invalid_token");return false;}
                t.profileId=entry.second.string;
            }
            else if(key=="description")
            {
                if(entry.second.kind!=J::String||entry.second.string.size()>1024){*error=fail("description","invalid_string");return false;}
            }
            else if(std::find(std::begin(Sections),std::end(Sections),key)!=std::end(Sections))
            {
                if(!parseSection(key,entry.second,&t,error)) return false;
            }
            else {*error=fail(key,"unknown_section");return false;}
        }
        if(!root.object.count("contract")){*error=fail("contract","missing");return false;}
        if(!root.object.count("profileId")){*error=fail("profileId","missing");return false;}
    }
    catch(const std::exception& e){*error=fail("document",e.what());return false;}
    if(!valid(t,error)) return false;
    *out=t;return true;
}

std::vector<Parameter> parameters(const Tuning& source)
{
    Tuning t=source;std::vector<Parameter> out;
    for(const auto& spec:numbers())
    {
        Parameter p;p.path=std::string(spec.section)+"."+spec.key;p.unit=spec.unit;p.description=spec.description;
        p.minimum=spec.minimum;p.maximum=spec.maximum;p.value=number(spec.field(t));out.push_back(p);
    }
    for(const auto& spec:flags())
    {
        Parameter p;p.path=std::string(spec.section)+"."+spec.key;p.unit="boolean";p.description=spec.description;
        p.maximum=1;p.value=spec.field(t)?"true":"false";out.push_back(p);
    }
    Parameter hit;hit.path="input.hitTest";hit.unit="choice";hit.value=name(t.input.hitTest);
    hit.description="Shot point: pose at the trigger, or the reticle drawn in the presented frame.";
    hit.choices={"pose","drawn_reticle"};out.push_back(hit);
    Parameter boost;boost.path="defense.boostPolicy";boost.unit="choice";boost.value=name(t.defense.boostPolicy);
    boost.description="Effect of a human speed boost or jump on an announced threat.";
    boost.choices={"cancel","pause","ignore"};out.push_back(boost);
    Parameter groups;groups.path="mission.disabledGroups";groups.unit="list";
    groups.description="Anchor groups that never offer capsules.";
    std::string list="[";
    for(size_t n=0;n<t.mission.disabledGroups.size();++n) list+=(n?",":"")+ScintillaJSON::quote(t.mission.disabledGroups[n]);
    groups.value=list+"]";out.push_back(groups);
    return out;
}

std::string effectiveJSON(const Tuning& t)
{
    std::ostringstream o;
    o<<"{\"contract\":\""<<Contract<<"\",\"profileId\":"<<ScintillaJSON::quote(t.profileId);
    for(const char* section:Sections)
    {
        o<<",\""<<section<<"\":{";bool first=true;
        for(const auto& p:parameters(t))
        {
            const std::string prefix=std::string(section)+".";
            if(p.path.compare(0,prefix.size(),prefix)!=0) continue;
            const std::string key=p.path.substr(prefix.size());
            o<<(first?"":",")<<ScintillaJSON::quote(key)<<":";first=false;
            if(p.unit=="choice") o<<ScintillaJSON::quote(p.value);else o<<p.value;
        }
        o<<"}";
    }
    o<<"}";return o.str();
}

std::string parametersJSON(const Tuning& t)
{
    const Tuning defaults;const std::vector<Parameter> base=parameters(defaults);
    std::ostringstream o;o<<"[";
    const std::vector<Parameter> current=parameters(t);
    for(size_t n=0;n<current.size();++n)
    {
        const Parameter& p=current[n];const bool text=p.unit=="choice";
        o<<(n?",":"")<<"{\"path\":"<<ScintillaJSON::quote(p.path)<<",\"unit\":"<<ScintillaJSON::quote(p.unit)
         <<",\"value\":"<<(text?ScintillaJSON::quote(p.value):p.value)
         <<",\"default\":"<<(text?ScintillaJSON::quote(base[n].value):base[n].value);
        if(p.unit!="boolean"&&p.unit!="choice"&&p.unit!="list") o<<",\"minimum\":"<<number(p.minimum)<<",\"maximum\":"<<number(p.maximum);
        if(!p.choices.empty())
        {
            o<<",\"choices\":[";
            for(size_t c=0;c<p.choices.size();++c) o<<(c?",":"")<<ScintillaJSON::quote(p.choices[c]);
            o<<"]";
        }
        o<<",\"description\":"<<ScintillaJSON::quote(p.description)<<"}";
    }
    o<<"]";return o.str();
}
}
