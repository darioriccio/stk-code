// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_defense_collision.hpp"
#include "BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkPairDetector.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btPointCollector.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include <algorithm>
#include <cmath>
namespace ScintillaDefenseCollision {
btTransform interpolate(const btTransform&a,const btTransform&b,btScalar t){return btTransform(a.getRotation().slerp(b.getRotation(),t),a.getOrigin().lerp(b.getOrigin(),t));}
bool socketPathClear(btCollisionWorld&world,const btVector3&from,const btVector3&to,const btCollisionObject*source,const btCollisionObject*human)
{
    if(!std::isfinite(double(from.length2()))||!std::isfinite(double(to.length2()))||(to-from).length2()<1e-12)return false;
    struct Sweep:btCollisionWorld::ClosestConvexResultCallback {
        const btCollisionObject*source,*human;
        Sweep(const btVector3&a,const btVector3&b,const btCollisionObject*s,const btCollisionObject*h):ClosestConvexResultCallback(a,b),source(s),human(h){}
        bool needsCollision(btBroadphaseProxy*p)const override {
            const auto*o=static_cast<btCollisionObject*>(p->m_clientObject);
            return o!=source&&o!=human&&o->hasContactResponse()&&ClosestConvexResultCallback::needsCollision(p);
        }
    } sweep(from,to,source,human);
    struct Contact:btCollisionWorld::ContactResultCallback {
        const btCollisionObject*source,*human;bool blocked=false;
        Contact(const btCollisionObject*s,const btCollisionObject*h):source(s),human(h){}
        bool needsCollision(btBroadphaseProxy*p)const override {
            const auto*o=static_cast<btCollisionObject*>(p->m_clientObject);
            return o!=source&&o!=human&&o->hasContactResponse()&&ContactResultCallback::needsCollision(p);
        }
        btScalar addSingleResult(btManifoldPoint&p,const btCollisionObject*,int,int,const btCollisionObject*,int,int)override {
            if(!std::isfinite(double(p.getDistance()))||p.getDistance()<=0)blocked=true;return 0;
        }
    } contact(source,human);
    // Inflate by 2 mm towards rejection, including the native cast's stop radius.
    btSphereShape sphere(.122);btCollisionObject socket;socket.setCollisionShape(&sphere);
    const btTransform a(btQuaternion::getIdentity(),from),b(btQuaternion::getIdentity(),to);
    for(const auto&pose:{a,b}){socket.setWorldTransform(pose);world.contactTest(&socket,contact);if(contact.blocked)return false;}
    world.convexSweepTest(&sphere,a,b,sweep,0);return !sweep.hasHit();
}
// Validate a real overlap with the FULL projectile after the conservative inner cast.
bool narrowOverlap(const btConvexShape*shape,const btTransform&body,const btTransform&pulse,double guard){
    btSphereShape sphere(.12);btVoronoiSimplexSolver simplex;btGjkEpaPenetrationDepthSolver penetration;
    btGjkPairDetector detector(&sphere,shape,&simplex,&penetration);btGjkPairDetector::ClosestPointInput input;
    input.m_transformA=pulse;input.m_transformB=body;btPointCollector p;detector.getClosestPoints(input,p,nullptr);
    return p.m_hasResult&&std::isfinite(double(p.m_distance))&&p.m_distance<=-guard;
}
bool separatedInterval(const btConvexShape*shape,const btTransform&body,const btVector3&pulse,double radius,double motionBound){
    btSphereShape sphere(radius);btVoronoiSimplexSolver simplex;btGjkEpaPenetrationDepthSolver penetration;
    btGjkPairDetector detector(&sphere,shape,&simplex,&penetration);btGjkPairDetector::ClosestPointInput input;
    input.m_transformA=btTransform(btQuaternion::getIdentity(),pulse);input.m_transformB=body;
    btPointCollector distance;detector.getClosestPoints(input,distance,nullptr);
    return distance.m_hasResult&&std::isfinite(double(distance.m_distance))&&distance.m_distance>motionBound;
}
void castShape(const btCollisionShape*shape,const btTransform&local,const btTransform&from,const btTransform&to,bool human,double epsilon,Result*out,const btVector3&pulse0,const btVector3&pulse1,unsigned depth)
{
    if(!out)return;
    if(!out->valid)return;
    if(!shape||!std::isfinite(double(pulse0.length2()))||!std::isfinite(double(pulse1.length2()))){out->valid=false;return;}
    if(depth>8){out->valid=false;return;}
    if(shape->isCompound()){
        const auto*c=static_cast<const btCompoundShape*>(shape);
        for(int i=0;i<c->getNumChildShapes();++i)castShape(c->getChildShape(i),local*c->getChildTransform(i),from,to,human,epsilon,out,pulse0,pulse1,depth+1);
        return;
    }
    if(!shape->isConvex()){out->valid=false;return;}
    const auto*convex=static_cast<const btConvexShape*>(shape);
    const double rho=local.getOrigin().length();
    const auto qa=from.getRotation(),qb=to.getRotation();
    const double norm=std::sqrt(double(qa.length2())*double(qb.length2()));
    if(!std::isfinite(norm)||norm<1e-12){out->valid=false;return;}
    const double phi=2*std::acos(std::min(1.0,std::abs(double(qa.dot(qb)))/norm));
    unsigned n=1;if(!std::isfinite(rho)){out->valid=false;return;}
    if(rho>.001&&phi>0){const double count=std::ceil(phi/(2*std::acos(std::max(-1.0,1-.001/rho))));if(!std::isfinite(count)||count>16){out->valid=false;return;}n=unsigned(count);}
    n=std::max(1u,n);if(n>16){out->valid=false;return;}
    const double error=rho*(1-std::cos(phi/(2*n))),radius=.12+(human?-1:1)*(error+epsilon);
    if(radius<=0){out->valid=false;return;}
    btSphereShape sphere(radius);
    for(unsigned i=0;i<n;++i){
        const double lo=double(i)/n,hi=double(i+1)/n;
        const btTransform b0=interpolate(from,to,lo)*local,b1=interpolate(from,to,hi)*local;
        btTransform a0(btQuaternion::getIdentity(),pulse0.lerp(pulse1,lo)),a1(btQuaternion::getIdentity(),pulse0.lerp(pulse1,hi));
        // A broadphase overlap need not imply a contact. Prove separation for
        // the ENTIRE interval before treating a failed native cast as uncertain.
        // Distance is 1-Lipschitz under translation; the angular chord bound
        // includes the child offset and every point of its actual convex shape.
        const double mid=(lo+hi)*.5;
        const double translation=((pulse1-pulse0)-(to.getOrigin()-from.getOrigin())).length()*(hi-lo)*.5;
        const double rotation=2*(rho+convex->getAngularMotionDisc())*std::sin(phi*(hi-lo)*.25);
        if(!human&&separatedInterval(convex,interpolate(from,to,mid)*local,pulse0.lerp(pulse1,mid),radius,translation+rotation+epsilon))continue;
        btVoronoiSimplexSolver simplex;btGjkEpaPenetrationDepthSolver penetration;
        btContinuousConvexCollision cast(&sphere,convex,&simplex,&penetration);btConvexCast::CastResult hit;hit.m_fraction=1;
        if(!cast.calcTimeOfImpact(a0,a1,b0,b1,hit)){if(!human){out->valid=false;return;}continue;}
        if(!std::isfinite(double(hit.m_fraction))||hit.m_fraction<0||hit.m_fraction>1){out->valid=false;return;}
        const double fraction=lo+(hi-lo)*hit.m_fraction;
        if(human){
            btTransform realBody=interpolate(from,to,fraction)*local,realPulse(btQuaternion::getIdentity(),pulse0.lerp(pulse1,fraction));
            if(!narrowOverlap(convex,realBody,realPulse,.0005)){out->valid=false;return;}
            out->human=std::min(out->human,fraction);
        }else out->obstacle=std::min(out->obstacle,fraction);
    }
}
}
