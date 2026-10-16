// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "modes/immersive_relay/scintilla_defense_collision.hpp"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<std::endl;std::exit(1);}}while(false)
using namespace ScintillaDefenseCollision;
namespace {
btTransform at(float x,float y=0,float z=0){return btTransform(btQuaternion::getIdentity(),btVector3(x,y,z));}
void translatedTarget(){btSphereShape target(.08);Result s;castShape(&target,at(0),at(0),at(0),true,.002,&s,btVector3(-1,0,0),btVector3(1,0,0));CHECK(s.valid&&s.human>=.4&&s.human<.42);}
void movingTarget(){btSphereShape target(.08);Result s;castShape(&target,at(0),at(1),at(-1),true,.002,&s,btVector3(-1,0,0),btVector3(1,0,0));CHECK(s.valid&&s.human>=.45&&s.human<.47);}
void actualCompoundOffset(){btBoxShape child(btVector3(.2,.2,.2));btCompoundShape body;body.addChildShape(at(0,.8),&child);Result s;castShape(&body,at(0),at(0),at(0),true,.002,&s,btVector3(-1,.8,0),btVector3(1,.8,0));CHECK(s.valid&&s.human<1);Result miss;castShape(&body,at(0),at(0),at(0),true,.002,&miss,btVector3(-1,0,0),btVector3(1,0,0));CHECK(miss.human>1);}
void rotatingOffset(){btSphereShape child(.08);btCompoundShape body;body.addChildShape(at(1),&child);Result s;const btTransform end(btQuaternion(btVector3(0,0,1),.5235987756f),btVector3(0,0,0));castShape(&body,at(0),at(0),end,true,.002,&s,btVector3(.97,-.2,0),btVector3(.97,.7,0));CHECK(s.valid&&s.human<=1);}
void obstacleWinsAndOrderIndependent(){btSphereShape human(.08),wall(.08);for(int order=0;order<2;++order){Result r;for(int i=0;i<2;i++){bool h=(i==order);castShape(h?&human:&wall,at(0),at(h?.3:0),at(h?.3:0),h,.002,&r,btVector3(-1,0,0),btVector3(1,0,0));}CHECK(r.valid&&r.obstacle<r.human);}}
void separatedObstacleIsNotAnUncertainContact(){btSphereShape obstacle(.08);Result r;castShape(&obstacle,at(0),at(0,3),at(0,3),false,.002,&r,btVector3(-1,0,0),btVector3(1,0,0));CHECK(r.valid&&r.obstacle>1);}
struct SocketWorld {
    btDefaultCollisionConfiguration config;btCollisionDispatcher dispatcher;btDbvtBroadphase broadphase;btCollisionWorld world;
    btSphereShape sourceShape,obstacleShape;btCollisionObject source,obstacle;
    SocketWorld():dispatcher(&config),world(&dispatcher,&broadphase,&config),sourceShape(2),obstacleShape(.1){
        source.setCollisionShape(&sourceShape);source.setWorldTransform(at(0));world.addCollisionObject(&source);
        obstacle.setCollisionShape(&obstacleShape);
    }
    void place(const btVector3&p,int flags=btCollisionObject::CF_STATIC_OBJECT){obstacle.setWorldTransform(btTransform(btQuaternion::getIdentity(),p));obstacle.setCollisionFlags(flags);world.addCollisionObject(&obstacle);}
    ~SocketWorld(){if(obstacle.getBroadphaseHandle())world.removeCollisionObject(&obstacle);world.removeCollisionObject(&source);}
};
void socketRelocationRejectsIntermediateObstacle(){
    const btVector3 start(0,0,0),end(1,0,1.7320508075688772);
    for(int flags:{0,int(btCollisionObject::CF_STATIC_OBJECT)}){
        SocketWorld f;CHECK(socketPathClear(f.world,start,end,&f.source,nullptr));
        // Off the source->human center ray; neither endpoint overlaps the obstacle.
        f.place(end*.5,flags);CHECK(!socketPathClear(f.world,start,end,&f.source,nullptr));
    }
}
void socketRelocationRejectsEndpointOverlap(){
    const btVector3 start(0,0,0),end(1,0,1.7320508075688772);
    for(const auto&point:{start,end}){SocketWorld f;f.place(point);CHECK(!socketPathClear(f.world,start,end,&f.source,nullptr));}
}
void socketRelocationRespectsCollisionExclusions(){
    SocketWorld f;const btVector3 start(0,0,0),end(1,0,1.7320508075688772);
    f.place(end*.5,btCollisionObject::CF_NO_CONTACT_RESPONSE);
    CHECK(socketPathClear(f.world,start,end,&f.source,nullptr));
    f.world.removeCollisionObject(&f.obstacle);f.place(end*.5);
    CHECK(socketPathClear(f.world,start,end,&f.source,&f.obstacle));
}
void socketRelocationRejectsInvalidSegment(){
    SocketWorld f;CHECK(!socketPathClear(f.world,btVector3(0,0,0),btVector3(0,0,0),&f.source,nullptr));
    CHECK(!socketPathClear(f.world,btVector3(0,0,0),btVector3(NAN,0,0),&f.source,nullptr));
}

}
int main(int argc,char**argv){if(argc!=2)return 2;const std::string n=argv[1];if(n=="translatedTarget")translatedTarget();else if(n=="movingTarget")movingTarget();else if(n=="actualCompoundOffset")actualCompoundOffset();else if(n=="rotatingOffset")rotatingOffset();else if(n=="obstacleWinsAndOrderIndependent")obstacleWinsAndOrderIndependent();else if(n=="separatedObstacleIsNotAnUncertainContact")separatedObstacleIsNotAnUncertainContact();else if(n=="socketRelocationRejectsIntermediateObstacle")socketRelocationRejectsIntermediateObstacle();else if(n=="socketRelocationRejectsEndpointOverlap")socketRelocationRejectsEndpointOverlap();else if(n=="socketRelocationRespectsCollisionExclusions")socketRelocationRespectsCollisionExclusions();else if(n=="socketRelocationRejectsInvalidSegment")socketRelocationRejectsInvalidSegment();else return 2;return 0;}
