// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_BOOST_MATH_HPP
#define HEADER_SCINTILLA_CREW_BOOST_MATH_HPP
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>

// Pure preflight, including the final conversion to the actual Bullet scalar.
// No floor, native speed state, body or controls are modified here.
namespace ScintillaCrewBoostMath {
inline bool durations(int fps,int* burst,int* fade) {
    if(!burst||!fade||fps<60||fps>1000)return false;
    const int b=fps/2,f=fps/4; // maximum durations: floor(.5F), floor(.25F)
    if(b<=0||f<=0||b>32767||f>32767)return false;
    *burst=b;*fade=f;return true;
}
template<class Scalar> struct Result {
    std::array<Scalar,3> velocity={{}};
    double tolerance=0,cap=0,tangentBefore=0,tangentAfter=0;
};
namespace Detail {
using V=std::array<double,3>;
inline double dot(const V& a,const V& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline V subtract(const V&a,const V&b){return {{a[0]-b[0],a[1]-b[1],a[2]-b[2]}};}
inline V scale(const V&a,double f){return {{a[0]*f,a[1]*f,a[2]*f}};}
inline bool finite(const V& a){return std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(a[2]);}
}
template<class Scalar> bool prepare(const std::array<Scalar,3>& velocity,
                                  const std::array<Scalar,3>& normal,
                                  double base,double current,Result<Scalar>* out) {
    static_assert(std::is_floating_point<Scalar>::value,"Bullet scalar must be floating point");
    using namespace Detail;
    if(!out||!std::isfinite(base)||!std::isfinite(current)||base<=0||current<0||current>base)return false;
    const V v={{double(velocity[0]),double(velocity[1]),double(velocity[2])}};
    const V n={{double(normal[0]),double(normal[1]),double(normal[2])}};
    if(!finite(v)||!finite(n))return false;
    const double unitTolerance=32*double(std::numeric_limits<Scalar>::epsilon());
    const double n2=dot(n,n),v2=dot(v,v);
    if(!std::isfinite(n2)||std::abs(n2-1)>unitTolerance||!std::isfinite(v2))return false;
    // Verified against MaxSpeed::update: without any native speed increase,
    // M0=B*f and M1=(B+1)*f. No call to the mutating MaxSpeed::update(0).
    const double cap=(base+1)*(current/base);
    const double epsilon=unitTolerance*std::max({1.0,std::sqrt(v2),cap,base});
    if(!std::isfinite(cap)||!std::isfinite(epsilon)||epsilon>.001)return false;
    const V vn=scale(n,dot(n,v)),vt=subtract(v,vn);
    const double t2=dot(vt,vt),z2=dot(vn,vn),cap2=cap*cap;
    if(!std::isfinite(t2)||!std::isfinite(z2)||!std::isfinite(cap2)||t2<=0)return false;
    const double t=std::sqrt(t2),limit=std::sqrt(std::max(0.0,cap2-z2));
    const double target=std::min(t+1,limit);
    if(!std::isfinite(target)||target<=t+epsilon)return false;
    Result<Scalar> result;const double ratio=target/t;
    V converted;
    for(unsigned axis=0;axis<3;++axis) {
        const double value=vn[axis]+vt[axis]*ratio;
        if(!std::isfinite(value)||std::abs(value)>double(std::numeric_limits<Scalar>::max()))return false;
        result.velocity[axis]=Scalar(value);converted[axis]=double(result.velocity[axis]);
    }
    if(!finite(converted))return false;
    const V normalAfter=scale(n,dot(n,converted));
    const V tangentAfter=subtract(converted,normalAfter);
    const V normalDifference=subtract(normalAfter,vn);
    const double total=std::sqrt(dot(converted,converted));
    const double after=std::sqrt(dot(tangentAfter,tangentAfter));
    const double normalError=std::sqrt(dot(normalDifference,normalDifference));
    if(!std::isfinite(total)||!std::isfinite(after)||!std::isfinite(normalError)||
       total>cap+epsilon||after<=t+epsilon||after-t>1+epsilon||normalError>epsilon)return false;
    result.tolerance=epsilon;result.cap=cap;result.tangentBefore=t;result.tangentAfter=after;
    *out=result;return true;
}
}
#endif
