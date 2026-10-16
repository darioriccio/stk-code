// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/scintilla_defense_physics.hpp"
#include "karts/abstract_kart.hpp"
#include "modes/immersive_relay/scintilla_defense_collision.hpp"
#include "karts/controller/controller.hpp"
#include "karts/max_speed.hpp"
#include "physics/physics.hpp"
#include "physics/stk_dynamics_world.hpp"
#include "physics/btKart.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>
namespace ScintillaDefensePhysics {
using namespace ScintillaDefense;
namespace {
Vec cv(const btVector3&v){return Vec(v.x(),v.y(),v.z());}
btVector3 bv(const Vec&v){return btVector3(v.x,v.y,v.z);}
bool finite(const btVector3&v){return cv(v).finite();}
// Cache bounds while the shape is known alive. A shape can be replaced during
// the native step; never dereference the saved pointer afterwards.
struct ShapeStamp {const btCollisionShape*shape;btTransform local;btVector3 scaling;btScalar margin;};
bool shapeState(const btCollisionShape*shape,const btTransform&local,std::vector<ShapeStamp>*out,unsigned depth=0){
    if(!shape||depth>8||out->size()>=64)return false;
    ShapeStamp s;s.shape=shape;s.local=local;s.scaling=shape->getLocalScaling();s.margin=shape->getMargin();out->push_back(s);
    if(shape->isCompound()){const auto*c=static_cast<const btCompoundShape*>(shape);for(int i=0;i<c->getNumChildShapes();++i)if(!shapeState(c->getChildShape(i),local*c->getChildTransform(i),out,depth+1))return false;}
    return true;
}
bool sameShapes(const std::vector<ShapeStamp>&a,const std::vector<ShapeStamp>&b){
    if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i){if(a[i].shape!=b[i].shape||a[i].scaling!=b[i].scaling||a[i].margin!=b[i].margin||a[i].local.getOrigin()!=b[i].local.getOrigin())return false;for(int axis=0;axis<3;++axis)if(a[i].local.getBasis().getColumn(axis)!=b[i].local.getBasis().getColumn(axis))return false;}return true;
}
struct Body {btTransform transform;const btCollisionShape* shape=nullptr;btVector3 lower,upper;btScalar disc=0;std::vector<ShapeStamp> shapes;};
std::map<const btCollisionObject*,Body> bodies;
const btCollisionObject* humanBody=nullptr;const btCollisionObject* sourceBody=nullptr;
btVector3 pulse0,pulse1,humanUp;bool armed=false;Trace result;
bool noBoost(AbstractKart*k){for(unsigned i=0;i<MaxSpeed::MS_INCREASE_MAX;++i)if(k->isSpeedIncreaseActive(i))return false;return true;}
bool overlap(const btVector3&alo,const btVector3&ahi,const btVector3&blo,const btVector3&bhi){for(int i=0;i<3;i++)if(ahi[i]<blo[i]||bhi[i]<alo[i])return false;return true;}
struct StaticCast:btCollisionWorld::ClosestConvexResultCallback {
    StaticCast(const btVector3&a,const btVector3&b):ClosestConvexResultCallback(a,b){}
    bool needsCollision(btBroadphaseProxy*p)const override{
        auto*o=static_cast<btCollisionObject*>(p->m_clientObject);return o->isStaticObject()&&o->hasContactResponse()&&ClosestConvexResultCallback::needsCollision(p);
    }
};

}
void clear(){bodies.clear();armed=false;result=Trace();humanBody=sourceBody=nullptr;}
bool eligibleHuman(AbstractKart*k){return k&&k->getBody()&&!k->isEliminated()&&!k->hasFinishedRace()&&!k->getKartAnimation()&&k->isOnGround()&&k->getSpeed()>=0&&!k->isInvulnerable()&&noBoost(k);}
bool cameraLineClear(const Vec&camera,AbstractKart*human,AbstractKart*source)
{
    if(!camera.finite()||!source||!source->getBody())return false;
    struct Ray:btCollisionWorld::ClosestRayResultCallback{
        const btCollisionObject*human,*source;
        Ray(const btVector3&a,const btVector3&b,const btCollisionObject*h,const btCollisionObject*s):ClosestRayResultCallback(a,b),human(h),source(s){}
        bool needsCollision(btBroadphaseProxy*p)const override{const auto*o=static_cast<btCollisionObject*>(p->m_clientObject);return o!=human&&o!=source&&o->hasContactResponse()&&ClosestRayResultCallback::needsCollision(p);}
    } ray(bv(camera),source->getBody()->getWorldTransform().getOrigin(),human?human->getBody():nullptr,source->getBody());
    Physics::get()->getPhysicsWorld()->rayTest(ray.m_rayFromWorld,ray.m_rayToWorld,ray);return !ray.hasHit();
}
bool opportunity(AbstractKart*human,AbstractKart*source,Sector retained,Candidate*out,Ballistic*solution,int fps,bool requireLaunchSolution)
{
    if(!out||!solution||!eligibleHuman(human)||!source||source==human||!source->getBody()||!source->getController()||source->getController()->isPlayerController()||source->isEliminated()||source->hasFinishedRace()||source->getKartAnimation()||source->isInvulnerable()||source->isShielded())return false;
    auto*world=Physics::get()->getPhysicsWorld();const btVector3 h=human->getBody()->getWorldTransform().getOrigin(),a=source->getBody()->getWorldTransform().getOrigin();
    Vec delta=cv(a-h),up=cv(-human->getBody()->getGravity()),forward=cv(human->getBody()->getWorldTransform().getBasis().getColumn(2));
    const double distance=delta.length();if(distance<8||distance>24)return false;
    Sector sector=Runtime::sector(delta,forward,up,retained);if(sector==Sector::None)return false;
    Vec origin=cv(a);Ballistic b;
    // Once launched, eligibility concerns this live threat. Requiring a NEW
    // intercept in the launch window would cancel a valid pulse near contact.
    if(!requireLaunchSolution){
        if(!cameraLineClear(cv(h),human,source))return false;
        out->sector=sector;out->origin=origin;out->velocity=cv(human->getBody()->getLinearVelocity());out->distance=distance;return true;
    }
    // Bounding support is derived from the actual compound AABB in source local space.
    btVector3 lower,upper;source->getBody()->getCollisionShape()->getAabb(btTransform::getIdentity(),lower,upper);
    btVector3 extent=lower.absolute();extent.setMax(upper.absolute());const double radius=extent.length()+.15;
    if(!Ballistic::solve(cv(h)-origin,cv(human->getBody()->getLinearVelocity()),fps,&b))return false;
    origin=origin+b.direction*radius;
    if(!ScintillaDefenseCollision::socketPathClear(*world,a,bv(origin),source->getBody(),human->getBody()))return false;
    // Line of launch may leave the source hull; world ray ignores only the source/human.
    struct Ray:btCollisionWorld::ClosestRayResultCallback{
        const btCollisionObject*s,*h;
        Ray(const btVector3&a,const btVector3&b,const btCollisionObject*ss,const btCollisionObject*hh):ClosestRayResultCallback(a,b),s(ss),h(hh){}
        bool needsCollision(btBroadphaseProxy*p)const override{const auto*o=static_cast<btCollisionObject*>(p->m_clientObject);return o!=s&&o!=h&&o->hasContactResponse()&&ClosestRayResultCallback::needsCollision(p);}
    } ray(a,h,source->getBody(),human->getBody());
    world->rayTest(a,h,ray);if(ray.hasHit())return false;
    if(!Ballistic::solve(cv(h)-origin,cv(human->getBody()->getLinearVelocity()),fps,&b))return false;
    out->sector=sector;out->origin=origin;out->velocity=cv(human->getBody()->getLinearVelocity());out->distance=distance;*solution=b;return true;
}
void before(AbstractKart*human,AbstractKart*source,const Vec&pulse,const Vec&velocity,int fps)
{
    clear();result.available=true;if(!eligibleHuman(human)||!source||!source->getBody()||!pulse.finite()||!velocity.finite()||fps<60||fps>1000)return;
    humanBody=human->getBody();sourceBody=source->getBody();humanUp=-human->getBody()->getGravity();pulse0=bv(pulse);pulse1=bv(pulse+velocity*(1.0/fps));
    auto*world=Physics::get()->getPhysicsWorld();
    for(int i=0;i<world->getNumCollisionObjects();++i){const auto*o=world->getCollisionObjectArray()[i];if(o->isStaticObject()||!o->hasContactResponse())continue;if(bodies.size()>=512)return;Body b;b.transform=o->getWorldTransform();b.shape=o->getCollisionShape();if(!shapeState(b.shape,btTransform::getIdentity(),&b.shapes))return;b.shape->getAabb(b.transform,b.lower,b.upper);b.disc=b.shape->getAngularMotionDisc();bodies[o]=b;}
    armed=true;
}
void after()
{
    if(!armed)return;armed=false;result.available=true;result.point=cv(pulse1);
    auto*world=Physics::get()->getPhysicsWorld();
    const double coordinate=std::max(pulse0.absolute().maxAxis()>=0?double(pulse0.absolute()[pulse0.absolute().maxAxis()]):0.0,double(pulse1.absolute()[pulse1.absolute().maxAxis()]));
    // 2 mm covers Bullet's 1 mm stop radius; scale guard accounts for btScalar precision.
    const double epsilon=.002+64*std::numeric_limits<btScalar>::epsilon()*std::max(1.0,coordinate);
    if(!std::isfinite(epsilon)||epsilon>.02)return;
    btSphereShape sphere(.12+epsilon);StaticCast wall(pulse0,pulse1);
    btTransform a0(btQuaternion::getIdentity(),pulse0),a1(btQuaternion::getIdentity(),pulse1);
    world->convexSweepTest(&sphere,a0,a1,wall,0);
    ScintillaDefenseCollision::Result contact;if(wall.hasHit())contact.obstacle=wall.m_closestHitFraction;
    btVector3 pmin=pulse0,pmax=pulse0;pmin.setMin(pulse1);pmax.setMax(pulse1);pmin-=btVector3(.15,.15,.15);pmax+=btVector3(.15,.15,.15);
    bool humanPresent=false;btTransform humanAfter;
    for(int i=0;i<world->getNumCollisionObjects();++i){
        const auto*o=world->getCollisionObjectArray()[i];if(o==humanBody){humanPresent=true;humanAfter=o->getWorldTransform();}
        if(o==sourceBody||o->isStaticObject()||!o->hasContactResponse())continue;
        auto old=bodies.find(o);btVector3 lo,hi;const auto*shape=o->getCollisionShape();if(!shape)return;shape->getAabb(o->getWorldTransform(),lo,hi);
        if(old==bodies.end()){if(overlap(pmin,pmax,lo,hi))return;continue;}
        lo.setMin(old->second.lower);hi.setMax(old->second.upper);
        // Full angular-motion-disc broadphase includes rotational intermediate poses.
        const double disc=std::max(old->second.disc,shape->getAngularMotionDisc());
        btVector3 originMin=old->second.transform.getOrigin(),originMax=originMin;originMin.setMin(o->getWorldTransform().getOrigin());originMax.setMax(o->getWorldTransform().getOrigin());
        lo.setMin(originMin-btVector3(disc,disc,disc));hi.setMax(originMax+btVector3(disc,disc,disc));
        if(!overlap(pmin,pmax,lo,hi))continue;
        std::vector<ShapeStamp> shapes;
        if(old->second.shape!=shape||!shapeState(shape,btTransform::getIdentity(),&shapes)||!sameShapes(old->second.shapes,shapes)||!finite(o->getWorldTransform().getOrigin())||(old->second.transform.getOrigin()-o->getWorldTransform().getOrigin()).length()>3)return;
        ScintillaDefenseCollision::castShape(shape,btTransform::getIdentity(),old->second.transform,o->getWorldTransform(),o==humanBody,epsilon,&contact,pulse0,pulse1);
        if(!contact.valid)return;
    }
    if(!humanPresent||!bodies.count(humanBody)||!finite(humanUp))return;
    result.valid=true;
    const double first=std::min(contact.human,contact.obstacle);
    if(first<=1){result.hit=true;result.human=contact.human+1e-6<contact.obstacle;result.point=cv(pulse0.lerp(pulse1,first));}
    const auto humanAtContact=ScintillaDefenseCollision::interpolate(bodies[humanBody].transform,humanAfter,std::min(1.0,first));
    result.humanPosition=cv(humanAtContact.getOrigin());result.humanForward=cv(humanAtContact.getBasis().getColumn(2));result.humanUp=cv(humanUp);
    bodies.clear();
}
Trace take(){Trace r=result;result=Trace();return r;}
bool applyImpact(AbstractKart*k)
{
    if(!eligibleHuman(k))return false;btVector3 up=-k->getBody()->getGravity();if(!finite(up)||up.length2()<1e-12)return false;up.normalize();
    const btVector3 v=k->getBody()->getLinearVelocity(),normal=up*v.dot(up),tangent=v-normal;
    if(!finite(v)||!finite(tangent)||tangent.length2()<1e-8)return false;
    k->getBody()->setLinearVelocity(normal+tangent*.85);const float minimum=k->getVehicle()->getMinSpeed();k->getVehicle()->resetMinSpeed();k->getVehicle()->setMinSpeed(minimum*.85f);return true;
}
bool applyBoost(AbstractKart*k,int fps)
{
    if(!eligibleHuman(k)||k->getSpeed()<=0||fps<60||fps>1000)return false;
    auto*vehicle=k->getVehicle();
    const btVector3 velocity=k->getBody()->getLinearVelocity(),normal=k->getNormal();
    if(!vehicle||!finite(velocity)||!finite(normal)||normal.length2()<1e-12||
       (velocity-normal*normal.dot(velocity)).fuzzyZero())return false;
    k->instantSpeedIncrease(MaxSpeed::MS_INCREASE_ZIPPER,2,2,0,fps,(fps+1)/2);
    if(k->getSpeedIncreaseTicksLeft(MaxSpeed::MS_INCREASE_ZIPPER)<=0)return false;
    // This commit follows Bullet. Apply the native one-shot speed floor now:
    // the next Kart::update resets it before the next physics step. A disabled
    // maximum avoids the native full-vector speed clamp; the floor itself is
    // already capped by MaxSpeed and preserves the normal/angular components.
    vehicle->adjustSpeed(vehicle->getMinSpeed(),-1);
    return true;
}
}
#endif
