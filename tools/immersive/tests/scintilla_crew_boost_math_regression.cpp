// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
// Pure numerical tests; these do not establish Bullet or physical-brake proof.
#include "modes/immersive_relay/scintilla_crew_boost_math.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';std::abort();}}while(0)
namespace {
using namespace ScintillaCrewBoostMath;
template<class S> std::array<S,3> vector(double x,double y,double z){return {{S(x),S(y),S(z)}};}
template<class S> double dot(const std::array<S,3>& a,const std::array<S,3>& b){
    return double(a[0])*b[0]+double(a[1])*b[1]+double(a[2])*b[2];
}
template<class S> double length(const std::array<S,3>& a){return std::sqrt(dot(a,a));}
template<class S> void counterexample() {
    Result<S> result;const auto v=vector<S>(0,3,4),n=vector<S>(0,1,0);
    CHECK(prepare(v,n,28,28,&result));
    CHECK(std::abs(double(result.velocity[1])-3)<=result.tolerance);
    CHECK(std::abs(double(result.velocity[2])-5)<=result.tolerance);
    CHECK(std::abs(length(result.velocity)-std::sqrt(34.0))<=result.tolerance);
    CHECK(std::abs(result.tangentAfter-result.tangentBefore-1)<=result.tolerance);
    CHECK(result.tolerance<=.001);
}
void partyTangentImpulsePreservesNormalAndMatchesCounterexample(){counterexample<float>();counterexample<double>();}
template<class S> void slowdownCap() {
    Result<S> result;const auto v=vector<S>(0,3,std::sqrt(14*14-9)),n=vector<S>(0,1,0);
    CHECK(prepare(v,n,28,14,&result));CHECK(result.cap==14.5);
    CHECK(length(result.velocity)<=14.5+result.tolerance);
    CHECK(std::abs(length(result.velocity)-14.5)<=result.tolerance);
    CHECK(result.tangentAfter-result.tangentBefore>result.tolerance);
    CHECK(result.tangentAfter-result.tangentBefore<1);
    CHECK(std::abs(double(result.velocity[1])-3)<=result.tolerance);
    CHECK(!prepare(vector<S>(0,30,1),n,28,28,&result));
    CHECK(!prepare(vector<S>(0,0,29),n,28,28,&result));
    CHECK(!prepare(vector<S>(0,0,10),n,28,0,&result));
}
void partyTotalCapUsesCurrentSlowdownAndRejectsNoUsefulImpulse(){slowdownCap<float>();slowdownCap<double>();}
template<class S> void conversions() {
    unsigned admitted=0,rejected=0;
    for(double b:{10.0,28.0,80.0,200.0})for(double factor:{.1,.4,.75,1.0})
    for(double angle:{0.0,.2,.7,1.4})for(double z:{-3.0,0.0,3.0})for(double part:{.1,.5,.95,1.2}) {
        const double current=b*factor,t=current*part;
        const auto normal=vector<S>(0,std::cos(angle),std::sin(angle));
        const auto velocity=vector<S>(0,z*std::cos(angle)-t*std::sin(angle),z*std::sin(angle)+t*std::cos(angle));
        Result<S> result;if(!prepare(velocity,normal,b,current,&result)){++rejected;continue;}++admitted;
        const double beforeNormal=dot(velocity,normal),afterNormal=dot(result.velocity,normal);
        CHECK(std::abs(beforeNormal-afterNormal)<=result.tolerance);
        CHECK(length(result.velocity)<=result.cap+result.tolerance);
        CHECK(result.tangentAfter-result.tangentBefore>result.tolerance);
        CHECK(result.tangentAfter-result.tangentBefore<=1+result.tolerance);
        CHECK(result.tolerance<=.001);
        // The impulse is collinear with the original tangent: no steering.
        std::array<double,3> tangent={{double(velocity[0])-normal[0]*beforeNormal,
            double(velocity[1])-normal[1]*beforeNormal,double(velocity[2])-normal[2]*beforeNormal}};
        std::array<double,3> impulse={{double(result.velocity[0])-velocity[0],
            double(result.velocity[1])-velocity[1],double(result.velocity[2])-velocity[2]}};
        std::array<double,3> cross={{impulse[1]*tangent[2]-impulse[2]*tangent[1],
            impulse[2]*tangent[0]-impulse[0]*tangent[2],impulse[0]*tangent[1]-impulse[1]*tangent[0]}};
        CHECK(length(cross)<=result.tolerance*std::max(1.0,length(tangent)));
    }
    CHECK(admitted>100&&rejected>100);
}
void partyFloatAndDoubleConversionsRespectBoundedImpulse(){conversions<float>();conversions<double>();}
template<class S> void invalid() {
    const auto v=vector<S>(0,0,10),normal=vector<S>(0,1,0);
    Result<S> result;result.velocity=vector<S>(71,72,73);const auto unchanged=result.velocity;
    CHECK(!prepare(v,normal,0,0,&result));CHECK(!prepare(v,normal,-28,28,&result));
    CHECK(!prepare(v,normal,28,-1,&result));CHECK(!prepare(v,normal,28,29,&result));
    CHECK(!prepare(v,normal,INFINITY,28,&result));CHECK(!prepare(v,normal,28,NAN,&result));
    CHECK(!prepare(vector<S>(NAN,0,1),normal,28,28,&result));
    CHECK(!prepare(vector<S>(0,0,INFINITY),normal,28,28,&result));
    CHECK(!prepare(v,vector<S>(0,0,0),28,28,&result));
    CHECK(!prepare(v,vector<S>(0,1.01,0),28,28,&result));
    CHECK(!prepare(vector<S>(0,3,0),normal,28,28,&result));
    CHECK(!prepare(v,normal,28,28,static_cast<Result<S>*>(nullptr)));
    CHECK(result.velocity==unchanged);
    const double tolerance=32*double(std::numeric_limits<S>::epsilon());
    CHECK(prepare(v,vector<S>(0,std::sqrt(1+tolerance/2),0),28,28,&result));
    CHECK(!prepare(v,vector<S>(0,std::sqrt(1+4*tolerance),0),28,28,&result));
}
void partyMalformedNumbersAndNonunitNormalsFailBeforeWriting(){invalid<float>();invalid<double>();}
void partyNumericalToleranceRejectsOutOfScaleInputs() {
    Result<float> f;CHECK(prepare(vector<float>(0,0,10),vector<float>(0,1,0),28,28,&f));
    CHECK(std::abs(f.tolerance-32*double(std::numeric_limits<float>::epsilon())*29)<1e-15);
    CHECK(!prepare(vector<float>(0,0,10),vector<float>(0,1,0),300,300,&f));
    Result<double> d;CHECK(!prepare(vector<double>(0,0,10),vector<double>(0,1,0),1e14,1e14,&d));
    CHECK(!prepare(vector<double>(0,0,1e300),vector<double>(0,1,0),28,28,&d));
}
void partyDurationsUseFloorAndNativeInt16BoundsAtEveryFrequency() {
    for(int fps=60;fps<=1000;++fps){int burst=0,fade=0;CHECK(durations(fps,&burst,&fade));
        CHECK(burst==fps/2&&fade==fps/4&&burst<=500&&fade<=250);
        CHECK(2*burst<=fps&&2*(burst+1)>fps);CHECK(4*fade<=fps&&4*(fade+1)>fps);}
    int burst=42,fade=43;CHECK(!durations(59,&burst,&fade)&&burst==42&&fade==43);
    CHECK(!durations(1001,&burst,&fade));CHECK(!durations(120,nullptr,&fade));CHECK(!durations(120,&burst,nullptr));
}
struct Case{const char*name;void(*run)();};const Case cases[]={
    {"partyTangentImpulsePreservesNormalAndMatchesCounterexample",partyTangentImpulsePreservesNormalAndMatchesCounterexample},
    {"partyTotalCapUsesCurrentSlowdownAndRejectsNoUsefulImpulse",partyTotalCapUsesCurrentSlowdownAndRejectsNoUsefulImpulse},
    {"partyFloatAndDoubleConversionsRespectBoundedImpulse",partyFloatAndDoubleConversionsRespectBoundedImpulse},
    {"partyMalformedNumbersAndNonunitNormalsFailBeforeWriting",partyMalformedNumbersAndNonunitNormalsFailBeforeWriting},
    {"partyNumericalToleranceRejectsOutOfScaleInputs",partyNumericalToleranceRejectsOutOfScaleInputs},
    {"partyDurationsUseFloorAndNativeInt16BoundsAtEveryFrequency",partyDurationsUseFloorAndNativeInt16BoundsAtEveryFrequency}};
}
int main(int argc,char**argv){
    if(argc==2&&std::string(argv[1])=="--list"){for(const auto&c:cases)std::cout<<c.name<<'\n';return 0;}
    if(argc!=1&&(argc!=3||std::string(argv[1])!="--case"))return 2;
    bool found=false;for(const auto&c:cases){if(argc==3&&std::string(argv[2])!=c.name)continue;c.run();found=true;}return found?0:2;
}
